#include "FrameLoader.h"

#include "FFmpegException.h"
#include "Packet.h"

FrameLoader::FrameLoader(std::string_view url)
{
    this->open(url);
}

bool FrameLoader::is_valid() const
{
    return this->format_ctx.is_opened();
}

void FrameLoader::seek(double position)
{
    if (position < 0)
        return;

    auto fmt_ctx = this->format_ctx.get();
    if (fmt_ctx->pb == nullptr || (fmt_ctx->pb->seekable & AVIO_SEEKABLE_NORMAL) == 0)
        return;

    int64_t timestamp = static_cast<int64_t>(position * AV_TIME_BASE);
    int ret = avformat_seek_file(fmt_ctx, -1, INT64_MIN, timestamp, INT64_MAX, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
    {
        throw FFmpegException(ret, "Failed to seek file");
    }
    avcodec_flush_buffers (this->codec_ctx.get());
}

AVFramePtr FrameLoader::load_frame(std::stop_token stop_token)
{
    auto frame = AVFramePtr(av_frame_alloc());
    while (!stop_token.stop_requested())
    {
        int result = avcodec_receive_frame(this->codec_ctx.get(), frame.get());
        if (result >= 0)
        {
            return frame;
        }
        else if (result != AVERROR(EAGAIN) && result != AVERROR_EOF)
        {
            throw FFmpegException(result, "Failed to receive frame");
        }

        while (!stop_token.stop_requested())
        {
            result = av_read_frame(this->format_ctx.get(), this->packet.get());
            if (result == AVERROR_EOF)
            {
                return nullptr;
            }
            else if (result < 0)
            {
                throw FFmpegException(result, "Failed to read frame");
            }

            if (this->packet->stream_index != this->video_stream_index)
            {
                av_packet_unref(this->packet.get());
                continue;
            }
            break;
        }

        result = avcodec_send_packet(this->codec_ctx.get(), this->packet.get());
        av_packet_unref(this->packet.get());

        if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF)
        {
            throw FFmpegException(result, "Failed to send packet");
        }
    }
    return nullptr;
}

AVFramePtr FrameLoader::load_frame(std::stop_token cancel_token, std::string_view url, double position)
{
    FrameLoader loader(url);
    if (!loader.is_valid())
    {
        return nullptr;
    }
    loader.seek(position);
    return loader.load_frame(cancel_token);
}

void FrameLoader::open(std::string_view url)
{
    this->format_ctx.open_in(url);
    this->video_stream_index = this->format_ctx.best_stream_index(AVMEDIA_TYPE_VIDEO);
    if (this->video_stream_index < 0)
    {
        this->format_ctx.close();
        return;
    }
    open_decode_context(this->format_ctx.stream(this->video_stream_index), nullptr);
    this->packet = AVPacketPtr(av_packet_alloc());
}
