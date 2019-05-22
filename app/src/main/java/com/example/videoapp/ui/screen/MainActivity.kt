package com.example.videoapp.ui.screen

import android.os.Bundle
import com.example.videoapp.R
import com.example.videoapp.ui.base.BaseActivity
import com.example.videoapp.ui.screen.camera.CameraFragment
import org.koin.android.viewmodel.ext.android.viewModel

class MainActivity : BaseActivity<com.example.videoapp.databinding.ActivityMainBinding, MainViewModel>() {
    override val viewModel: MainViewModel by viewModel()
    override val layoutId: Int = R.layout.activity_main

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportFragmentManager.beginTransaction()
            .add(R.id.constraintMain, CameraFragment.newInstance())
            .commit()
    }
}
