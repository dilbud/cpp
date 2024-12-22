extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iostream>

#ifdef av_err2str
#undef av_err2str
#include <string>
#include <vector>
av_always_inline std::string av_err2string(int errnum) {
    char str[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).c_str()
#endif  // av_err2str

struct AppContext {
    const char *src_filename = "/workspaces/cpp/video_data/sintel-short.mp4";
    const char *video_dst_filename = "/workspaces/cpp/video_data/new-sintel-short.mp4";
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    int width;
    int height;
    enum AVPixelFormat pix_fmt;
    FILE *video_dst_file = NULL;
    int video_dst_bufsize;
    int video_frame_count = 0;
    int video_dst_linesize[4];
    uint8_t *video_dst_data[4] = {NULL};
};

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                              enum AVMediaType type, AppContext *appCtx) {
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type),
                appCtx->src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int output_video_frame(AVFrame *frame, AppContext *appCtx) {
    if (frame->width != appCtx->width || frame->height != appCtx->height || frame->format != appCtx->pix_fmt) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr,
                "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                appCtx->width, appCtx->height, av_get_pix_fmt_name(appCtx->pix_fmt), frame->width, frame->height,
                av_get_pix_fmt_name((AVPixelFormat)(frame->format)));
        return -1;
    }

    printf("video_frame n:%d\n", appCtx->video_frame_count++);

    /* copy decoded frame to destination buffer:
     * this is required since rawvideo expects non aligned data */
    av_image_copy(appCtx->video_dst_data, appCtx->video_dst_linesize, const_cast<const uint8_t **>(frame->data),
                  frame->linesize, appCtx->pix_fmt, appCtx->width, appCtx->height);

    /* write to rawvideo file */
    fwrite(appCtx->video_dst_data[0], 1, appCtx->video_dst_bufsize, appCtx->video_dst_file);
    return 0;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt, AppContext *appCtx) {
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, appCtx->frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO) {
            ret = output_video_frame(appCtx->frame, appCtx);
        }

        av_frame_unref(appCtx->frame);
    }

    return ret;
}

int main_demux_decoder(int argc, char **argv) {
    int ret = 0;

    AppContext appCtx;

    int video_stream_idx = -1;
    AVCodecContext *video_dec_ctx = NULL;
    AVStream *video_stream = NULL;

    AVFormatContext *fmt_ctx = NULL;
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, appCtx.src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", appCtx.src_filename);
        exit(1);
    }
    std::printf("AVFormatContext done\n");
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO, &appCtx) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        appCtx.video_dst_file = fopen(appCtx.video_dst_filename, "wb");
        if (!appCtx.video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", appCtx.video_dst_filename);
            ret = 1;
            goto end;
        }

        /* allocate image where the decoded image will be put */
        appCtx.width = video_dec_ctx->width;
        appCtx.height = video_dec_ctx->height;
        appCtx.pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(appCtx.video_dst_data, appCtx.video_dst_linesize, appCtx.width, appCtx.height,
                             appCtx.pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto end;
        }
        appCtx.video_dst_bufsize = ret;
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, appCtx.src_filename, 0);

    if (!video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    appCtx.frame = av_frame_alloc();
    if (!appCtx.frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    appCtx.pkt = av_packet_alloc();
    if (!appCtx.pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (video_stream)
        printf("Demuxing video from file '%s' into '%s'\n", appCtx.src_filename, appCtx.video_dst_filename);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, appCtx.pkt) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise
        // skip it
        if (appCtx.pkt->stream_index == video_stream_idx) {
            ret = decode_packet(video_dec_ctx, appCtx.pkt, &appCtx);
        }
        if (ret < 0) {
            break;
        }
    }

    /* flush the decoders */
    if (video_dec_ctx) {
        decode_packet(video_dec_ctx, NULL, &appCtx);
    }

    printf("Demuxing succeeded.\n");

    if (video_stream) {
        printf(
            "Play the output video file with the command:\n"
            "ffplay -f rawvideo -pixel_format %s -video_size %dx%d %s\n",
            av_get_pix_fmt_name(appCtx.pix_fmt), appCtx.width, appCtx.height, appCtx.video_dst_filename);
    }

end:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (appCtx.video_dst_file) fclose(appCtx.video_dst_file);
    av_packet_free(&(appCtx.pkt));
    av_frame_free(&(appCtx.frame));
    av_free(appCtx.video_dst_data[0]);

    return 0;
}

