#ifndef VIDEORENDER_H
#define VIDEORENDER_H

#include "DataModel.h"
#include "SmartStruct.h"

#include <functional>
#include <thread>

class VideoRender
{
public:
    enum class State { Stopped, Paused, Running };
public:
    VideoRender(std::shared_ptr<Queue<AVFramePointer>> frames);
    ~VideoRender();

public:
    void set_callback(std::function<void(AVFramePointer)> callback);
    void set_clock(std::function<int64_t()> sync_clock);

public:
    void run();
    void stop();
    void pause();
    void set_speed(double speed);

public:
    State state() const;
    int64_t clock() const;

private:
    void self_sync();
    void external_sync();

private:
    std::function<int64_t()> external_clock; // 外部时钟
    std::atomic<int64_t> video_time {-1}; // 视频时钟
    std::atomic<double> play_speed {1.0}; // 播放速度

private:
    std::atomic<State> render_state {State::Stopped};

private:
    std::thread thread;
    std::shared_ptr<Queue<AVFramePointer>> frames;
    std::function<void(AVFramePointer)> callback; // 外部渲染回调
};

#endif // VIDEORENDER_H
