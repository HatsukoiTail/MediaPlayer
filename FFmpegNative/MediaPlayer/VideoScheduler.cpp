#include "VideoScheduler.h"

#include <cassert>
#include <format>

extern "C"
{
#include <libavutil/time.h>
}

namespace MediaPlayer
{

VideoScheduler::~VideoScheduler()
{
    this->stop();
}

void VideoScheduler::set_source_queue(std::shared_ptr<FrameQueue> queue)
{
    this->source_queue = std::move(queue);
}

void VideoScheduler::set_renderer(IVideoRenderer *r)
{
    this->renderer = r;
}

void VideoScheduler::set_master_clock(std::function<double()> fn)
{
    this->master_clock_fn = std::move(fn);
}

void VideoScheduler::set_queue_serial(const int *serial_ptr)
{
    this->queue_serial_ptr = serial_ptr;
    this->vidclk.set_queue_serial(serial_ptr);
}

void VideoScheduler::start()
{
    assert(this->source_queue);
    assert(this->renderer);
    assert(this->master_clock_fn);

    this->is_thread_running = true;
    this->thread = std::jthread([this](std::stop_token token) { this->run(token); });
}

void VideoScheduler::stop()
{
    if (!this->thread.joinable())
        return;
    this->thread.request_stop();
    this->thread.join();
    this->is_thread_running = false;
}

void VideoScheduler::flush()
{
    this->frame_timer = 0.0;
    this->last_serial = -1;
}

bool VideoScheduler::is_running() const
{
    return this->is_thread_running.load();
}

// ===========================================================================

static double now_sec()
{
    return av_gettime_relative() / 1000000.0;
}

void VideoScheduler::run(std::stop_token token)
{
    this->logger.open("../log/video_scheduler.log");
    this->logger.log("VideoScheduler started");

    while (!token.stop_requested())
    {
        if (this->source_queue->size() == 0)
        {
            // 无帧，短暂休眠后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        auto current = this->source_queue->peek();
        if (current.is_null())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        AVRational tb = current.get()->time_base;
        if (tb.num == 0 || tb.den == 0)
            tb = {1, 90000}; // fallback

        int current_serial = this->queue_serial_ptr ? *this->queue_serial_ptr : 0;

        // 检测 seek 后第一帧（frame_timer 由 flush 置零）
        if (this->frame_timer == 0.0 || this->last_serial != current_serial)
        {
            this->frame_timer = now_sec();
            this->last_serial = current_serial;
        }

        // 帧间隔：优先用下一帧的 PTS 差
        double duration = 0.04; // 默认 25fps
        auto next = this->source_queue->peek_next();
        if (!next.is_null())
        {
            double pts_cur  = current.pts() * av_q2d(tb);
            double pts_next = next.pts() * av_q2d(tb);
            double delta = pts_next - pts_cur;
            if (delta > 0 && delta < NOSYNC_THRESHOLD)
                duration = delta;
        }

        double delay = this->compute_target_delay(duration);

        double time_now = now_sec();
        double target_time = this->frame_timer + delay;

        if (time_now < target_time)
        {
            // 还没到渲染时刻
            double remaining = target_time - time_now;
            if (remaining > 0.001)
            {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(remaining * 0.5)); // 减半休眠，避免过度
            }
            continue;
        }

        // 出队并渲染
        this->source_queue->dequeue();
        this->renderer->present(current.get());

        this->vidclk.set(current.pts() * av_q2d(tb), this->last_serial);
        this->frame_timer = now_sec();

        // 限制 frame_timer 不落后太久
        if (this->frame_timer < time_now - NOSYNC_THRESHOLD)
            this->frame_timer = time_now - NOSYNC_THRESHOLD;
    }

    this->logger.log("VideoScheduler stopped");
    this->is_thread_running = false;
}

double VideoScheduler::compute_target_delay(double delay) const
{
    double master = this->master_clock_fn();
    if (std::isnan(master))
        return delay;

    double diff = this->vidclk.get() - master;
    if (std::isnan(diff) || std::fabs(diff) >= NOSYNC_THRESHOLD)
        return delay;

    double threshold = std::max(SYNC_THRESHOLD_MIN, std::min(SYNC_THRESHOLD_MAX, delay));

    if (diff <= -threshold)
    {
        // 视频落后 → 缩短延迟（赶进度）
        delay = std::max(0.0, delay + diff);
    }
    else if (diff >= threshold)
    {
        // 视频超前
        if (delay > FRAMEDUP_THRESHOLD)
            delay = delay + diff;      // 延长等待
        else
            delay = 2.0 * delay;       // 重复帧（加倍延迟）
    }
    // else: diff 在阈值内，不调整

    return delay;
}

} // namespace MediaPlayer
