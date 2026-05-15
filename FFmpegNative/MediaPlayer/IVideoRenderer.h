#pragma once

#ifndef MEDIAPLAYER_IVIDEORENDERER_H
#define MEDIAPLAYER_IVIDEORENDERER_H

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;

namespace MediaPlayer
{

/// 视频渲染器抽象接口。
/// 实现者负责将解码后的帧呈现到屏幕（或离屏表面）。
/// 播放器调用 present() 时，帧的时间戳已通过同步检查，应立即渲染。
class IVideoRenderer
{
public:
    virtual ~IVideoRenderer() = default;

    /// 打开渲染器，传入期望的视频参数。
    /// 返回渲染器首选的像素格式（可能和请求的不同）。
    virtual bool open(int width, int height, AVPixelFormat fmt) = 0;

    /// 关闭渲染器。
    virtual void close() = 0;

    /// 呈现一帧。调用前已通过 AV 同步，应立刻显示。
    virtual void present(const AVFrame *frame) = 0;

    /// 如果有外部硬件设备（如 D3D11/CUDA），传入供渲染器做零拷贝上传。
    /// 返回 nullptr 或不调用此方法表示仅支持软件帧。
    virtual void set_hw_device(AVBufferRef *device) = 0;

    /// 渲染器偏好的像素格式（解码器/滤镜依此选择输出格式）。
    virtual AVPixelFormat preferred_format() const = 0;

    /// 当前渲染尺寸（可能因 resize 而改变）。
    virtual int width() const = 0;
    virtual int height() const = 0;

    /// 是否需要限制帧率（由渲染器决定，某些渲染器自带 vsync）。
    virtual bool wants_vsync() const = 0;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_IVIDEORENDERER_H
