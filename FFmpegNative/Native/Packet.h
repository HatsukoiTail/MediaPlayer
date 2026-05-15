#pragma once

#ifndef FFMPEG_PACKET_H
#define FFMPEG_PACKET_H

#include <memory>

extern "C"
{
#include <libavcodec/packet.h>
}

struct AVPacketDeleter
{
    void operator()(AVPacket *ptr) const
    {
        av_packet_free(&ptr);
    }
};

using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

class Packet
{
public:
    Packet() = default;
    Packet(const Packet &) = delete;
    Packet &operator=(const Packet &) = delete;
    Packet(Packet &&other) noexcept;
    Packet &operator=(Packet &&other) noexcept;
    ~Packet();
    static Packet create();

public:
    AVPacket* get();
    const AVPacket* get() const;
    void reset(AVPacket* ptr = nullptr) noexcept;
    void unref();

public:
    bool is_null() const;
    int stream_index() const;
    int size() const;
    int serial() const;
    double duration() const;

public:
    void set_serial(int serial) noexcept;
    void set_duration(double duration) noexcept;

private:
    AVPacket *raw_packet_ptr = nullptr;
    int packet_serial = -1;
    double packet_duration = 0.0;
};

#endif // FFMPEG_PACKET_H