#include "VideoDecoder.h"

#include "Factory.h"
#include "Print.h"
#include "Util.h"

VideoDecoder::VideoDecoder(std::shared_ptr<Queue<AVPacketPointer> > packets)
    : Decoder{packets}
{

}

VideoDecoder::~VideoDecoder()
{
    this->close();
    print("VideoDecoder delete!");
}

void VideoDecoder::set_format(const VideoFormat &format)
{
    assert(this->state() != Decoder::State::Closed);
    this->dst_format = format;
}

VideoFormat VideoDecoder::default_format() const
{
    assert(this->state() != Decoder::State::Closed);
    return {
        .width = width(),
        .height = height(),
        .pixelFormat = from_ff_format(pixel_format())
    };
}

void VideoDecoder::close()
{
    Decoder::close();
    this->sws_ctx.reset();
}

AVFramePointer VideoDecoder::process(AVFramePointer frame)
{
    const bool is_need_scale = (to_ff_format(this->dst_format.pixelFormat) != frame->format) ||
                               (this->dst_format.width != frame->width) ||
                               (this->dst_format.height != frame->height);
    if (this->dst_format.isValid() && is_need_scale)
    {
        const bool need_new_sws = !this->sws_ctx ||
                                  (frame->width != this->src_format.width ||
                                   frame->height != this->src_format.height ||
                                   frame->format != to_ff_format(this->src_format.pixelFormat));

        if (need_new_sws)
        {
            this->src_format.width = frame->width;
            this->src_format.height = frame->height;
            this->src_format.pixelFormat = from_ff_format(static_cast<AVPixelFormat>(frame->format));

            this->sws_ctx = SwsContextPointer(open_sws_context(this->src_format.width, this->src_format.height, to_ff_format(this->src_format.pixelFormat),
                                                               this->dst_format.width, this->dst_format.height, to_ff_format(this->dst_format.pixelFormat)));
        }

        AVFramePointer sws_frame = AVFramePointer(av_frame_alloc());
        sws_frame->width = this->dst_format.width;
        sws_frame->height = this->dst_format.height;
        sws_frame->format = to_ff_format(this->dst_format.pixelFormat);

        int result = sws_scale_frame(this->sws_ctx.get(), sws_frame.get(), frame.get());
        if (result < 0)
        {
            print(Ansi::Red, "Fail to scale video frame, {}", debug(result));
            return nullptr;
        }

        sws_frame->pts = frame->pts;
        return sws_frame;
    }
    return frame;
}
