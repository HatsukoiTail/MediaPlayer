#pragma once

#ifndef FFMPEG_FORMATCONTEXT
#define FFMPEG_FORMATCONTEXT

#include <memory>
#include <string_view>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct AVIOContextDeleter
{
    void operator()(AVIOContext *ptr) const
    {
        avio_context_free(&ptr);
    }
};
struct AVFormatContextDeleter
{
    void operator()(AVFormatContext *ptr) const
    {
        if (ptr == nullptr)
            return;
        if (ptr->iformat == nullptr)
        {
            avformat_free_context(ptr);
            return;
        }
        avformat_close_input(&ptr);
    }
};
using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

class FormatContext
{
public:
    FormatContext() = default;
    FormatContext(const FormatContext &) = delete;
    FormatContext &operator=(const FormatContext &) = delete;
    FormatContext(FormatContext &&) noexcept;
    FormatContext &operator=(FormatContext &&) noexcept;
    ~FormatContext();

public:
    void open_in(std::string_view filename);
    void open_out(std::string_view filename, std::string_view format);
    void close();
    const AVFormatContext *get() const;
    AVFormatContext *get();
    int stream_count() const;
    const AVStream *stream(int index) const;
    AVStream *stream(int index);
    AVStream *add_stream(const AVCodecContext *codec);
    int best_stream_index(AVMediaType type) const;
    void write_header();
    void write_trailer();

public:
    bool is_opened() const;

private:
    AVFormatContext *raw_fmt_ctx = nullptr;
};

#endif // FFMPEG_FORMATCONTEXT