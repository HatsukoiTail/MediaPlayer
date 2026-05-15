#include "CodecContext.h"

#include <cassert>

extern "C"
{
#include <libavutil/hwcontext.h>
}

#include "FFmpegException.h"
#include "Helper.h"
#include <iostream>

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const auto hw_pix_fmt = *reinterpret_cast<AVPixelFormat *>(ctx->opaque);
    av_log(ctx, AV_LOG_INFO, "get_hw_format: opaque=%d, candidates: ", *(AVPixelFormat *)ctx->opaque);
    while (*pix_fmts != AV_PIX_FMT_NONE)
    {
        if (*pix_fmts == hw_pix_fmt)
        {
            return *pix_fmts;
        }
        pix_fmts++;
    }
    return AV_PIX_FMT_NONE;
};

AVCodecContextPtr open_decode_context(const AVStream *stream, AVBufferRef *hw_device_ctx)
{
    const auto decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (decoder == nullptr)
    {
        throw FFmpegException("Failed to find decoder");
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(decoder);
    if (codec_ctx == nullptr)
    {
        throw FFmpegException("Failed to allocate codec context");
    }

    if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0)
    {
        avcodec_free_context(&codec_ctx);
        throw FFmpegException("Failed to copy codec parameters");
    }

    if (hw_device_ctx != nullptr && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        // 使用硬件解码
        AVHWDeviceContext *hw_device = reinterpret_cast<AVHWDeviceContext *>(hw_device_ctx->data);
        AVHWDeviceType device_type = hw_device->type;

        AVPixelFormat hw_pix_fmt = codec_support_hw_format(decoder, device_type);

        if (hw_pix_fmt == AV_PIX_FMT_NONE)
        {
            avcodec_free_context(&codec_ctx);
            throw FFmpegException("Failed to find a suitable hardware format");
        }

        codec_ctx->opaque = av_malloc(sizeof(hw_pix_fmt));
        *reinterpret_cast<AVPixelFormat *>(codec_ctx->opaque) = hw_pix_fmt;
        codec_ctx->get_format = get_hw_format;
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        if (codec_ctx->hw_device_ctx == nullptr)
        {
            av_freep(&codec_ctx->opaque);
            avcodec_free_context(&codec_ctx);
            throw FFmpegException("Failed to copy hardware device context");
        }
    }

    if (avcodec_open2(codec_ctx, decoder, nullptr) < 0)
    {
        av_buffer_unref(&codec_ctx->hw_device_ctx);
        av_freep(&codec_ctx->opaque);
        avcodec_free_context(&codec_ctx);
        throw FFmpegException("Failed to open codec");
    }
    return AVCodecContextPtr(codec_ctx);
}

void open_encode_context(AVCodecContext *codec_ctx, AVBufferRef *hw_device_ctx)
{
    if (hw_device_ctx != nullptr && codec_ctx->codec->type == AVMEDIA_TYPE_VIDEO)
    {
        // 使用硬件编码
        AVBufferRef *hw_frames_ctx = av_hwframe_ctx_alloc(codec_ctx->hw_device_ctx);
        if (!hw_frames_ctx)
        {
            av_buffer_unref(&codec_ctx->hw_device_ctx);
            throw FFmpegException("Failed to allocate hardware frame context");
        }

        AVHWFramesContext *frame_ctx = (AVHWFramesContext *)hw_frames_ctx->data;
        frame_ctx->format = codec_ctx->pix_fmt;
        frame_ctx->sw_format = codec_ctx->sw_pix_fmt;
        frame_ctx->width = codec_ctx->width;
        frame_ctx->height = codec_ctx->height;
        frame_ctx->initial_pool_size = 16;

        if ((av_hwframe_ctx_init(hw_frames_ctx) < 0))
        {
            av_buffer_unref(&hw_frames_ctx);
            av_buffer_unref(&codec_ctx->hw_device_ctx);
            throw FFmpegException("Failed to initialize hardware frame context");
        }

        codec_ctx->hw_frames_ctx = hw_frames_ctx;
    }

    if (avcodec_open2(codec_ctx, nullptr, nullptr) < 0)
    {
        av_buffer_unref(&codec_ctx->hw_frames_ctx);
        av_buffer_unref(&codec_ctx->hw_device_ctx);
        throw FFmpegException("Failed to open codec");
    }
}

bool format_supported_by_encoder(std::string_view codec_name, AVPixelFormat pix_fmt)
{
    auto encoder = avcodec_find_encoder_by_name(codec_name.data());
    if (encoder == nullptr)
    {
        throw FFmpegException("Failed to find encoder");
    }

    const void *pix_configs = nullptr;
    int ret = avcodec_get_supported_config(nullptr, encoder, AV_CODEC_CONFIG_PIX_FORMAT, 0, &pix_configs, nullptr);
    if (ret < 0 || pix_configs == nullptr)
    {
        return false;
    }

    const AVPixelFormat *pix_fmts = static_cast<const AVPixelFormat *>(pix_configs);
    while (*pix_fmts != AV_PIX_FMT_NONE)
    {
        if (*pix_fmts == pix_fmt)
        {
            return true;
        }
        pix_fmts++;
    }

    return false;
}

bool format_supported_by_encoder(std::string_view codec_name, AVSampleFormat smp_fmt)
{
    auto encoder = avcodec_find_encoder_by_name(codec_name.data());
    if (encoder == nullptr)
    {
        throw FFmpegException("Failed to find encoder");
    }

    const void *smp_configs = nullptr;
    int ret = avcodec_get_supported_config(nullptr, encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &smp_configs, nullptr);
    if (ret < 0 || smp_configs == nullptr)
    {
        return false;
    }

    const AVSampleFormat *smp_fmts = static_cast<const AVSampleFormat *>(smp_configs);
    while (*smp_fmts != AV_SAMPLE_FMT_NONE)
    {
        if (*smp_fmts == smp_fmt)
        {
            return true;
        }
        smp_fmts++;
    }

    return false;
}
