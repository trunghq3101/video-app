//
// Created by Trung Hoang on 2019-06-02.
//

#ifndef VIDEO_APP_LIVE_H
#define VIDEO_APP_LIVE_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_david_camerapush_ffmpeg_FFmpegHandler
 * Method:    init
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_init
        (JNIEnv *, jobject, jstring);

/*
 * Class:     com_david_camerapush_ffmpeg_FFmpegHandler
 * Method:    changeFilter
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_changeFilter
        (JNIEnv *, jobject, jint);

/*
 * Class:     com_david_camerapush_ffmpeg_FFmpegHandler
 * Method:    pushCameraData
 * Signature: ([BI[BI[BI)I
 */
JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_pushCameraData
        (JNIEnv *, jobject, jint, jbyteArray, jint, jbyteArray, jint, jbyteArray, jint);

/*
 * Class:     com_david_camerapush_ffmpeg_FFmpegHandler
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_close
        (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif //VIDEO_APP_LIVE_H

#ifndef _Included_com_example_videoapp_utils_FFmpegHandler_SingletonInstance
#define _Included_com_example_videoapp_utils_FFmpegHandler_SingletonInstance
#endif