package com.example.videoapp.utils

class FFmpegHandler private constructor() {

    private object SingletonInstance {
        val INSTANCE = FFmpegHandler()
    }

    external fun init(outUrl: String): Int

    external fun pushCameraData(
        n: Int,
        buffer: ByteArray,
        ylen: Int,
        ubuffer: ByteArray,
        ulen: Int,
        vbuffer: ByteArray,
        vlen: Int
    ): Int

    external fun changeFilter(curFilter: Int): Int

    external fun close(): Int

    companion object {

        val instance: FFmpegHandler
            get() = SingletonInstance.INSTANCE

        init {
            System.loadLibrary("live")
            //        System.loadLibrary("avcodec-58");
            //        System.loadLibrary("avdevice-58");
            //        System.loadLibrary("avfilter-7");
            //        System.loadLibrary("avformat-58");
            //        System.loadLibrary("avutil-56");
            //        System.loadLibrary("postproc-55");
            //        System.loadLibrary("swresample-3");
            //        System.loadLibrary("swscale-5");

        }
    }
}