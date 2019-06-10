package com.example.videoapp.ui.screen.camera

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.res.Configuration
import android.graphics.*
import android.hardware.camera2.*
import android.media.ImageReader
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.util.Size
import android.util.SparseIntArray
import android.view.*
import android.view.Surface.*
import androidx.fragment.app.Fragment
import com.example.videoapp.R.string
import com.example.videoapp.data.constants.Constants.REQUEST_CAMERA_PERMISSION
import com.example.videoapp.utils.FFmpegHandler
import kotlinx.android.synthetic.main.camera_fragment.*
import org.koin.android.viewmodel.ext.android.viewModel
import pub.devrel.easypermissions.EasyPermissions
import java.nio.ByteBuffer
import java.util.*
import java.util.concurrent.Semaphore
import kotlin.collections.ArrayList


class CameraFragment : Fragment(), EasyPermissions.PermissionCallbacks {
    companion object {
        const val BACKGROUND_THREAD = "CameraBackground"
        const val MAX_PREVIEW_WIDTH = 1920
        const val MAX_PREVIEW_HEIGHT = 1080
        fun newInstance() = CameraFragment()
        val ORIENTATIONS = SparseIntArray().apply {
            append(ROTATION_0, 90)
            append(ROTATION_90, 0)
            append(ROTATION_180, 270)
            append(ROTATION_270, 180)
        }

        private fun chooseOptimalSize(
            choices: List<Size>,
            textureViewWidth: Int,
            textureViewHeight: Int,
            maxWidth: Int,
            maxHeight: Int,
            aspectRatio: Size
        ): Size {
            val bigEnough = ArrayList<Size>()
            val notBigEnough = ArrayList<Size>()
            val w = aspectRatio.width
            val h = aspectRatio.height
            for (option in choices) {
                if (option.width <= maxWidth && option.height <= maxHeight && option.height == option.width * h / w) {
                    if (option.width >= textureViewWidth && option.height >= textureViewHeight) {
                        bigEnough.add(option)
                    } else {
                        notBigEnough.add(option)
                    }
                }
            }
            return when {
                bigEnough.size > 0 -> Collections.min(bigEnough, CompareSizesByArea())
                notBigEnough.size > 0 -> Collections.max(notBigEnough, CompareSizesByArea())
                else -> choices[0]
            }
        }
    }


    private val viewModel: CameraViewModel by viewModel()
    /**
     * A [Semaphore] to prevent the app from exiting before closing the camera.
     */
    private val cameraOpenCloseLock = Semaphore(1)
    private var cameraDevice: CameraDevice? = null
    private var cameraCaptureSession: CameraCaptureSession? = null
    private var captureRequestBuilder: CaptureRequest.Builder? = null
    private var captureRequest: CaptureRequest? = null
    private var backgroundThread: HandlerThread? = null
    private var backgroundHandler: Handler? = null
    private var previewSize: Size? = null
    private var cameraId: String? = null
    private val surfaceTextureListener = object : TextureView.SurfaceTextureListener {
        override fun onSurfaceTextureAvailable(texture: SurfaceTexture, width: Int, height: Int) {
            openCamera(width, height)
            mySurfaceView.setBitmap(width, height)
        }

        override fun onSurfaceTextureSizeChanged(texture: SurfaceTexture, width: Int, height: Int) {
            configureTransform(width, height)
        }

        override fun onSurfaceTextureDestroyed(texture: SurfaceTexture) = false

        override fun onSurfaceTextureUpdated(texture: SurfaceTexture) = Unit

    }
    private val stateCallback = object : CameraDevice.StateCallback() {
        override fun onOpened(cameraDevice: CameraDevice) {
            cameraOpenCloseLock.release()
            this@CameraFragment.cameraDevice = cameraDevice
            createCameraPreviewSession()
        }

        override fun onDisconnected(cameraDevice: CameraDevice) {
            cameraOpenCloseLock.release()
            cameraDevice.close()
            this@CameraFragment.cameraDevice = null
        }

        override fun onError(cameraDevice: CameraDevice, error: Int) {
            onDisconnected(cameraDevice)
            activity?.finish()
        }
    }
    private var imageReader: ImageReader? = null
    private val imageAvailableListener = ImageReader.OnImageAvailableListener { reader ->
        handler?.post {
            val image = reader?.acquireLatestImage()
            image?.let {
                FFmpegHandler.instance.encodeFrame(
                    0,
                    it.width,
                    it.height,
                    it.planes[0].buffer,
                    it.planes[0].pixelStride,
                    it.planes[0].rowStride,
                    it.planes[1].buffer,
                    it.planes[1].pixelStride,
                    it.planes[1].rowStride,
                    it.planes[2].buffer,
                    it.planes[2].pixelStride,
                    it.planes[2].rowStride
                )
                if (drawChange != 0) {
                    mySurfaceView.myBitmap?.let { bitmap ->
                        val buffer = ByteBuffer.allocateDirect(bitmap.rowBytes * bitmap.height)
                        bitmap.copyPixelsToBuffer(buffer)
                        drawChange = FFmpegHandler.instance.encodeARGBFrame(1, bitmap.width, bitmap.height, buffer)
                    }
                } else {
                    FFmpegHandler.instance.encodeARGBFrame(1, 0, 0, null)
                }
                it.close()
            }
        }
    }
    private var handlerThread: HandlerThread? = null
    private var handler: Handler? = null
    private val captureCallback = object : CameraCaptureSession.CaptureCallback() {
        override fun onCaptureCompleted(
            session: CameraCaptureSession,
            request: CaptureRequest,
            result: TotalCaptureResult
        ) {
        }
    }
    private var sensorOrientation: Int? = null
    private var deviceOrientation: Int? = null
    private var characteristics: CameraCharacteristics? = null
    private var drawChange: Int? = 1

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(com.example.videoapp.R.layout.camera_fragment, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        EasyPermissions.onRequestPermissionsResult(requestCode, permissions, grantResults, this)
    }

