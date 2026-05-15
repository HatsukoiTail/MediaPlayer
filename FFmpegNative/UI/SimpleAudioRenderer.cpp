#include "SimpleAudioRenderer.h"
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <cstring>
#include <vector>

bool SimpleAudioRenderer::open(int sample_rate, AVSampleFormat fmt,
                                const AVChannelLayout &layout,
                                Callback cb)
{
    this->hw_rate = sample_rate;
    this->hw_ch   = layout.nb_channels;
    this->hw_clock = 0.0;
    this->bytes_written = 0;

    QAudioFormat audio_fmt;
    audio_fmt.setSampleRate(sample_rate);
    audio_fmt.setChannelCount(layout.nb_channels);

    // AVSampleFormat → QAudioFormat
    switch (fmt)
    {
    case AV_SAMPLE_FMT_S16:
        audio_fmt.setSampleFormat(QAudioFormat::Int16);
        this->bytes_per_sample = 2;
        break;
    case AV_SAMPLE_FMT_S32:
        audio_fmt.setSampleFormat(QAudioFormat::Int32);
        this->bytes_per_sample = 4;
        break;
    case AV_SAMPLE_FMT_FLT:
        audio_fmt.setSampleFormat(QAudioFormat::Float);
        this->bytes_per_sample = 4;
        break;
    default:
        audio_fmt.setSampleFormat(QAudioFormat::Int16);
        this->bytes_per_sample = 2;
        break;
    }

    this->hw_buf = sample_rate / 25 * layout.nb_channels * this->bytes_per_sample; // 40ms

    this->callback = std::move(cb);

    this->sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), audio_fmt, this);
    this->sink->setBufferSize(this->hw_buf * 2);

    this->device = this->sink->start();

    if (!this->device)
        return false;

    connect(this->device, &QIODevice::readyRead, this, &SimpleAudioRenderer::onNeedMoreData);

    return true;
}

void SimpleAudioRenderer::close()
{
    if (this->sink)
    {
        this->sink->stop();
        this->sink->deleteLater();
        this->sink   = nullptr;
        this->device = nullptr;
    }
}

void SimpleAudioRenderer::pause()
{
    if (this->sink) this->sink->suspend();
}

void SimpleAudioRenderer::resume()
{
    if (this->sink) this->sink->resume();
}

double SimpleAudioRenderer::clock() const
{
    return this->hw_clock.load();
}

void SimpleAudioRenderer::set_volume(double vol)
{
    if (this->sink)
        this->sink->setVolume(qBound(0.0, vol, 1.0));
}

int SimpleAudioRenderer::sample_rate() const   { return this->hw_rate; }
int SimpleAudioRenderer::channels() const       { return this->hw_ch; }
int SimpleAudioRenderer::buffer_size() const    { return this->hw_buf; }
AVSampleFormat SimpleAudioRenderer::sample_format() const { return AV_SAMPLE_FMT_S16; }

void SimpleAudioRenderer::onNeedMoreData()
{
    if (!this->device || !this->sink)
        return;

    int needed = this->sink->bytesFree();
    if (needed <= 0)
        return;

    std::vector<uint8_t> buf(needed, 0);

    this->callback(buf.data(), needed);

    qint64 written = this->device->write(reinterpret_cast<const char *>(buf.data()), needed);
    if (written > 0)
    {
        this->bytes_written += written;
        // 时钟 = 已写入总字节 / 字节每秒
        this->hw_clock = (double)this->bytes_written.load() /
                         (this->hw_rate * this->hw_ch * this->bytes_per_sample);
    }
}