struct AppContext_1 {
    const char *src_filename = "/workspaces/cpp/video_data/sintel-short.mp4";
};

int main_idn(int argc, char **argv) {
    AppContext_1 appCtx_1;
    AVFormatContext *fmt_ctx = NULL;
    const AVDictionaryEntry *tag = NULL;
    int ret;

    if ((ret = avformat_open_input(&fmt_ctx, appCtx_1.src_filename, NULL, NULL))) return ret;

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        printf("%s=%s\n", tag->key, tag->value);
    }

    avformat_close_input(&fmt_ctx);
    return 0;
}

struct AppContext_2 {
    const char *src_filename100 = "/workspaces/cpp/media/vp8/chunk100.bin";
    const char *src_filename101 = "/workspaces/cpp/media/vp8/chunk101.bin";
    AVCodecParameters *param = NULL;
    AVCodecContext *video_dec_ctx = NULL;
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
};

int save_frame_as_jpeg(AVFrame *pFrame, int curFrameNumber) {
    AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_JPEG2000);
    if (!jpegCodec) {
        return -1;
    }
    AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        return -1;
    }

    jpegContext->pix_fmt = (AVPixelFormat)pFrame->format;
    jpegContext->height = pFrame->height;
    jpegContext->width = pFrame->width;
    jpegContext->time_base = (AVRational){1, 1};

    if (avcodec_open2(jpegContext, jpegCodec, NULL) < 0) {
        return -1;
    }
    FILE *JPEGFile;
    char JPEGFName[256];

    AVPacket packet = {.data = NULL, .size = 0};
    av_init_packet(&packet);
    int gotFrame;

    if (avcodec_encode_video2(jpegContext, &packet, pFrame, &gotFrame) < 0) {
        return -1;
    }

    sprintf(JPEGFName, "/workspaces/cpp/media/vp8/dvr-%06d.jpg", curFrameNumber);
    JPEGFile = fopen(JPEGFName, "wb");
    fwrite(packet.data, 1, packet.size, JPEGFile);
    fclose(JPEGFile);

    av_free_packet(&packet);
    avcodec_close(jpegContext);
    return 0;
}

static int output_video_frame_2(AVFrame *frame, AppContext_2 *appCtx, int curFrameNumber) {
    if (frame->width != appCtx->param->width || frame->height != appCtx->param->height) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr,
                "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                appCtx->param->width, appCtx->param->height, av_get_pix_fmt_name((AVPixelFormat)(frame->format)),
                frame->width, frame->height, av_get_pix_fmt_name((AVPixelFormat)(frame->format)));
        return -1;
    }
    save_frame_as_jpeg(frame, curFrameNumber);

    return 0;
}

void save_frame_as_jpeg(AVFrame *pFrame, int width, int height, int iFrame) {
    AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        fprintf(stderr, "MJPEG codec not found\n");
        return;
    }
    AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        fprintf(stderr, "Could not allocate JPEG codec context\n");
        return;
    }
    jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpegContext->height = height;
    jpegContext->width = width;
    jpegContext->time_base = (AVRational){1, 25};
    if (avcodec_open2(jpegContext, jpegCodec, NULL) < 0) {
        fprintf(stderr, "Could not open JPEG codec\n");
        avcodec_free_context(&jpegContext);
        return;
    }
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate AVPacket\n");
        avcodec_free_context(&jpegContext);
        return;
    }
    if (avcodec_send_frame(jpegContext, pFrame) < 0) {
        fprintf(stderr, "Error sending frame to codec\n");
        av_packet_free(&packet);
        avcodec_free_context(&jpegContext);
        return;
    }
    if (avcodec_receive_packet(jpegContext, packet) == 0) {
        char filename[64];
        // snprintf(filename, sizeof(filename), "frame%03d.jpg", iFrame);
        snprintf(filename, sizeof(filename), "/workspaces/cpp/media/vp8/%s_%03d.jpg", "test", iFrame);
        // snprintf(filename, sizeof(filename), "/workspaces/cpp/media/vp8/%s_%03d.jpg", "test", iFrame);
        FILE *jpegFile = fopen(filename, "wb");
        if (jpegFile) {
            fwrite(packet->data, 1, packet->size, jpegFile);
            fclose(jpegFile);
            printf("Saved %s\n", filename);
        } else {
            fprintf(stderr, "Could not open %s\n", filename);
        }
        av_packet_unref(packet);
    } else {
        fprintf(stderr, "Error encoding frame\n");
    }
    av_packet_free(&packet);
    avcodec_free_context(&jpegContext);
}

