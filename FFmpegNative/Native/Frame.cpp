#include "Frame.h"

#include <cassert>

#include "FFmpegException.h"

Frame::Frame(AVFrame *ptr)
{
    this->raw_frame_ptr = ptr;
}

Frame::Frame(Frame &&other) noexcept
{
    this->raw_frame_ptr = other.raw_frame_ptr;
    other.raw_frame_ptr = nullptr;
}

Frame &Frame::operator=(Frame &&other) noexcept
{
    assert(this != &other);
    av_frame_free(&this->raw_frame_ptr);
    this->raw_frame_ptr = other.raw_frame_ptr;
    other.raw_frame_ptr = nullptr;
    return *this;
}

Frame::~Frame()
{
    av_frame_free(&this->raw_frame_ptr);
}

Frame Frame::create()
{
    auto raw_ptr = av_frame_alloc();
    if (raw_ptr == nullptr)
    {
        throw FFmpegException("Failed to allocate frame");
    }
    auto frame = Frame();
    frame.raw_frame_ptr = raw_ptr;
    return frame;
}

Frame Frame::ref(const Frame &frame)
{
    auto result = Frame::create();
    av_frame_ref(result.get(), frame.get());
    return result;
}

AVFrame *Frame::get()
{
    return this->raw_frame_ptr;
}

const AVFrame *Frame::get() const
{
    return this->raw_frame_ptr;
}

void Frame::reset(AVFrame *ptr) noexcept
{
    if (this->raw_frame_ptr != nullptr)
    {
        av_frame_free(&this->raw_frame_ptr);
    }
    this->raw_frame_ptr = ptr;
}

bool Frame::is_null() const
{
    return this->raw_frame_ptr != nullptr;
}

int64_t Frame::pts() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->pts;
}

int Frame::format() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->format;
}

int Frame::width() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->width;
}

int Frame::height() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->height;
}

int Frame::sample_rate() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->sample_rate;
}

int Frame::sample_count() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->nb_samples;
}

int Frame::channel_count() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->ch_layout.nb_channels;
}

uint8_t **Frame::data()
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->data;
}

const uint8_t *Frame::data(int index) const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->data[index];
}

uint8_t *Frame::data(int index)
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->data[index];
}

const int *Frame::linesize() const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->linesize;
}

int *Frame::linesize()
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->linesize;
}

int Frame::linesize(int index) const
{
    assert(this->raw_frame_ptr != nullptr);
    return this->raw_frame_ptr->linesize[index];
}