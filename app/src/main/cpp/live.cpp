//
// Created by Trung Hoang on 2019-06-02.
//

#include "live.h"
#include <jni.h>
#include<android/log.h>

#define LOG_TAG    "ffmpeg-c"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

AVFormatContext *ofmt_ctx;
AVStream *video_st;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVPacket enc_pkt;
AVFrame *pFrameYUV;
AVFilterContext *buffersink_ctx;
AVFilterContext **buffersrc_ctx;
AVFilterGraph *filter_graph;
AVFilter *buffersrc;
AVFilter *buffersink;
AVFrame* picref;
int filter_change = 1;
int videoInputs = 2;
const char *filter_descr = "[0:v][1:v]overlay=0:0[v]";
const char *filter_logo = "movie=logo.jpeg[wm];[in][wm]overlay=5:5[out]";
const char *filter_mirror = "crop=iw/2:ih:0:0,split[left][tmp];[tmp]hflip[right];[left]pad=iw*2[a];[a][right]overlay=w";
const char *filter_negate = "negate[out]";
const char *filter_edge = "edgedetect[out]";
const char *filter_split4 = "scale=iw/2:ih/2[in_tmp];[in_tmp]split=4[in_1][in_2][in_3][in_4];[in_1]pad=iw*2:ih*2[a];[a][in_2]overlay=w[b];[b][in_3]overlay=0:h[d];[d][in_4]overlay=w:h[out]";
const char *filter_vintage = "curves=vintage";
typedef enum{
    FILTER_NULL = 48,
    FILTER_MIRROR,
    FILTER_NEGATE,
    FILTER_EDGE,
    FILTER_SPLIT4,
    FILTER_VINTAGE
}FILTERS;
int count = 0;
int yuv_width;
int yuv_height;
int y_length;
int uv_length;
int width = 640;
int height = 480;
int fps = 15;

/*
 * Class:     com_david_camerapush_ffmpeg_FFmpegHandler
 * Method:    init  初始化ffmpeg相关,准备推送
 * Signature: (Ljava/lang/String;)I rmtp服务地址
 */
JNIEXPORT jint JNICALL Java_com_david_camerapush_ffmpeg_FFmpegHandler_changeFilter(
        JNIEnv *jniEnv, jobject instance, jint curFilter) {
        switch (48 + curFilter % 6) {
            case FILTER_NULL:
            {
                printf("\nnow using null filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = "null";
                break;
            }
            case FILTER_MIRROR:
            {
                printf("\nnow using mirror filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = filter_mirror;
                break;
            }
            case FILTER_NEGATE:
            {
                printf("\nnow using negate filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = filter_negate;
                break;
            }
            case FILTER_EDGE:
            {
                printf("\nnow using edge filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = filter_edge;
                break;
            }
            case FILTER_SPLIT4:
            {
                printf("\nnow using split4 filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = filter_split4;
                break;
            }
            case FILTER_VINTAGE:
            {
                printf("\nnow using vintage filter\nPress other numbers for other filters:");
                filter_change = 1;
                filter_descr = filter_vintage;
                break;
            }
            default:
            {
                break;
            }
        }
        return 0;
}

static int apply_filters(/*AVFormatContext *ifmt_ctx*/)
{
    char args[512];
    int ret;
    AVFilterInOut **outputs;
    outputs = av_calloc((size_t) (videoInputs + 1), sizeof(AVFilterInOut*));
    if (!outputs)
    {
        printf("Cannot alloc int\n");
        return -1;
    }
    /*if (!outputs) {
        printf("Cannot alloc output\n");
        return -1;
    }*/
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (!inputs)
    {
        printf("Cannot alloc out\n");
        return -1;
    }

    if (filter_graph)
        avfilter_graph_free(&filter_graph);
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph)
    {
        printf("Cannot create filter graph\n");
        return -1;
    }

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             pCodecCtx->width,
             pCodecCtx->height,
             pCodecCtx->pix_fmt,
             pCodecCtx->time_base.num,
             pCodecCtx->time_base.den,
             1,
             1);
    /* buffer video source: the decoded frames from the decoder will be inserted here. *//*
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ifmt_ctx->streams[0]->codec->width,
             ifmt_ctx->streams[0]->codec->height,
             ifmt_ctx->streams[0]->codec->pix_fmt,
             ifmt_ctx->streams[0]->time_base.num,
             ifmt_ctx->streams[0]->time_base.den,
             ifmt_ctx->streams[0]->codec->sample_aspect_ratio.num,
             ifmt_ctx->streams[0]->codec->sample_aspect_ratio.den);*/

    for (int i = 0; i < videoInputs; i ++) {
        char name[6];
        if (videoInputs > 1) {
            snprintf(name, 6, "%d:v", i);
        } else {
            snprintf(name, 6, "in");
        }
        outputs[i] = avfilter_inout_alloc();
        if (!outputs[i]) {
            printf("Cannot alloc input\n");
            return -1;
        }
        ret = avfilter_graph_create_filter(&buffersrc_ctx[i], buffersrc, name,
                                           args, NULL, filter_graph);
        if (ret < 0) {
            printf("Cannot create buffer source\n");
            return ret;
        }
        outputs[i]->name = av_strdup(name);
        outputs[i]->filter_ctx = buffersrc_ctx[i];
        outputs[i]->pad_idx = 0;
        outputs[i]->next = NULL;
        if (i > 0) {
            outputs[i - 1]->next = outputs[i];
        }
    }

    char *name = videoInputs > 1 ? "v" : "out";
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, name,
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        printf("Cannot create buffer sink\n");
        return ret;
    }
    inputs->name = av_strdup(name);
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, outputs, NULL)) < 0)
        return ret;
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return ret;
    avfilter_inout_free(&inputs);
    avfilter_inout_free(outputs);

    return 0;
}

