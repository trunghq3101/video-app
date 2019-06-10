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
        w: Int,
        h: Int,
        bufferY: ByteBuffer,
        pixY: Int,
        rowY: Int,
        bufferU: ByteBuffer,
        pixU: Int,
        rowU: Int,
        bufferV: ByteBuffer,
        pixV: Int,
        rowV: Int
    ): Int

    external fun encodeARGBFrame(
        n: Int,
        w: Int,
        h: Int,
        bytes: ByteBuffer?
    ): Int

    external fun changeFilter(curFilter: Int): Int

    external fun close(): Int

    companion object {

        val instance: FFmpegHandler
            get() = SingletonInstance.INSTANCE

        init {
            System.loadLibrary("live")
        }
    }
}