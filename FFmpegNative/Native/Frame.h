#pragma once

#ifndef FFMPEG_FRAME_H
#define FFMPEG_FRAME_H

#include <memory>

extern "C"
{
#include <libavutil/frame.h>
};

struct AVFrameDeleter
{
    void operator()(AVFrame *ptr) const
    {
        av_frame_free(&ptr);
    }
};

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

class Frame
{
public:
    Frame() = default;
    Frame(AVFrame *ptr);
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&other) noexcept;
    Frame &operator=(Frame &&other) noexcept;
    ~Frame();
    static Frame create();
    static Frame ref(const Frame &frame);

public:
    AVFrame *get();
    const AVFrame *get() const;
    void reset(AVFrame *ptr = nullptr) noexcept;

public:
    bool is_null() const;
    int64_t pts() const;
    int format() const;
    int width() const;
    int height() const;
    int sample_rate() const;
    int sample_count() const;
    int channel_count() const;
    uint8_t** data();
    const uint8_t* data(int index) const;
    uint8_t* data(int index);
    const int* linesize() const;
    int* linesize();
    int linesize(int index) const;

private:
    AVFrame *raw_frame_ptr = nullptr;
};

#endif // FFMPEG_FRAME_H