#pragma once

#ifndef FFMPEG_CODECCONTEXT_H
#define FFMPEG_CODECCONTEXT_H

#include <memory>
#include <string_view>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct AVCodecContextDeleter
{
    void operator()(AVCodecContext *ptr) const
    {
        if (ptr == nullptr)
            return;

        av_buffer_unref(&ptr->hw_device_ctx);
        av_freep(&ptr->opaque);
        avcodec_free_context(&ptr);
    }
};

using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

AVCodecContextPtr open_decode_context(const AVStream *stream, AVBufferRef *hw_device_ctx = nullptr);

void open_encode_context(AVCodecContext* codec_ctx, AVBufferRef *hw_device_ctx = nullptr);

bool format_supported_by_encoder(std::string_view codec_name, AVPixelFormat pix_fmt);

bool format_supported_by_encoder(std::string_view codec_name, AVSampleFormat smp_fmt);

#endif // FFMPEG_CODECCONTEXT_H