    override fun onResume() {
        super.onResume()

        // For real debug device
        //FFmpegHandler.instance.init("rtmp://192.168.1.241/live")

        // For emulator
        FFmpegHandler.instance.init("rtmp://10.0.2.2/live")

        startBackgroundThread()
        if (textureCamera.isAvailable) {
            openCamera(textureCamera.width, textureCamera.height)
        } else {
            textureCamera.surfaceTextureListener = surfaceTextureListener
        }
    }

    override fun onPause() {
        closeCamera()
        closeBackgroundThread()
        super.onPause()
    }

    override fun onPermissionsDenied(requestCode: Int, perms: List<String>) {
        activity?.onBackPressed()
    }

    override fun onPermissionsGranted(requestCode: Int, perms: MutableList<String>) {
        //openCamera()
    }

    @SuppressLint("MissingPermission")
    private fun openCamera(width: Int, height: Int) {
        if (!EasyPermissions.hasPermissions(
                requireContext(),
                Manifest.permission.CAMERA,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE
            )
        ) {
            EasyPermissions.requestPermissions(
                this,
                getString(string.camera_permission),
                REQUEST_CAMERA_PERMISSION,
                Manifest.permission.CAMERA,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE
            )
            return
        }
        setupCamera(width, height)
        configureTransform(width, height)
        val manager = activity?.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        try {
            manager.openCamera(cameraId, stateCallback, backgroundHandler)
        } catch (e: CameraAccessException) {
            e.printStackTrace()
        }
    }

