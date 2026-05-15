#include "FormatContext.h"

#include <cassert>
#include <format>
#include <stdexcept>

#include "FFmpegException.h"
#include "Helper.h"

FormatContext::FormatContext(FormatContext &&other) noexcept
{
    this->raw_fmt_ctx = other.raw_fmt_ctx;
    other.raw_fmt_ctx = nullptr;
}

FormatContext &FormatContext::operator=(FormatContext &&other) noexcept
{
    assert(this != &other);
    this->close();
    this->raw_fmt_ctx = other.raw_fmt_ctx;
    other.raw_fmt_ctx = nullptr;
    return *this;
}

FormatContext::~FormatContext()
{
    this->close();
}

void FormatContext::open_in(std::string_view filename)
{
    assert(this->is_opened() == false);
    AVFormatContext *format_ctx = avformat_alloc_context();
    if (format_ctx == nullptr)
    {
        throw FFmpegException("Failed to allocate format context");
    }
    int ret = avformat_open_input(&format_ctx, filename.data(), nullptr, nullptr);
    if (ret < 0)
    {
        avformat_free_context(format_ctx);
        throw FFmpegException(ret, "Failed to open input file");
    }
    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0)
    {
        avformat_close_input(&format_ctx);
        throw FFmpegException(ret, "Failed to find stream info");
    }
    this->raw_fmt_ctx = format_ctx;
}

void FormatContext::open_out(std::string_view filename, std::string_view format)
{
    assert(this->is_opened() == false);
    const AVOutputFormat *output_format = av_guess_format(format.data(), nullptr, nullptr);
    int ret = avformat_alloc_output_context2(&this->raw_fmt_ctx, output_format, nullptr, filename.data());
    if (ret < 0)
    {
        throw FFmpegException(ret, "Failed to allocate output context");
    }
    if ((this->raw_fmt_ctx->oformat->flags & AVFMT_NOFILE) == 0)
    {
        int ret = avio_open(&this->raw_fmt_ctx->pb, filename.data(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            avformat_free_context(this->raw_fmt_ctx);
            this->raw_fmt_ctx = nullptr;
            throw FFmpegException(ret, "Failed to open output file");
        }
    }
}

void FormatContext::close()
{
    avformat_close_input(&this->raw_fmt_ctx);
}

const AVFormatContext *FormatContext::get() const
{
    return this->raw_fmt_ctx;
}

AVFormatContext *FormatContext::get()
{
    return this->raw_fmt_ctx;
}

int FormatContext::stream_count() const
{
    assert(this->raw_fmt_ctx != nullptr);
    return this->raw_fmt_ctx->nb_streams;
}

const AVStream *FormatContext::stream(int index) const
{
    assert(this->raw_fmt_ctx != nullptr);
    return this->raw_fmt_ctx->streams[index];
}

AVStream *FormatContext::stream(int index)
{
    assert(this->raw_fmt_ctx != nullptr);
    return this->raw_fmt_ctx->streams[index];
}

AVStream *FormatContext::add_stream(const AVCodecContext *codec)
{
    assert(this->is_opened() == true);
    assert(this->raw_fmt_ctx->oformat != nullptr);

    AVStream *out_stream = avformat_new_stream(this->raw_fmt_ctx, nullptr);
    if (out_stream == nullptr)
    {
        throw FFmpegException("Failed to create output stream");
    }

    int ret = avcodec_parameters_from_context(out_stream->codecpar, codec);
    if (ret < 0)
    {
        throw FFmpegException(ret, "Failed to copy encoder parameters to output stream");
    }

    out_stream->time_base = codec->time_base;
    return out_stream;
}

int FormatContext::best_stream_index(AVMediaType type) const
{
    assert(this->raw_fmt_ctx != nullptr);
    return av_find_best_stream(this->raw_fmt_ctx, type, -1, -1, nullptr, 0);
}

void FormatContext::write_header()
{
    assert(this->is_opened() == true);
    assert(this->raw_fmt_ctx->oformat != nullptr);
    int ret = avformat_write_header(this->raw_fmt_ctx, nullptr);
    if (ret < 0)
    {
        throw FFmpegException(ret, "Failed to write header");
    }
}

void FormatContext::write_trailer()
{
    assert(this->is_opened() == true);
    assert(this->raw_fmt_ctx->oformat != nullptr);
    int ret = av_write_trailer(this->raw_fmt_ctx);
    if (ret < 0)
    {
        throw FFmpegException(ret, "Failed to write trailer");
    }
}

bool FormatContext::is_opened() const
{
    return this->raw_fmt_ctx != nullptr;
}
