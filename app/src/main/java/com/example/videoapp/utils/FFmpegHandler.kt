package com.example.videoapp.utils

import java.nio.ByteBuffer

class FFmpegHandler private constructor() {

    private class SingletonInstance {
        companion object {
            val INSTANCE = FFmpegHandler()
        }
    }

    external fun init(outUrl: String): Int

    external fun encodeFrame(
        n: Int,
        bufferY: ByteBuffer,
        bufferU: ByteBuffer,
        bufferV: ByteBuffer
    ): Int

    /*external fun pushCameraData(
        n: Int,
        buffer: ByteArray,
        ylen: Int,
        ubuffer: ByteArray,
        ulen: Int,
        vbuffer: ByteArray,
        vlen: Int
    ): Int*/

    external fun changeFilter(curFilter: Int): Int

    external fun close(): Int

    companion object {

        val instance: FFmpegHandler
            get() = SingletonInstance.INSTANCE

        init {
//            System.loadLibrary("avcodec")
//            System.loadLibrary("avfilter")
//            System.loadLibrary("avformat")
//            System.loadLibrary("avutil")
//            System.loadLibrary("swscale")
            System.loadLibrary("live")
        }
    }
}