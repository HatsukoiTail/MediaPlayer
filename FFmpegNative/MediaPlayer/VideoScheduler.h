#pragma once

#ifndef MEDIAPLAYER_VIDEOSCHEDULER_H
#define MEDIAPLAYER_VIDEOSCHEDULER_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "Clock.h"
#include "FrameQueue.h"
#include "IVideoRenderer.h"
#include "Logger.h"

namespace MediaPlayer
{

/// 视频调度器。
/// 独立线程：从滤镜输出队列取帧 → 以主时钟为准做音视频同步 → present/drop/sleep。
class VideoScheduler
{
public:
    static constexpr double SYNC_THRESHOLD_MIN  = 0.04;   // 40ms 内不修正
    static constexpr double SYNC_THRESHOLD_MAX  = 0.1;    // 超过 100ms 必须修正
    static constexpr double FRAMEDUP_THRESHOLD  = 0.1;
    static constexpr double NOSYNC_THRESHOLD    = 10.0;

    VideoScheduler() = default;
    ~VideoScheduler();

    void set_source_queue(std::shared_ptr<FrameQueue> queue);
    void set_renderer(IVideoRenderer *renderer);
    void set_master_clock(std::function<double()> fn);
    void set_queue_serial(const int *serial_ptr);

    void start();
    void stop();
    void flush();

    bool is_running() const;

    Clock vidclk;   // 视频时钟（外部可读）

private:
    void run(std::stop_token token);
    double compute_target_delay(double delay) const;

    std::shared_ptr<FrameQueue> source_queue;
    IVideoRenderer *renderer = nullptr;
    std::function<double()> master_clock_fn;

    const int *queue_serial_ptr = nullptr;
    std::jthread thread;
    std::atomic<bool> is_thread_running{false};

    double frame_timer = 0.0;
    int last_serial = -1;
    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_VIDEOSCHEDULER_H
