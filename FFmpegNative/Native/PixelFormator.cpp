#include "PixelFormator.h"

#include "FFmpegException.h"

PixelFormator::PixelFormator()
    : sws_ctx(nullptr), dst_width(0), dst_height(0), dst_format(AV_PIX_FMT_NONE)
{
}

PixelFormator::PixelFormator(int dst_width, int dst_height, AVPixelFormat dst_format)
    : sws_ctx(nullptr), dst_width(dst_width), dst_height(dst_height), dst_format(dst_format)
{
}

PixelFormator::~PixelFormator()
{
    sws_free_context(&this->sws_ctx);
}

void PixelFormator::reset(int dst_width, int dst_height, AVPixelFormat dst_format)
{
    if (this->sws_ctx != nullptr && this->sws_ctx->dst_w == dst_width && this->sws_ctx->dst_h == dst_height && this->sws_ctx->dst_format == dst_format)
    {
        return;
    }
    sws_free_context(&this->sws_ctx);
    this->dst_width = dst_width;
    this->dst_height = dst_height;
    this->dst_format = dst_format;
}

void PixelFormator::format(const AVFrame *input_frame, AVFrame *output_frame)
{
    if (this->sws_ctx == nullptr ||
        (input_frame->format != this->sws_ctx->src_format ||
         input_frame->width != this->sws_ctx->src_w ||
         input_frame->height != this->sws_ctx->src_h))
    {
        sws_free_context(&this->sws_ctx);
        this->sws_ctx = sws_getCachedContext(this->sws_ctx,
                                             input_frame->width, input_frame->height, static_cast<AVPixelFormat>(input_frame->format),
                                             this->dst_width, this->dst_height, this->dst_format,
                                             SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (this->sws_ctx == nullptr)
        {
            throw FFmpegException("Failed to create sws context");
        }
    }

    output_frame->width = this->dst_width;
    output_frame->height = this->dst_height;
    output_frame->format = this->dst_format;

    int ret = sws_scale_frame(this->sws_ctx, output_frame, input_frame);
    if (ret < 0)
    {
        throw FFmpegException("Failed to scale frame");
    }
}

AVFramePtr PixelFormator::format(const AVFrame *input_frame)
{
    AVFramePtr output_frame = AVFramePtr(av_frame_alloc());
    format(input_frame, output_frame.get());
    return output_frame;
}

void format_frame(const AVFrame *input_frame, AVFrame *output_frame)
{
    SwsContext *sws_ctx = sws_getContext(input_frame->width, input_frame->height, static_cast<AVPixelFormat>(input_frame->format),
                                         output_frame->width, output_frame->height, static_cast<AVPixelFormat>(output_frame->format),
                                         SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (sws_ctx == nullptr)
    {
        throw FFmpegException("Failed to create sws context");
    }

    int result = sws_scale_frame(sws_ctx, output_frame, input_frame);
    if (result < 0)
    {
        sws_freeContext(sws_ctx);
        throw FFmpegException("Failed to scale frame");
    }

    sws_freeContext(sws_ctx);
}

AVFramePtr format_frame(const AVFrame *input_frame, int dst_width, int dst_height, AVPixelFormat dst_format)
{
    AVFramePtr output_frame = AVFramePtr(av_frame_alloc());
    output_frame->width = dst_width;
    output_frame->height = dst_height;
    output_frame->format = dst_format;

    format_frame(input_frame, output_frame.get());
    return output_frame;
}
