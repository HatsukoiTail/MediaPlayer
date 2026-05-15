#include "ImageLoader.h"

#include "FFmpegException.h"
#include "Helper.h"

ImageLoader::ImageLoader(ImageLoader &&) noexcept = default;
ImageLoader &ImageLoader::operator=(ImageLoader &&) noexcept = default;
ImageLoader::~ImageLoader() = default;

void ImageLoader::open(std::string_view url)
{
    AVFormatContext *raw_fmt_ctx = avformat_alloc_context();
    if (raw_fmt_ctx == nullptr)
    {
        throw FFmpegException("Failed to allocate format context");
    }

    int ret = avformat_open_input(&raw_fmt_ctx, url.data(), nullptr, nullptr);
    if (ret < 0)
    {
        avformat_free_context(raw_fmt_ctx);
        throw FFmpegException(ret, "Failed to open input file");
    }
    this->format_ctx = AVFormatContextPtr(raw_fmt_ctx);

    this->video_stream_index = av_find_best_stream(raw_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (this->video_stream_index < 0)
    {
        throw FFmpegException("No video stream found");
    }

    this->codec_ctx = open_codec_context(raw_fmt_ctx->streams[this->video_stream_index]);
    this->image_width = this->codec_ctx->width;
    this->image_height = this->codec_ctx->height;
    this->packet = AVPacketPtr(av_packet_alloc());
}

AVFramePtr ImageLoader::load_frame()
{
    if (this->format_ctx == nullptr || this->frame_loaded)
        return nullptr;

    int ret = av_read_frame(this->format_ctx.get(), this->packet.get());
    if (ret < 0)
        return nullptr;

    if (this->packet->stream_index != this->video_stream_index)
    {
        av_packet_unref(this->packet.get());
        return nullptr;
    }

    ret = avcodec_send_packet(this->codec_ctx.get(), this->packet.get());
    av_packet_unref(this->packet.get());
    if (ret < 0)
        return nullptr;

    auto frame = AVFramePtr(av_frame_alloc());
    if (frame == nullptr)
        return nullptr;

    ret = avcodec_receive_frame(this->codec_ctx.get(), frame.get());
    if (ret < 0)
        return nullptr;

    this->frame_loaded = true;
    return frame;
}

bool ImageLoader::is_valid() const
{
    return this->format_ctx != nullptr;
}

int ImageLoader::width() const
{
    return this->image_width;
}

int ImageLoader::height() const
{
    return this->image_height;
}

AVFramePtr ImageLoader::load_frame(std::string_view url)
{
    ImageLoader loader;
    loader.open(url);
    return loader.load_frame();
}

AVFramePtr load_image(std::string_view url)
{
    auto raw_format_ctx = avformat_alloc_context();
    if (raw_format_ctx == nullptr)
    {
        throw FFmpegException("Failed to allocate format context");
    }

    int ret = avformat_open_input(&raw_format_ctx, url.data(), nullptr, nullptr);
    if (ret < 0)
    {
        avformat_free_context(raw_format_ctx);
        throw FFmpegException(ret, "Failed to open input file");
    }

    AVFormatContextPtr format_ctx(raw_format_ctx);

    int video_stream_index = av_find_best_stream(format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0)
        return nullptr;

    auto codec_ctx = open_codec_context(format_ctx->streams[video_stream_index]);

    auto packet = AVPacketPtr(av_packet_alloc());
    if (packet == nullptr)
        return nullptr;

    ret = av_read_frame(format_ctx.get(), packet.get());
    if (ret < 0)
        return nullptr;

    if (packet->stream_index != video_stream_index)
    {
        return nullptr;
    }

    ret = avcodec_send_packet(codec_ctx.get(), packet.get());
    if (ret < 0)
        return nullptr;

    auto frame = AVFramePtr(av_frame_alloc());
    if (frame == nullptr)
        return nullptr;

    ret = avcodec_receive_frame(codec_ctx.get(), frame.get());
    if (ret < 0)
        return nullptr;

    return frame;
}
