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
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

AVFormatContext *ofmt_ctx;
AVStream *video_st;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVPacket enc_pkt;
AVFilterContext *buffersink_ctx;
AVFilterContext **buffersrc_ctx;
AVFilterGraph *filter_graph;
AVFilter *buffersrc;
AVFilter *buffersink;
int filter_change = 1;
int videoInputs = 2;
//const char *filter_descr = "transpose=1";
const char *filter_descr = "[0:v]transpose=1[tout];[1:v]colorkey=0x000000[ckout];[tout][ckout]overlay=0:0[v]";
/*const char *filter_logo = "movie=logo.jpeg[wm];[in][wm]overlay=5:5[out]";
const char *filter_mirror = "crop=iw/2:ih:0:0,split[left][tmp];[tmp]hflip[right];[left]pad=iw*2[a];[a][right]overlay=w";
const char *filter_negate = "negate[out]";
const char *filter_edge = "edgedetect[out]";
const char *filter_split4 = "scale=iw/2:ih/2[in_tmp];[in_tmp]split=4[in_1][in_2][in_3][in_4];[in_1]pad=iw*2:ih*2[a];[a][in_2]overlay=w[b];[b][in_3]overlay=0:h[d];[d][in_4]overlay=w:h[out]";
const char *filter_vintage = "curves=vintage";*/
const enum AVPixelFormat OUTPUT_FMT = AV_PIX_FMT_YUV420P;
typedef enum {
    FILTER_NULL = 48,
    /*FILTER_MIRROR,
    FILTER_NEGATE,
    FILTER_EDGE,
    FILTER_SPLIT4,
    FILTER_VINTAGE*/
} FILTERS;
int count = 0;
int width = 480;
int height = 640;
int fps = 30;
AVFrame *pFrameARGB;
int drawChange = 1;

static int apply_filters(/*AVFormatContext *ifmt_ctx*/) {
    char args[512];
    int ret = 0;
    AVFilterInOut **outputs;
    outputs = av_calloc((size_t) (videoInputs + 1), sizeof(AVFilterInOut *));
    if (!outputs) {
        printf("Cannot alloc int\n");
        return -1;
    }
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (!inputs) {
        printf("Cannot alloc out\n");
        return -1;
    }

    if (pCodecCtx == NULL) {
        ret = -1;
        goto END;
    }

    if (filter_graph)
        avfilter_graph_free(&filter_graph);
    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        printf("Cannot create filter graph\n");
        ret = -1;
        goto END;
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

    for (int i = 0; i < videoInputs; i++) {
        char name[6];
        if (videoInputs > 1) {
            snprintf(name, 6, "%d:v", i);
        } else {
            snprintf(name, 6, "in");
        }
        outputs[i] = avfilter_inout_alloc();
        if (!outputs[i]) {
            printf("Cannot alloc input\n");
            ret = -1;
            goto END;
        }
        ret = avfilter_graph_create_filter(&buffersrc_ctx[i], buffersrc, name,
                                           args, NULL, filter_graph);
        if (ret < 0) {
            printf("Cannot create buffer source\n");
            goto END;
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
        goto END;
    }
    inputs->name = av_strdup(name);
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, outputs, NULL)) < 0) {
        goto END;
    }
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
        goto END;
    }

    END:

    avfilter_inout_free(&inputs);
    avfilter_inout_free(outputs);

    return ret;
}

