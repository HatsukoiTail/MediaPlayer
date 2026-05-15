#pragma once

#ifndef MEDIAPLAYER_CLOCK_H
#define MEDIAPLAYER_CLOCK_H

#include <cmath>
#include <chrono>

namespace MediaPlayer
{

/// 基于 ffplay 的时钟实现。
/// 核心思想：存储 pts - wall_time 的漂移量，读取时恢复当前 pts。
/// 支持暂停、变速、串号（seek 后自动淘汰旧时钟值）。
class Clock
{
public:
    Clock() = default;

    /// 重置为 NAN（未初始化状态）。
    void reset()
    {
        this->pts         = NAN;
        this->pts_drift   = NAN;
        this->last_updated = 0.0;
        this->speed       = 1.0;
        this->serial      = -1;
        this->paused      = false;
        this->queue_serial_ptr = nullptr;
    }

    /// 绑定外部 queue serial，用于 seek 淘汰检测。
    void set_queue_serial(const int *serial_ptr)
    {
        this->queue_serial_ptr = serial_ptr;
    }

    /// 设置当前时钟值（自动记录漂移量）。
    void set(double new_pts, int new_serial)
    {
        this->pts          = new_pts;
        this->last_updated = now();
        this->pts_drift    = new_pts - this->last_updated;
        this->serial       = new_serial;
    }

    /// 读取当前时钟值。
    /// 如果时钟串号已过期（queue serial 变化）返回 NAN。
    double get() const
    {
        if (this->queue_serial_ptr && this->serial != *this->queue_serial_ptr)
            return NAN;
        if (this->paused)
            return this->pts;
        return this->pts_drift + (now() - this->last_updated) * this->speed;
    }

    /// 变速（不影响已流逝的时间）。
    void set_speed(double new_speed)
    {
        if (new_speed <= 0.0) return;
        // 先以旧速度读出当前值，再以新速度写入
        set(this->get(), this->serial);
        this->speed = new_speed;
    }

    void pause()   { this->paused = true; }
    void resume()  { this->paused = false; this->last_updated = now(); }

    double get_speed() const { return this->speed; }
    int    get_serial() const { return this->serial; }

private:
    static double now()
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    double pts         = NAN;
    double pts_drift   = NAN;
    double last_updated = 0.0;
    double speed       = 1.0;
    int    serial      = -1;
    bool   paused      = false;
    const int *queue_serial_ptr = nullptr;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_CLOCK_H
