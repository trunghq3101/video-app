cmake_minimum_required(VERSION 3.4.1)

set(ffmpeg_DIR
        ${CMAKE_SOURCE_DIR})

find_library( # Sets the name of the path variable.
        log-lib
        log)

add_library(avutil
        SHARED
        IMPORTED)
set_target_properties(avutil
        PROPERTIES IMPORTED_LOCATION
        ${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/lib/libavutil.so)

add_library(avformat
        SHARED
        IMPORTED)
set_target_properties(avformat
        PROPERTIES IMPORTED_LOCATION
         ${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/lib/libavformat.so)

add_library(avcodec
        SHARED
        IMPORTED)

set_target_properties(avcodec
        PROPERTIES IMPORTED_LOCATION
         ${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/lib/libavcodec.so)

add_library(avfilter
        SHARED
        IMPORTED)

set_target_properties(avfilter
        PROPERTIES IMPORTED_LOCATION
        ${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/lib/libavfilter.so)

add_library(swscale
        SHARED
        IMPORTED)

set_target_properties(swscale
        PROPERTIES IMPORTED_LOCATION
        ${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/lib/libswscale.so)

add_library(live
        SHARED
        live.c)

include_directories(${ffmpeg_DIR}/${ANDROID_ABI}/ffmpeg/include)

target_link_libraries(live
        avutil avformat avcodec avfilter swscale
        ${log-lib} ${android-lib})