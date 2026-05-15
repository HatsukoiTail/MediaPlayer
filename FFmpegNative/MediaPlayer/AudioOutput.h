#pragma once

#ifndef MEDIAPLAYER_AUDIOOUTPUT_H
#define MEDIAPLAYER_AUDIOOUTPUT_H

#include <atomic>
#include <memory>

extern "C"
{
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

#include "Clock.h"
#include "FrameQueue.h"
#include "IAudioRenderer.h"
#include "Logger.h"

namespace MediaPlayer
{

/// 音频输出 / 音频调度器。
/// 不创建独立线程——全部工作在 IAudioRenderer 的回调中完成。
/// 负责：从滤镜队列取帧 → swresample → 推入音频硬件 → 更新时钟。
class AudioOutput
{
public:
    AudioOutput() = default;
    ~AudioOutput();

    void set_source_queue(std::shared_ptr<FrameQueue> queue);
    void set_renderer(IAudioRenderer *renderer);
    void set_queue_serial(const int *serial_ptr);

    bool open(int sample_rate, AVSampleFormat fmt, const AVChannelLayout &layout);
    void close();
    void pause();
    void resume();
    void flush();

    double clock() const;
    void set_volume(double vol);
    bool is_opened() const { return this->renderer != nullptr && this->audio_buf != nullptr; }

    Clock audclk;

private:
    // 注册给 IAudioRenderer::open() 的回调
    void on_audio_callback(uint8_t *buf, int len);

    // 从 source_queue 拉一帧，swresample，存入内部缓冲
    int decode_audio_frame();

    std::shared_ptr<FrameQueue> source_queue;
    IAudioRenderer *renderer = nullptr;
    SwrContext *swr = nullptr;
    const int *queue_serial_ptr = nullptr;

    // 内部环形缓冲
    uint8_t *audio_buf    = nullptr;
    int audio_buf_size   = 0;   // 已分配总大小
    int audio_buf_index  = 0;   // 当前读位置
    int audio_buf_data   = 0;   // 有效数据量（字节）

    // 硬件参数
    int hw_sample_rate  = 0;
    int hw_channels     = 0;
    AVSampleFormat hw_sample_fmt = AV_SAMPLE_FMT_NONE;
    int bytes_per_sec   = 0;

    // 当前正在消费的帧的 PTS（用于 clock 计算）
    double frame_pts    = 0.0;
    int    frame_serial = -1;

    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_AUDIOOUTPUT_H