    private fun setupCamera(width: Int, height: Int) {
        val manager = activity?.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        for (cameraId in manager.cameraIdList) {
            characteristics = manager.getCameraCharacteristics(cameraId)
            // We don't use a front facing camera in this sample.
            val facing = characteristics?.get(CameraCharacteristics.LENS_FACING)
            if (facing != null && facing != CameraCharacteristics.LENS_FACING_BACK) {
                continue
            }
            val map = characteristics?.get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP
            ) ?: continue
            val largest = Collections.max(
                map.getOutputSizes(ImageFormat.YUV_420_888).toList(),
                CompareSizesByArea()
            )
            imageReader = ImageReader.newInstance(largest.width, largest.height, ImageFormat.YUV_420_888, 2).apply {
                setOnImageAvailableListener(imageAvailableListener, backgroundHandler)
            }
            sensorOrientation = characteristics?.get(CameraCharacteristics.SENSOR_ORIENTATION)
            val displayRotation = requireActivity().windowManager.defaultDisplay.rotation
            var swappedDimensions = false
            when (displayRotation) {
                ROTATION_0, ROTATION_180 -> if (sensorOrientation == 90 || sensorOrientation == 270) {
                    swappedDimensions = true
                }
                ROTATION_90, ROTATION_270 -> if (sensorOrientation == 0 || sensorOrientation == 180) {
                    swappedDimensions = true
                }
            }
            val displaySize = Point()
            requireActivity().windowManager.defaultDisplay.getSize(displaySize)
            var rotatedPreviewWidth = width
            var rotatedPreviewHeight = height
            var maxPreviewWidth = displaySize.x
            var maxPreviewHeight = displaySize.y

            if (swappedDimensions) {
                rotatedPreviewWidth = height
                rotatedPreviewHeight = width
                maxPreviewWidth = displaySize.y
                maxPreviewHeight = displaySize.x
            }

            if (maxPreviewWidth > MAX_PREVIEW_WIDTH) {
                maxPreviewWidth = MAX_PREVIEW_WIDTH
            }

            if (maxPreviewHeight > MAX_PREVIEW_HEIGHT) {
                maxPreviewHeight = MAX_PREVIEW_HEIGHT
            }
            previewSize = chooseOptimalSize(
                map.getOutputSizes(SurfaceTexture::class.java).toList(),
                rotatedPreviewWidth,
                rotatedPreviewHeight,
                maxPreviewWidth,
                maxPreviewHeight,
                largest
            )
            deviceOrientation = resources.configuration.orientation
            previewSize?.let {
                if (deviceOrientation == Configuration.ORIENTATION_LANDSCAPE) {
                    textureCamera.setAspectRatio(
                        it.width, it.height
                    )
                } else {
                    textureCamera.setAspectRatio(
                        it.height, it.width
                    )
                }
            }
            this.cameraId = cameraId
            return
        }

    }

    private fun startBackgroundThread() {
        backgroundThread = HandlerThread(BACKGROUND_THREAD).also { it.start() }
        backgroundHandler = Handler(backgroundThread?.looper)
        handlerThread = HandlerThread("Push Frame").also { it.start() }
        handler = Handler(handlerThread?.looper)
    }

    private fun createCameraPreviewSession() {
        try {
            val surfaceTexture = textureCamera.surfaceTexture
            previewSize?.let {
                surfaceTexture.setDefaultBufferSize(it.width, it.height)
            }
            val previewSurface = Surface(surfaceTexture)
            val imageSurface = imageReader?.surface
            captureRequestBuilder =
                cameraDevice?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)?.apply {
                    addTarget(previewSurface)
                    addTarget(imageSurface)
                }
            cameraDevice?.createCaptureSession(
                arrayListOf(previewSurface, imageSurface),
                object : CameraCaptureSession.StateCallback() {
                    override fun onConfigureFailed(session: CameraCaptureSession) {
                    }

                    override fun onConfigured(session: CameraCaptureSession) {
                        try {
                            captureRequest = captureRequestBuilder?.build()
                            cameraCaptureSession = session.apply {
                                setRepeatingRequest(
                                    captureRequest,
                                    captureCallback,
                                    backgroundHandler
                                )
                            }
                        } catch (e: CameraAccessException) {
                            e.printStackTrace()
                        }
                    }

                },
                backgroundHandler
            )
        } catch (e: CameraAccessException) {
            e.printStackTrace()
        }
    }

    /**
     * Retrieves the image orientation from the specified screen rotation.
     * Used to calculate bitmap image rotation
     */
    private fun getImageOrientation(deviceRotation: Int, characteristics: CameraCharacteristics): Int {
        if (deviceRotation == OrientationEventListener.ORIENTATION_UNKNOWN) {
            return 0
        }
        val sensorOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
        // Sensor orientation is 90 for most devices, or 270 for some devices (eg. Nexus 5X)
        // We have to take that into account and rotate JPEG properly.
        // For devices with orientation of 90, we simply return our mapping from ORIENTATIONS.
        // For devices with orientation of 270, we need to rotate the JPEG 180 degrees.
        return (ORIENTATIONS.get(deviceRotation) + sensorOrientation + 270) % 360
    }

    private fun configureTransform(viewWidth: Int, viewHeight: Int) {
        if (null == textureCamera || null == previewSize || null == activity) {
            return
        }
        val rotation = requireActivity().windowManager.defaultDisplay.rotation
        val matrix = Matrix()
        val viewRect = RectF(0f, 0f, viewWidth.toFloat(), viewHeight.toFloat())
        val bufferRect = RectF(0f, 0f, previewSize!!.height.toFloat(), previewSize!!.width.toFloat())
        val centerX = viewRect.centerX()
        val centerY = viewRect.centerY()
        if (ROTATION_90 == rotation || ROTATION_270 == rotation) {
            bufferRect.offset(centerX - bufferRect.centerX(), centerY - bufferRect.centerY())
            matrix.setRectToRect(viewRect, bufferRect, Matrix.ScaleToFit.FILL)
            val scale = Math.max(
                viewHeight.toFloat() / previewSize!!.height,
                viewWidth.toFloat() / previewSize!!.width
            )
            matrix.postScale(scale, scale, centerX, centerY)
            matrix.postRotate((90 * (rotation - 2)).toFloat(), centerX, centerY)
        } else if (ROTATION_180 == rotation) {
            matrix.postRotate(180F, centerX, centerY)
        }
        textureCamera.setTransform(matrix)
    }

    private fun closeCamera() {
        cameraOpenCloseLock.acquire()
        cameraCaptureSession?.close()
        cameraCaptureSession = null
        cameraDevice?.close()
        cameraDevice = null
        imageReader?.close()
        imageReader = null
        cameraOpenCloseLock.release()
    }

    private fun closeBackgroundThread() {
        backgroundHandler?.let {
            backgroundThread?.quitSafely()
            backgroundThread = null
            backgroundHandler = null
        }
        handler?.let {
            handlerThread?.quitSafely()
            handlerThread?.join()
            FFmpegHandler.instance.close()
            handlerThread = null
            handler = null
        }
    }

    class CompareSizesByArea : Comparator<Size> {

        override fun compare(lhs: Size, rhs: Size): Int {
            // We cast here to ensure the multiplications won't overflow
            return lhs.width * lhs.height - rhs.width * rhs.height
        }

    }
}
