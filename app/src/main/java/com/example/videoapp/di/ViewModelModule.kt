package com.example.videoapp.di

import com.example.videoapp.ui.screen.MainViewModel
import com.example.videoapp.ui.screen.camera.CameraViewModel
import org.koin.android.viewmodel.dsl.viewModel
import org.koin.dsl.module

val viewModelModule = module {
    viewModel { MainViewModel() }
    viewModel { CameraViewModel() }
}