static int convertFrame(AVFrame *pSource, AVFrame *pDest) {
    struct SwsContext *img_convert_ctx;
    int ret = 0;

    if (pCodecCtx == NULL) {
        return -1;
    }

    av_frame_free(&pDest);
    pDest = av_frame_alloc();
    pDest->width = pCodecCtx->width;
    pDest->height = pCodecCtx->height;
    pDest->format = pCodecCtx->pix_fmt;
    ret = av_frame_get_buffer(pDest, 0);
    if (ret != 0) {
        LOGE("Error getting frame buffer %s\n", av_err2str(ret));
        return -1;
    }
    pDest->linesize[0] = pCodecCtx->width;
    pDest->linesize[1] = pCodecCtx->width / 2;
    pDest->linesize[2] = pCodecCtx->width / 2;

    img_convert_ctx = sws_getContext(
            pSource->width,
            pSource->height,
            (enum AVPixelFormat) pSource->format,
            pDest->width,
            pDest->height,
            (enum AVPixelFormat) pDest->format,
            SWS_BICUBIC, NULL, NULL, NULL);

    if (img_convert_ctx == NULL) return -1;

    ret = sws_scale(
            img_convert_ctx,
            pSource->data,
            pSource->linesize,
            0,
            pSource->height,
            pDest->data,
            pDest->linesize);

    sws_freeContext(img_convert_ctx);

    return ret;
}

static int processFrame(int n, AVFrame *pOutFrame) {
    int ret = 0;
    AVFrame *pFilteredFrame = av_frame_alloc();

    if (filter_change) {
        apply_filters();
    }

    filter_change = 0;

    if (filter_graph == NULL || buffersrc_ctx == NULL || buffersink_ctx == NULL || ofmt_ctx == NULL) {
        goto END;
    }

    ret = av_buffersrc_add_frame(buffersrc_ctx[n], pOutFrame);
    if (ret < 0) {
        printf("Error while feeding the filtergraph\n");
        goto END;
    }

    ret = av_buffersink_get_frame_flags(buffersink_ctx, pFilteredFrame, 0);
    if (ret < 0) {
        printf("Error getting frame %s \n", av_err2str(ret));
        goto END;
    }

    if (pFilteredFrame) {
        ret = convertFrame(pFilteredFrame, pOutFrame);
        if (ret < 0) goto END;
    }
    av_frame_free(&pFilteredFrame);

    enc_pkt.data = NULL;
    enc_pkt.size = 0;

    ret = avcodec_send_frame(pCodecCtx, pOutFrame);
    if (ret != 0) {
        LOGE("avcodec_send_frame error");
        goto END;
    }

    ret = avcodec_receive_packet(pCodecCtx, &enc_pkt);
    if (ret != 0 || enc_pkt.size <= 0) {
        LOGE("avcodec_receive_packet error %s", av_err2str(ret));
        goto END;
    }

    enc_pkt.stream_index = video_st->index;
    enc_pkt.pts = count * (video_st->time_base.den) / ((video_st->time_base.num) * fps);
    enc_pkt.dts = enc_pkt.pts;
    enc_pkt.duration = (video_st->time_base.den) / ((video_st->time_base.num) * fps);
    enc_pkt.pos = -1;

    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    if (ret != 0) {
        LOGE("av_interleaved_write_frame failed");
        goto END;
    }
    count++;

    END:
    av_frame_free(&pFilteredFrame);
    av_packet_unref(&enc_pkt);

    return ret;
}

JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_init
        (JNIEnv *jniEnv, jobject instance, jstring url) {

    buffersrc = (AVFilter *) avfilter_get_by_name("buffer");
    buffersink = (AVFilter *) avfilter_get_by_name("buffersink");
    buffersrc_ctx = av_calloc((size_t) (videoInputs + 1), sizeof(AVFilterContext));

    const char *out_url = (*jniEnv)->GetStringUTFChars(jniEnv, url, 0);

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
    pCodecCtx->codec_id = pCodec->id;
    pCodecCtx->pix_fmt = OUTPUT_FMT;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->width = width;
    pCodecCtx->height = height;
    pCodecCtx->framerate = (AVRational) {fps, 1};
    pCodecCtx->time_base = (AVRational) {1, fps};
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->gop_size = 50;
    /* Some formats want stream headers to be separate. */
    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //H264 codec param
    pCodecCtx->qcompress = 0.6;
    pCodecCtx->qmin = 10;
    pCodecCtx->qmax = 51;
    pCodecCtx->max_b_frames = 0;
    AVDictionary *param = 0;
    //H.264
    if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "superfast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    ret = avcodec_open2(pCodecCtx, pCodec, &param);
    if (ret < 0) {
        LOGE("Failed to open encoder! %s\n", av_err2str(ret));
        return -1;
    }

    //Add a new stream to output,should be called by the user before avformat_write_header() for muxing
    video_st = avformat_new_stream(ofmt_ctx, pCodec);
    if (video_st == NULL) {
        return -1;
    }
    video_st->time_base = (AVRational) {1, fps};
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

JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_changeFilter(
        JNIEnv *jniEnv, jobject instance, jint curFilter) {
    switch (48 + curFilter % 6) {
        case FILTER_NULL: {
            printf("\nnow using null filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = "null";
            break;
        }
        /*case FILTER_MIRROR: {
            printf("\nnow using mirror filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = filter_mirror;
            break;
        }
        case FILTER_NEGATE: {
            printf("\nnow using negate filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = filter_negate;
            break;
        }
        case FILTER_EDGE: {
            printf("\nnow using edge filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = filter_edge;
            break;
        }
        case FILTER_SPLIT4: {
            printf("\nnow using split4 filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = filter_split4;
            break;
        }
        case FILTER_VINTAGE: {
            printf("\nnow using vintage filter\nPress other numbers for other filters:");
            filter_change = 1;
            filter_descr = filter_vintage;
            break;
        }*/
        default: {
            break;
        }
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_encodeFrame(
        JNIEnv *jniEnv, jobject instance,
        jint n,
        jint w,
        jint h,
        jobject bufferY,
        jint pixY,
        jint rowY,
        jobject bufferU,
        jint pixU,
        jint rowU,
        jobject bufferV,
        jint pixV,
        jint rowV
) {
    int ret = 0;

    int picture_size = av_image_get_buffer_size(
            AV_PIX_FMT_YUV420P,
            w,
            h,
            1);
    uint8_t *buffers = (uint8_t *) av_malloc(picture_size);
    AVFrame *pFrameYUV = av_frame_alloc();
    AVFrame *pOutFrame = av_frame_alloc();
    av_image_fill_arrays(
            pFrameYUV->data,
            pFrameYUV->linesize,
            buffers,
            AV_PIX_FMT_YUV420P,
            w,
            h,
            1);

    jbyte *yin = (*jniEnv)->GetDirectBufferAddress(jniEnv, bufferY);
    jbyte *uin = (*jniEnv)->GetDirectBufferAddress(jniEnv, bufferU);
    jbyte *vin = (*jniEnv)->GetDirectBufferAddress(jniEnv, bufferV);
    int srcIndex = 0;
    int dstIndex = 0;
    jbyte *yArray = av_malloc_array((size_t) (w * h + 1), sizeof(jbyte));
    jbyte *uArray = av_malloc_array((size_t) (w * h / 4 + 1), sizeof(jbyte));
    jbyte *vArray = av_malloc_array((size_t) (w * h / 4 + 1), sizeof(jbyte));

    if (ofmt_ctx == NULL) {
        ret = -1;
        goto END;
    }

    if (ofmt_ctx->pb == NULL) {
        ret = -1;
        goto END;
    }

    for (int i = 0; i < h; i = i + 1) {
        for (int j = 0; j < w; j = j + 1) {
            yArray[dstIndex] = yin[srcIndex];
            dstIndex ++;
            srcIndex ++;
        }
        srcIndex += rowY - w;
    }
    srcIndex = 0;
    dstIndex = 0;
    for (int i = 0; i < h/2; i ++) {
        for (int j = 0; j < w/2; j ++) {
            uArray[dstIndex] = uin[srcIndex];
            dstIndex ++;
            srcIndex += pixU;
        }
        if (pixU == 2) {
            srcIndex += rowU - w;
        } else if (pixU == 1) {
            srcIndex += rowU - w / 2;
        }
    }
    srcIndex = 0;
    dstIndex = 0;
    for (int i = 0; i < h/2; i ++) {
        for (int j = 0; j < w/2; j ++) {
            vArray[dstIndex] = vin[srcIndex];
            dstIndex ++;
            srcIndex += pixV;
        }
        if (pixV == 2) {
            srcIndex += rowV - w;
        } else if (pixV == 1) {
            srcIndex += rowV - w / 2;
        }
    }

    pFrameYUV->data[0] = (uint8_t *) yArray;
    pFrameYUV->data[1] = (uint8_t *) uArray;
    pFrameYUV->data[2] = (uint8_t *) vArray;
    pFrameYUV->pts = count;
    pFrameYUV->format = AV_PIX_FMT_YUV420P;
    pFrameYUV->width = w;
    pFrameYUV->height = h;
    pFrameYUV->linesize[0] = w;
    pFrameYUV->linesize[1] = w / 2;
    pFrameYUV->linesize[2] = w / 2;

    ret = convertFrame(pFrameYUV, pOutFrame);
    if (ret < 0) goto END;

    ret = processFrame(n, pOutFrame);
    if (ret != 0) goto END;

    END:
    av_frame_free(&pFrameYUV);
    av_frame_free(&pOutFrame);

    if (buffers != NULL) {
        av_free(buffers);
    }
    if (yArray != NULL) {
        av_free(yArray);
    }
    if (uArray != NULL) {
        av_free(uArray);
    }
    if (vArray != NULL) {
        av_free(vArray);
    }

    return ret;
}

JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_encodeARGBFrame(
        JNIEnv *jniEnv, jobject instance,
        jint n,
        jint w,
        jint h,
        jobject buffer
) {
    int ret = 0;
    AVFrame *pOutFrame = av_frame_alloc();

    if (ofmt_ctx == NULL) {
        ret = -1;
        goto END;
    }

    if (ofmt_ctx->pb == NULL) {
        ret = -1;
        goto END;
    }

    if (drawChange != 0 && pFrameARGB == NULL && buffer != NULL) {
        pFrameARGB = av_frame_alloc();
        int picture_size = av_image_get_buffer_size(
                AV_PIX_FMT_RGBA,
                w,
                h,
                1);
        uint8_t *buffers = (uint8_t *) av_malloc(picture_size);
        av_image_fill_arrays(
                pFrameARGB->data,
                pFrameARGB->linesize,
                buffers,
                AV_PIX_FMT_RGBA,
                w,
                h,
                1);
        av_free(buffers);

        uint8_t *in = (*jniEnv)->GetDirectBufferAddress(jniEnv, buffer);
        pFrameARGB->data[0] = in;
        pFrameARGB->pts = count;
        pFrameARGB->format = AV_PIX_FMT_RGBA;
        pFrameARGB->width = w;
        pFrameARGB->height = h;
        pFrameARGB->linesize[0] = w * 4;
    }

    if (pFrameARGB != NULL) {
        ret = convertFrame(pFrameARGB, pOutFrame);
        if (ret < 0) goto END;

        ret = processFrame(n, pOutFrame);
        if (ret != 0) goto END;

        drawChange = 0;
    }

    END:

    //av_frame_free(&pFrameARGB);
    av_frame_free(&pOutFrame);

    return ret;
}

JNIEXPORT jint JNICALL Java_com_example_videoapp_utils_FFmpegHandler_close
        (JNIEnv *jniEnv, jobject instance) {

    if (pFrameARGB != NULL) {
        av_frame_free(&pFrameARGB);
    }

    if (pCodecCtx != NULL) {
        avcodec_close(pCodecCtx);
        pCodecCtx = NULL;
    }
    if (ofmt_ctx) {
        if (ofmt_ctx->pb != NULL) {
            avio_close(ofmt_ctx->pb);
        }
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

