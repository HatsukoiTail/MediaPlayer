#pragma once

#ifndef FFMPEG_FRAMEQUEUE_H
#define FFMPEG_FRAMEQUEUE_H

#include <condition_variable>
#include <deque>
#include <mutex>

#include "Frame.h"

class FrameQueue
{
public:
    FrameQueue() = default;
    FrameQueue(int max_count);
    ~FrameQueue();

public:
    bool enqueue(Frame frame);
    bool try_enqueue(Frame frame);
    Frame peek();
    Frame peek_next();
    Frame dequeue();
    Frame try_dequeue();
    void flush();
    void start();
    void stop();
    void awake();
    size_t size() const;

private:
    std::deque<Frame> queue;
    mutable std::mutex mutex;
    std::condition_variable condition;
    int max_count = 15;
    bool stop_request = true;
};

#endif // FFMPEG_FRAMEQUEUE_H