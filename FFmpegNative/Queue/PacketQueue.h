#pragma once

#ifndef FFMPEG_PACKETQUEUE_H
#define FFMPEG_PACKETQUEUE_H

#include <condition_variable>
#include <deque>

#include "Packet.h"

class PacketQueue
{
public:
    PacketQueue();
    explicit PacketQueue(int max_size);
    ~PacketQueue();

public:
    bool enqueue(Packet packet);
    bool try_enqueue(Packet packet);
    Packet dequeue();
    bool try_dequeue(Packet &packet);
    void flush();
    void start();
    void stop();

public:
    bool is_stopped() const;
    int serial() const;
    int count() const;
    int size() const;
    int64_t duration() const;
    int max_size() const;

private:
    std::deque<Packet> queue;
    mutable std::mutex mutex;
    std::condition_variable cv;

private:
    int queue_size = 0;
    int64_t queue_duration = 0;
    int queue_serial = 0;
    int max_queue_size;
    bool stop_request = true;
};


#endif //FFMPEG_PACKETQUEUE_H