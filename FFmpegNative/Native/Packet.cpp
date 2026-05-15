#include "Packet.h"

#include <cassert>

#include "FFmpegException.h"

Packet::Packet(Packet &&other) noexcept
{
    this->raw_packet_ptr = other.raw_packet_ptr;
    other.raw_packet_ptr = nullptr;
}

Packet &Packet::operator=(Packet &&other) noexcept
{
    assert(this != &other);
    av_packet_free(&this->raw_packet_ptr);
    this->raw_packet_ptr = other.raw_packet_ptr;
    other.raw_packet_ptr = nullptr;
    return *this;
}

Packet::~Packet()
{
    av_packet_free(&this->raw_packet_ptr);
}

Packet Packet::create()
{
    auto ptr = av_packet_alloc();
    if (ptr == nullptr)
    {
        throw FFmpegException("Failed to allocate AVPacket");
    }
    Packet result;
    result.raw_packet_ptr = ptr;
    return result;
}

AVPacket *Packet::get()
{
    return this->raw_packet_ptr;
}

const AVPacket *Packet::get() const
{
    return this->raw_packet_ptr;
}

void Packet::reset(AVPacket *ptr) noexcept
{
    if (this->raw_packet_ptr != nullptr)
    {
        av_packet_free(&this->raw_packet_ptr);
    }
    this->raw_packet_ptr = ptr;
}

void Packet::unref()
{
    assert(this->raw_packet_ptr != nullptr);
    av_packet_unref(this->raw_packet_ptr);
}

bool Packet::is_null() const
{
    return this->raw_packet_ptr != nullptr;
}

int Packet::stream_index() const
{
    assert(this->raw_packet_ptr != nullptr);
    return this->raw_packet_ptr->stream_index;
}

int Packet::size() const
{
    assert(this->raw_packet_ptr != nullptr);
    return this->raw_packet_ptr->size;
}

int Packet::serial() const
{
    assert(this->raw_packet_ptr != nullptr);
    return this->packet_serial;
}

double Packet::duration() const
{
    assert(this->raw_packet_ptr != nullptr);
    return this->packet_duration;
}

void Packet::set_serial(int serial) noexcept
{
    this->packet_serial = serial;
}

void Packet::set_duration(double duration) noexcept
{
    this->packet_duration = duration;
}