JNIEXPORT jint JNICALL Java_com_david_camerapush_ffmpeg_FFmpegHandler_init
        (JNIEnv *jniEnv, jobject instance, jstring url) {

    buffersrc = (AVFilter *) avfilter_get_by_name("buffer");
    buffersink = (AVFilter *) avfilter_get_by_name("buffersink");
    buffersrc_ctx = av_calloc((size_t) (videoInputs + 1), sizeof(AVFilterContext));

    const char *out_url = (*jniEnv)->GetStringUTFChars(jniEnv, url, 0);

    //计算yuv数据的长度
    yuv_width = width;
    yuv_height = height;
    y_length = width * height;
    uv_length = width * height / 4;
    //output initialize
    int ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_url);
    if (ret < 0) {
        LOGE("avformat_alloc_output_context2 error");
    }


    //output encoder initialize
    pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);

    if (!pCodec) {
        LOGE("Can not find encoder!\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    //编码器的ID号，这里为264编码器，可以根据video_st里的codecID 参数赋值
    pCodecCtx->codec_id = pCodec->id;

    //像素的格式，也就是说采用什么样的色彩空间来表明一个像素点
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    //编码器编码的数据类型
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    //编码目标的视频帧大小，以像素为单位
    pCodecCtx->width = width;
    pCodecCtx->height = height;
    pCodecCtx->framerate = (AVRational) {15, 1};

    //帧率的基本单位，我们用分数来表示，
    pCodecCtx->time_base = (AVRational) {1, 15};
    //目标的码率，即采样的码率；显然，采样码率越大，视频大小越大
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->gop_size = 50;
    /* Some formats want stream headers to be separate. */
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //H264 codec param
//    pCodecCtx->me_range = 16;
    //pCodecCtx->max_qdiff = 4;
    pCodecCtx->qcompress = 0.6;
    //最大和最小量化系数
    pCodecCtx->qmin = 10;
    pCodecCtx->qmax = 51;
    //Optional Param
    //两个非B帧之间允许出现多少个B帧数
    //设置0表示不使用B帧，b 帧越多，图片越小
    pCodecCtx->max_b_frames = 0;
    AVDictionary *param = 0;
    //H.264
    if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "superfast", 0); //x264编码速度的选项
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if (avcodec_open2(pCodecCtx, pCodec, &param) < 0) {
        LOGE("Failed to open encoder!\n");
        return -1;
    }

    //Add a new stream to output,should be called by the user before avformat_write_header() for muxing
    video_st = avformat_new_stream(ofmt_ctx, pCodec);
    if (video_st == NULL) {
        return -1;
    }
    video_st->time_base = (AVRational) {25, 1};
    video_st->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(video_st->codecpar, pCodecCtx);

    int err = avio_open(&ofmt_ctx->pb, out_url, AVIO_FLAG_READ_WRITE);
    if (err < 0) {
        LOGE("Failed to open output：%s", av_err2str(err));
        return -1;
    }

    //Write File Header
    avformat_write_header(ofmt_ctx, NULL);
    av_init_packet(&enc_pkt);

    return 0;
}


