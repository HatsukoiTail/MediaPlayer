#pragma once

#ifndef MEDIAPLAYER_DEMUXER_H
#define MEDIAPLAYER_DEMUXER_H

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "FormatContext.h"
#include "Logger.h"
#include "PacketQueue.h"

namespace MediaPlayer
{

struct StreamInfo
{
    int index = -1;
    AVMediaType type = AVMEDIA_TYPE_UNKNOWN;
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    std::string codec_name;
    AVRational time_base{};
};

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
    // 播放控制
    void seek(double seconds);
    void flush();

    // 媒体信息
    double duration() const;
    double position() const;
    std::vector<StreamInfo> streams() const;
    std::vector<StreamInfo> streams(AVMediaType type) const;
    AVStream *stream(int index);

    // 活跃流管理（不活跃的流丢弃包，节省内存）
    void set_stream_active(int index, bool active);
    bool is_stream_active(int index) const;
    std::shared_ptr<PacketQueue> stream_queue(int index) const;

    // 状态
    bool is_opened() const;
    bool is_running() const;

private:
    void demux_thread_func(std::stop_token token);

private:
    FormatContext format_ctx;
    std::vector<StreamInfo> stream_info;
    std::map<int, std::shared_ptr<PacketQueue>> stream_queues;
    std::set<int> active_streams;
    std::jthread demux_thread;
    std::atomic<bool> is_thread_running = false;
    std::atomic<double> current_position{0.0};
    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_DEMUXER_H
