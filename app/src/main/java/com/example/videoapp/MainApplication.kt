package com.example.videoapp

import android.app.Application
import com.example.videoapp.di.appModule
import com.example.videoapp.di.viewModelModule
import org.koin.core.context.startKoin

class MainApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        startKoin {
            modules(appModule, viewModelModule)
        }
    }
}