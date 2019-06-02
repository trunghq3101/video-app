package com.example.videoapp.ui.screen.oldOldCamera

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.example.videoapp.R
import kotlinx.android.synthetic.main.old_old_camera_fragment.*
import org.koin.android.viewmodel.ext.android.viewModel

class OldOldCameraFragment : Fragment() {
    companion object {
        fun newInstance() = OldOldCameraFragment()
    }

    private val viewModel: OldOldCameraViewModel by viewModel()

    override fun onCreateView(
            inflater: LayoutInflater, container: ViewGroup?,
            savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.old_old_camera_fragment, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        floatingCapture.setOnClickListener {
            cameraView.captureVideo()
        }
    }

    override fun onResume() {
        super.onResume()
        cameraView.start()
    }

    override fun onPause() {
        cameraView.stop()
        super.onPause()
    }
}
