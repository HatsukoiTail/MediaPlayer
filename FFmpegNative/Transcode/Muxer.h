#pragma once

#ifndef FFMPEG_MUXER_H
#define FFMPEG_MUXER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct AVCodecContext;
struct AVFormatContext;
struct AVStream;

#include "FormatContext.h"
#include "Logger.h"
#include "PacketQueue.h"

namespace Transcode
{

class Muxer
{
public:
    Muxer() = default;
    Muxer(const Muxer &) = delete;
    Muxer &operator=(const Muxer &) = delete;
    Muxer(Muxer &&) = delete;
    Muxer &operator=(Muxer &&) = delete;
    ~Muxer();

public:
    void open(std::string_view output_file, std::string_view format);
    void close();
    AVStream *add_stream(const AVCodecContext *enc_ctx);
    void set_input_queue(int stream_index, std::shared_ptr<PacketQueue> queue);
    void set_metadata(const std::map<std::string, std::string> &meta);
    void set_cover(const std::vector<uint8_t> &data);
    void start();
    void stop();
    bool eof() const;
    double progress() const;

public:
    bool is_opened() const;
    bool is_running() const;

private:
    void mux_thread_func(std::stop_token token);
    void write_packet(AVPacket *packet);
    bool all_queues_drained() const;

private:
    FormatContext format_ctx;
    std::map<int, std::shared_ptr<PacketQueue>> input_queues;
    std::jthread mux_thread;
    volatile bool is_thread_running = false;
    bool header_written = false;
    volatile double current_progress = 0.0;
    volatile bool has_eof = false;
    std::map<std::string, std::string> metadata;
    std::vector<uint8_t> cover_data;
    Logger logger;
};

}

#endif // FFMPEG_MUXER_H
