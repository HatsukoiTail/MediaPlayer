#pragma once

#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <atomic>
#include <memory>
#include <string>

extern "C"
{
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;

#include "AudioFilter.h"
#include "AudioOutput.h"
#include "Clock.h"
#include "Decoder.h"
#include "Demuxer.h"
#include "IAudioRenderer.h"
#include "IVideoRenderer.h"
#include "VideoFilter.h"
#include "VideoScheduler.h"

namespace MediaPlayer
{

class MediaPlayer
{
public:
    enum State { Stopped, Playing, Paused };

    MediaPlayer();
    ~MediaPlayer();

    /// 打开媒体文件，自动选择最佳音视频流
    void open(const char *file_path);

    /// 注入渲染器（必须在 open 之前调用）
    void set_video_renderer(IVideoRenderer *renderer);
    void set_audio_renderer(IAudioRenderer *renderer);

    /// 注入硬件设备（可选，用于硬件解码）
    void set_hw_device(AVBufferRef *device);

    /// 播放控制
    void play();
    void pause();
    void stop();
    void seek(double seconds);

    /// 倍速（内部同步适配）
    void set_speed(double speed);
    void set_volume(double vol);

    /// 状态
    State state() const;
    double duration() const;
    double position() const;

    /// 流信息与选择
    std::vector<StreamInfo> video_streams() const;
    std::vector<StreamInfo> audio_streams() const;
    int  current_video_stream() const;
    int  current_audio_stream() const;
    void select_stream(int index);  // 自动判断音视频类型

private:
    void build_pipeline();
    void stop_pipeline();
    void flush_pipeline();

    Demuxer demuxer;

    // --- 视频管线 ---
    std::unique_ptr<Decoder>      video_decoder;
    std::unique_ptr<VideoFilter>  video_filter;
    VideoScheduler                video_scheduler;
    IVideoRenderer               *video_renderer = nullptr;
    int                           video_stream_index = -1;

    // --- 音频管线 ---
    std::unique_ptr<Decoder>      audio_decoder;
    std::unique_ptr<AudioFilter>  audio_filter;
    AudioOutput                   audio_output;
    IAudioRenderer               *audio_renderer = nullptr;
    int                           audio_stream_index = -1;

    // --- 同步 ---
    int    queue_serial = 0;
    Clock  audclk;
    Clock  vidclk;
    Clock  extclk;

    // --- 状态 ---
    AVBufferRef *hw_device = nullptr;  // borrowed
    double speed = 1.0;
    State state_ = Stopped;
    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_H
