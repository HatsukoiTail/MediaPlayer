#pragma once

#include "MediaPlayer/IVideoRenderer.h"
#include <QImage>
#include <QObject>

/// 视频渲染器：将 AVFrame 转为 QImage，通过信号发送给 Widget
class SimpleVideoRenderer : public QObject, public MediaPlayer::IVideoRenderer
{
    Q_OBJECT
public:
    bool open(int width, int height, AVPixelFormat fmt) override;
    void close() override;
    void present(const AVFrame *frame) override;
    void set_hw_device(AVBufferRef *device) override;
    AVPixelFormat preferred_format() const override;
    int width() const override;
    int height() const override;
    bool wants_vsync() const override;

signals:
    void frame_ready(const QImage &image);

private:
    int display_w = 1920;
    int display_h = 1080;
};
