#pragma once

#ifndef FFMPEG_DEMUXER_H
#define FFMPEG_DEMUXER_H

#include <map>
#include <memory>
#include <string_view>
#include <thread>

#include "FormatContext.h"
#include "Logger.h"
#include "PacketQueue.h"

namespace Transcode
{

class Demuxer
{
public:
    Demuxer() = default;
    Demuxer(const Demuxer &) = delete;
    Demuxer &operator=(const Demuxer &) = delete;
    Demuxer(Demuxer &&) = delete;
    Demuxer &operator=(Demuxer &&) = delete;
    ~Demuxer();
    
public:
    void open(std::string_view file_path);
    void close();
    void start();
    void stop();

public:
    bool is_opened() const;
    bool is_running() const;
    int stream_count() const;
    AVStream *stream(int index);
    std::shared_ptr<PacketQueue> stream_queue(int index) const;

private:
    void demux_thread_func(std::stop_token token);

private:
    FormatContext format_ctx;
    Logger logger;
    std::map<int, std::shared_ptr<PacketQueue>> stream_queues;
    std::jthread demux_thread;
    volatile bool is_thread_running = false;
};

} // namespace Transcode

#endif // FFMPEG_DEMUXER_H
