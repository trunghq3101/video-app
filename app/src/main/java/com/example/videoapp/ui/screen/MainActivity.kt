package com.example.videoapp.ui.screen

import android.os.Bundle
import android.util.Log
import android.widget.TextView
import com.arthenica.mobileffmpeg.FFmpeg
import com.example.videoapp.R
import com.example.videoapp.ui.base.BaseActivity
import com.example.videoapp.ui.screen.camera.CameraFragment
import kotlinx.android.synthetic.main.activity_main.*
import org.koin.android.viewmodel.ext.android.viewModel

class MainActivity : BaseActivity<com.example.videoapp.databinding.ActivityMainBinding, MainViewModel>() {
    companion object {
        const val TAG = "MainActivity"
    }
    override val viewModel: MainViewModel by viewModel()
    override val layoutId: Int = com.example.videoapp.R.layout.activity_main

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buttonRun.setOnClickListener {
            val cmd = editCommand.text.toString()
            val command: Array<String> = cmd.split(" ").toTypedArray()
            if (command.isNotEmpty()) {
                execFFmpegBinary(command)
            }
        }
        buttonCamera.setOnClickListener {
            supportFragmentManager.beginTransaction()
                .add(R.id.constraintMain, CameraFragment.newInstance())
                .addToBackStack(null)
                .commit()
        }
    }

    private fun execFFmpegBinary(command: Array<String>) {
        val rsCode = FFmpeg.execute(command)
        Log.d("MainActivity", rsCode.toString())
        val rs = FFmpeg.getLastCommandOutput()
        if (rsCode == FFmpeg.RETURN_CODE_SUCCESS) {
            addTextViewToLayout(rs)
        }
    }

    private fun addTextViewToLayout(text: String) {
        val textView = TextView(this)
        textView.text = text
        linearOutput.addView(textView)
    }
}