// Save RGB image as PPM file format
static void ppm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename) {
    FILE *f;
    int i;

    f = fopen(filename, "wb");
    fprintf(f, "P6\n%d %d\n%d\n", xsize, ysize, 255);

    for (i = 0; i < ysize; i++) {
        fwrite(buf + i * wrap, 1, xsize * 3, f);
    }

    fclose(f);
}

static int decode_packet_2(AVCodecContext *dec, const AVPacket *pkt, AppContext_2 *appCtx) {
    char buf[1024];
    int ret = 0;
    int sts;
    int curFrameNumber = 0;
    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    SwsContext *sws_ctx = sws_getContext(dec->width, dec->height, dec->pix_fmt, dec->width, dec->height,
                                         AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
    if (sws_ctx == nullptr) {
        return -1;
    }

    struct SwsContext *sws_ctxJpj = sws_getContext(dec->width, dec->height, dec->pix_fmt, dec->width, dec->height,
                                                   AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        return -1;
    }
    // Allocate frame for storing image converted to RGB.
    ////////////////////////////////////////////////////////////////////////////
    AVFrame *pRGBFrame = av_frame_alloc();

    pRGBFrame->format = AV_PIX_FMT_RGB24;
    pRGBFrame->width = dec->width;
    pRGBFrame->height = dec->height;

    {
        sts = av_frame_get_buffer(pRGBFrame, 0);

        if (sts < 0) {
            return -1;
        }
    }

    AVFrame *pFrameYUV = av_frame_alloc();
    pFrameYUV->format = AV_PIX_FMT_YUV420P;
    pFrameYUV->color_range = dec->color_range;
    pFrameYUV->width = dec->width;
    pFrameYUV->height = dec->height;
    {
        sts = av_frame_get_buffer(pFrameYUV, 0);

        if (sts < 0) {
            return -1;
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, appCtx->frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // Convert from input format (e.g YUV420) to RGB and save to PPM:
        ////////////////////////////////////////////////////////////////////////////
        ret = sws_scale(sws_ctx,                  // struct SwsContext* c,
                        appCtx->frame->data,      // const uint8_t* const srcSlice[],
                        appCtx->frame->linesize,  // const int srcStride[],
                        0,                        // int srcSliceY,
                        appCtx->frame->height,    // int srcSliceH,
                        pRGBFrame->data,          // uint8_t* const dst[],
                        pRGBFrame->linesize);     // const int dstStride[]);

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO) {
            std::printf("decoded\n");
            curFrameNumber++;
            // ret = output_video_frame_2(appCtx->frame, appCtx, curFrameNumber);
            std::printf("%d\n", appCtx->frame->key_frame);
            snprintf(buf, sizeof(buf), "/workspaces/cpp/media/vp8/%s_%03d.ppm", "test", dec->frame_number);
            ppm_save(pRGBFrame->data[0], pRGBFrame->linesize[0], pRGBFrame->width, pRGBFrame->height, buf);
        }

        {
            sws_scale(sws_ctxJpj, appCtx->frame->data, appCtx->frame->linesize, 0, appCtx->frame->height, pFrameYUV->data,
                      pFrameYUV->linesize);
            save_frame_as_jpeg(pFrameYUV, pFrameYUV->width, pFrameYUV->height, dec->frame_number);
        }

        av_frame_unref(appCtx->frame);
    }

    return ret;
}

int main(int argc, char **argv) {
    int ret;
    AppContext_2 appCtx_2;

    appCtx_2.param = avcodec_parameters_alloc();

    appCtx_2.param->codec_type = AVMEDIA_TYPE_VIDEO;
    appCtx_2.param->codec_id = AV_CODEC_ID_VP8;
    appCtx_2.param->height = 306;
    appCtx_2.param->width = 720;
    appCtx_2.param->color_space = AVCOL_SPC_SMPTE170M;
    appCtx_2.param->color_range = AVCOL_RANGE_UNSPECIFIED;

    const AVCodec *dec = NULL;
    dec = avcodec_find_decoder(appCtx_2.param->codec_id);
    if (!dec) {
        fprintf(stderr, "Failed to find codec\n");
        return AVERROR(EINVAL);
    }
    appCtx_2.video_dec_ctx = avcodec_alloc_context3(dec);
    if (!(appCtx_2.video_dec_ctx)) {
        fprintf(stderr, "Failed to allocate the codec context\n");
        return AVERROR(ENOMEM);
    }

    /* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(appCtx_2.video_dec_ctx, appCtx_2.param)) < 0) {
        fprintf(stderr, "Failed to copy codec parameters to decoder context\n");
        return ret;
    }

    /* Init the decoders */
    if ((ret = avcodec_open2(appCtx_2.video_dec_ctx, dec, NULL)) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return ret;
    }

    appCtx_2.frame = av_frame_alloc();
    if (!appCtx_2.frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        return ret;
    }

    {
        appCtx_2.pkt = av_packet_alloc();
        if (!appCtx_2.pkt) {
            fprintf(stderr, "Could not allocate packet\n");
            ret = AVERROR(ENOMEM);
            return ret;
        }
        std::vector<uint8_t> vec_buf;
        std::ifstream bin_file(appCtx_2.src_filename100, std::ios::binary);

        if (bin_file.good()) {
            /*Read Binary data using streambuffer iterators.*/
            std::vector<uint8_t> v_buf((std::istreambuf_iterator<char>(bin_file)), (std::istreambuf_iterator<char>()));
            vec_buf = v_buf;
            bin_file.close();
        }

        else {
            throw std::exception();
        }

        appCtx_2.pkt->stream_index = 0;         // Set the correct stream index
        appCtx_2.pkt->pts = 4302528;            // Set presentation timestamp
        appCtx_2.pkt->dts = 4302528;            // Set decoding timestamp (same as pts for keyframes)
        appCtx_2.pkt->data = vec_buf.data();    // Set pointer to encoded data
        appCtx_2.pkt->size = 4821;              // Set size of data
        appCtx_2.pkt->flags = AV_PKT_FLAG_KEY;  // Set keyframe flag

        ret = avcodec_send_packet(appCtx_2.video_dec_ctx, appCtx_2.pkt);
        if (ret < 0) {
            fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        ret = decode_packet_2(appCtx_2.video_dec_ctx, appCtx_2.pkt, &appCtx_2);
        appCtx_2.pkt = NULL;
    }

    {
        appCtx_2.pkt = av_packet_alloc();
        if (!appCtx_2.pkt) {
            fprintf(stderr, "Could not allocate packet\n");
            ret = AVERROR(ENOMEM);
            return ret;
        }
        std::vector<uint8_t> vec_buf;
        std::ifstream bin_file(appCtx_2.src_filename101, std::ios::binary);

        if (bin_file.good()) {
            /*Read Binary data using streambuffer iterators.*/
            std::vector<uint8_t> v_buf((std::istreambuf_iterator<char>(bin_file)), (std::istreambuf_iterator<char>()));
            vec_buf = v_buf;
            bin_file.close();
        }

        else {
            throw std::exception();
        }

        appCtx_2.pkt->stream_index = 0;                // Set the correct stream index
        appCtx_2.pkt->pts = 4336132;                   // Set presentation timestamp
        appCtx_2.pkt->dts = 4336132;                   // Set decoding timestamp (same as pts for keyframes)
        appCtx_2.pkt->data = vec_buf.data();           // Set pointer to encoded data
        appCtx_2.pkt->size = 20230;                    // Set size of data
        appCtx_2.pkt->flags = AV_PKT_FLAG_DISPOSABLE;  // Set keyframe flag

        ret = avcodec_send_packet(appCtx_2.video_dec_ctx, appCtx_2.pkt);
        if (ret < 0) {
            fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        ret = decode_packet_2(appCtx_2.video_dec_ctx, appCtx_2.pkt, &appCtx_2);
        appCtx_2.pkt = NULL;
    }

    /* flush the decoders */
    if (appCtx_2.video_dec_ctx) {
        decode_packet_2(appCtx_2.video_dec_ctx, NULL, &appCtx_2);
    }

    avcodec_parameters_free(&appCtx_2.param);
    return 0;
}