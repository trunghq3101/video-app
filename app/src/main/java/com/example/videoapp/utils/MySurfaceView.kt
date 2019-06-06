package com.example.videoapp.utils

/**
 * Created by Hoang Trung on 06/06/2019
 */

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View
import com.example.videoapp.R

class MySurfaceView: View {
    private lateinit var paint: Paint
    private lateinit var icon: Bitmap
    lateinit var myBitmap: Bitmap
    lateinit var myCanvas: Canvas

    constructor(context: Context?) : super(context) {
        initView()
    }
    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs) {
        initView()
    }
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(context, attrs, defStyleAttr) {
        initView()
    }
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int, defStyleRes: Int) : super(
        context,
        attrs,
        defStyleAttr,
        defStyleRes
    ) {
        initView()
    }

    override fun onDraw(canvas: Canvas?) {
        super.onDraw(canvas)
        myCanvas.drawBitmap(icon, 10F, 10F, paint)
        canvas?.drawBitmap(myBitmap, 0F, 0F, paint)
    }

    /*override fun surfaceCreated(holder: SurfaceHolder) {
        var canvas: Canvas? = null
        val myCanvas: Canvas
        try {
            canvas = holder.lockCanvas(null)
            myBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            myCanvas = Canvas(myBitmap)
            synchronized(holder) {
                //myCanvas.drawRGB(255, 255, 0)
                myCanvas.drawBitmap(icon, 10F, 10F, paint)
                canvas.drawBitmap(myBitmap, 0F, 0F, paint)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        } finally {
            if (canvas != null) {
                holder.unlockCanvasAndPost(canvas)
            }
        }
    }*/

    private fun initView() {
        paint = Paint()
        icon = BitmapFactory.decodeResource(resources, R.drawable.ic_action_info)
        myBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888)
        myCanvas = Canvas(myBitmap)
    }

    fun getNV21(inputWidth: Int, inputHeight: Int, scaled: Bitmap): ByteArray {

        val argb = IntArray(inputWidth * inputHeight)

        scaled.getPixels(argb, 0, inputWidth, 0, 0, inputWidth, inputHeight)

        val yuv = ByteArray(inputWidth * inputHeight * 3 / 2)
        encodeYUV420SP(yuv, argb, inputWidth, inputHeight)

        return yuv
    }

    private fun encodeYUV420SP(yuv420sp: ByteArray, argb: IntArray, width: Int, height: Int) {
        val frameSize = width * height

        var yIndex = 0
        var uvIndex = frameSize

        var a: Int
        var R: Int
        var G: Int
        var B: Int
        var Y: Int
        var U: Int
        var V: Int
        var index = 0
        for (j in 0 until height) {
            for (i in 0 until width) {

                a = argb[index] and -0x1000000 shr 24 // a is not used obviously
                R = argb[index] and 0xff0000 shr 16
                G = argb[index] and 0xff00 shr 8
                B = argb[index] and 0xff shr 0

                /*if (a == 0) {
                    R = 0
                    G = 255
                    B = 0
                }*/

                // well known RGB to YUV algorithm
                Y = (66 * R + 129 * G + 25 * B + 128 shr 8) + 16
                U = (-38 * R - 74 * G + 112 * B + 128 shr 8) + 128
                V = (112 * R - 94 * G - 18 * B + 128 shr 8) + 128

                // NV21 has a plane of Y and interleaved planes of VU each sampled by a factor of 2
                //    meaning for every 4 Y pixels there are 1 V and 1 U.  Note the sampling is every other
                //    pixel AND every other scanline.
                yuv420sp[yIndex++] = (if (Y < 0) 0 else if (Y > 255) 255 else Y).toByte()
                if (j % 2 == 0 && index % 2 == 0) {
                    yuv420sp[uvIndex++] = (if (V < 0) 0 else if (V > 255) 255 else V).toByte()
                    yuv420sp[uvIndex++] = (if (U < 0) 0 else if (U > 255) 255 else U).toByte()
                }

                index++
            }
        }
    }
}