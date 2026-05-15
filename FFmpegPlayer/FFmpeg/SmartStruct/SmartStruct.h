#ifndef SMARTSTRUCT_H
#define SMARTSTRUCT_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <portaudio.h>
}

#include <memory>

// 自定义删除器
struct AVPacketDeleter
{
    void operator()(AVPacket *ptr) noexcept
    {
        if (ptr)
            av_packet_free(&ptr);
    }
};
struct AVFrameDeleter
{
    void operator()(AVFrame *ptr) noexcept
    {
        if (ptr)
            av_frame_free(&ptr);
    }
};
struct AVFormatContextDeleter
{
    void operator()(AVFormatContext *ctx) noexcept
    {
        if (!ctx)
            return;

        if (ctx->iformat)
        {
            avformat_close_input(&ctx);
        }
        else
        {
            if (ctx->pb)
                avio_context_free(&ctx->pb);
            avformat_free_context(ctx);
        }
    }
};
struct AVCodecContextDeleter
{
    void operator()(AVCodecContext *ctx) noexcept
    {
        if (ctx)
            avcodec_free_context(&ctx);
    }
};
struct SwsContextDeleter
{
    void operator()(SwsContext* ctx) noexcept
    {
        if (ctx)
            sws_freeContext(ctx);
    }
};
struct SwrContextDeleter
{
    void operator()(SwrContext* ctx) noexcept
    {
        if (ctx)
            swr_free(&ctx);
    }
};
struct PaStreamDeleter
{
    void operator()(PaStream* stream) noexcept
    {
        if (stream)
            Pa_CloseStream(stream);
    }
};
struct AVFilterGraphDeleter
{
    void operator()(AVFilterGraph* graph) noexcept
    {
        if (graph)
            avfilter_graph_free(&graph);
    }
};

// 智能指针
using AVPacketPointer = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePointer = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVFormatContextPointer = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPointer = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using SwsContextPointer = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPointer = std::unique_ptr<SwrContext, SwrContextDeleter>;
using PaStreamPointer = std::unique_ptr<PaStream, PaStreamDeleter>;
using AVFilterGraphPointer = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;
using SharedFramePointer = std::shared_ptr<AVFrame>;

inline SharedFramePointer make_shared_frame(AVFramePointer frame)
{
    return SharedFramePointer(frame.release(), AVFrameDeleter());
}

#endif // SMARTSTRUCT_H