JNIEXPORT jint JNICALL Java_com_david_camerapush_ffmpeg_FFmpegHandler_pushCameraData
        (JNIEnv *jniEnv, jobject instance, jint n, jbyteArray yArray, jint yLen, jbyteArray uArray, jint uLen,
         jbyteArray vArray, jint vLen) {
    jbyte *yin = (*jniEnv)->GetByteArrayElements(jniEnv, yArray, NULL);
    jbyte *uin = (*jniEnv)->GetByteArrayElements(jniEnv, uArray, NULL);
    jbyte *vin = (*jniEnv)->GetByteArrayElements(jniEnv, vArray, NULL);

    int ret = 0;

    pFrameYUV = av_frame_alloc();
    int picture_size = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width,
                                                pCodecCtx->height, 1);
    uint8_t *buffers = (uint8_t *) av_malloc(picture_size);


    //将buffers的地址赋给AVFrame中的图像数据，根据像素格式判断有几个数据指针
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, buffers, pCodecCtx->pix_fmt,
                         pCodecCtx->width, pCodecCtx->height, 1);

    memcpy(pFrameYUV->data[0], yin, (size_t) yLen); //Y
    memcpy(pFrameYUV->data[1], uin, (size_t) uLen); //U
    memcpy(pFrameYUV->data[2], vin, (size_t) vLen); //V
    pFrameYUV->pts = count;
    pFrameYUV->format = AV_PIX_FMT_YUV420P;
    pFrameYUV->width = yuv_width;
    pFrameYUV->height = yuv_height;

    // FILTER
    struct SwsContext *img_convert_ctx;
    if (filter_change) {
        apply_filters();
    }
    filter_change = 0;
    if (av_buffersrc_add_frame(buffersrc_ctx[n], pFrameYUV) < 0) {
        printf("Error while feeding the filtergraph\n");
        return -1;
    }
    picref = av_frame_alloc();
    ret = av_buffersink_get_frame_flags(buffersink_ctx, picref, 0);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return -1;
    if (ret < 0)
        return ret;
    if (picref) {
        img_convert_ctx = sws_getContext(picref->width, picref->height, (enum AVPixelFormat) picref->format,
                                         pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL,
                                         NULL, NULL);
        sws_scale(img_convert_ctx, (const uint8_t *const *) picref->data, picref->linesize, 0, pCodecCtx->height,
                  pFrameYUV->data, pFrameYUV->linesize);
        sws_freeContext(img_convert_ctx);
        pFrameYUV->width = picref->width;
        pFrameYUV->height = picref->height;
        pFrameYUV->format = AV_PIX_FMT_YUV420P;
    }
    av_frame_unref(picref);

    //例如对于H.264来说。1个AVPacket的data通常对应一个NAL
    //初始化AVPacket
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
//    __android_log_print(ANDROID_LOG_WARN, "eric", "编码前时间:%lld",
//                        (long long) ((av_gettime() - startTime) / 1000));
    //开始编码YUV数据
    ret = avcodec_send_frame(pCodecCtx, pFrameYUV);
    if (ret != 0) {
        LOGE("avcodec_send_frame error");
        return -1;
    }
    //获取编码后的数据
    ret = avcodec_receive_packet(pCodecCtx, &enc_pkt);

    av_frame_free(&pFrameYUV);
    if (ret != 0 || enc_pkt.size <= 0) {
        LOGE("avcodec_receive_packet error %s", av_err2str(ret));
        return -2;
    }
    enc_pkt.stream_index = video_st->index;
    enc_pkt.pts = count * (video_st->time_base.den) / ((video_st->time_base.num) * fps);
    enc_pkt.dts = enc_pkt.pts;
    enc_pkt.duration = (video_st->time_base.den) / ((video_st->time_base.num) * fps);
    enc_pkt.pos = -1;


    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    if (ret != 0) {
        LOGE("av_interleaved_write_frame failed");
    }
    count++;
    av_packet_unref(&enc_pkt);
    av_frame_free(&pFrameYUV);
    av_free(buffers);
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, yArray, yin, 0);
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, uArray, uin, 0);
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, vArray, vin, 0);

    return 0;
}


/**
 * 释放资源
 */
JNIEXPORT jint JNICALL Java_com_david_camerapush_ffmpeg_FFmpegHandler_close
        (JNIEnv *jniEnv, jobject instance) {
    if (video_st)
        avcodec_close(pCodecCtx);
    if (ofmt_ctx) {
        avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
        ofmt_ctx = NULL;
    }
    if (filter_graph) {
        avfilter_graph_free(&filter_graph);
        buffersink_ctx = NULL;
        buffersrc_ctx = NULL;
    }
    filter_change = 1;
    return 0;
}

