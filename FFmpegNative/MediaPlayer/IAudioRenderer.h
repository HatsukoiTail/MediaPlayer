#pragma once

#ifndef MEDIAPLAYER_IAUDIORENDERER_H
#define MEDIAPLAYER_IAUDIORENDERER_H

#include <functional>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

namespace MediaPlayer
{

/// 音频渲染器抽象接口（回调 / pull 模型）。
///
/// 实现者（SDL / Avalonia / WASAPI）在打开设备时注册回调，
/// 由音频硬件线程按固定周期调用回调函数来索取 PCM 数据。
/// 回调内应从源队列拉取解码好的帧、写入 buf。
///
/// 音频时钟由硬件缓冲区消费进度反推，是播放器的主时钟。
class IAudioRenderer
{
public:
    virtual ~IAudioRenderer() = default;

    /// 音频回调：buf 为输出缓冲区，len 为需填充的字节数。
    /// 由音频硬件线程调用，必须在回调结束前填满。
    using Callback = std::function<void(uint8_t *buf, int len)>;

    /// 打开设备，注册回调。
    /// 返回实际采用的参数（可能与请求不同，例如硬件不支持 float32 → 退回 s16）。
    virtual bool open(int sample_rate, AVSampleFormat fmt,
                      const AVChannelLayout &layout,
                      Callback cb) = 0;

    /// 关闭设备。
    virtual void close() = 0;

    /// 暂停 / 恢复。
    virtual void pause() = 0;
    virtual void resume() = 0;

    /// 主时钟（秒）。
    /// = 上次回调的 PTS - (hw_buf_size + write_buf) / bytes_per_sec
    virtual double clock() const = 0;

    /// 音量 [0.0, 2.0]。
    virtual void set_volume(double vol) = 0;

    /// 实际采用的参数。
    virtual int sample_rate() const = 0;
    virtual int channels() const = 0;
    virtual AVSampleFormat sample_format() const = 0;

    /// 硬件缓冲区大小（字节），用于时钟计算。
    virtual int buffer_size() const = 0;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_IAUDIORENDERER_H
