#pragma once

#include "MediaPlayer/IAudioRenderer.h"
#include <QAudioSink>
#include <QObject>

/// 音频渲染器：QAudioSink 实现真实音频输出
class SimpleAudioRenderer : public QObject, public MediaPlayer::IAudioRenderer
{
    Q_OBJECT
public:
    bool open(int sample_rate, AVSampleFormat fmt,
              const AVChannelLayout &layout,
              Callback cb) override;
    void close() override;
    void pause() override;
    void resume() override;
    double clock() const override;
    void set_volume(double vol) override;
    int sample_rate() const override;
    int channels() const override;
    int buffer_size() const override;
    AVSampleFormat sample_format() const override;

private slots:
    void onNeedMoreData();

private:
    QAudioSink *sink   = nullptr;
    QIODevice  *device = nullptr;
    Callback    callback;
    int         hw_rate   = 44100;
    int         hw_ch     = 2;
    int         hw_buf    = 4096;
    int         bytes_per_sample = 2;
    std::atomic<double> hw_clock{0.0};
    std::atomic<qint64> bytes_written{0};
};
