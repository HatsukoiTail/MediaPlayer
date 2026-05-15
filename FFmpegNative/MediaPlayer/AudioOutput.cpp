#include "AudioOutput.h"

#include <cassert>
#include <cstring>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

#include "FFmpegException.h"

#define AUDIO_BUF_SIZE (256 * 1024)  // 256KB 内部缓冲

namespace MediaPlayer
{

AudioOutput::~AudioOutput()
{
    this->close();
}

void AudioOutput::set_source_queue(std::shared_ptr<FrameQueue> queue)
{
    this->source_queue = std::move(queue);
}

void AudioOutput::set_renderer(IAudioRenderer *r)
{
    this->renderer = r;
}

void AudioOutput::set_queue_serial(const int *serial_ptr)
{
    this->queue_serial_ptr = serial_ptr;
    this->audclk.set_queue_serial(serial_ptr);
}

bool AudioOutput::open(int sample_rate, AVSampleFormat fmt,
                        const AVChannelLayout &layout)
{
    assert(this->source_queue);
    assert(this->renderer);

    // 分配 swresample 上下文
    AVChannelLayout out_layout;
    av_channel_layout_copy(&out_layout, &layout);

    int ret = swr_alloc_set_opts2(&this->swr,
                                   &out_layout, AV_SAMPLE_FMT_S16, sample_rate,
                                   &layout, fmt, sample_rate,
                                   0, nullptr);
    if (ret < 0 || !this->swr)
        throw FFmpegException("Failed to allocate swresample context");

    ret = swr_init(this->swr);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to init swresample");

    this->hw_sample_rate = sample_rate;
    this->hw_channels    = layout.nb_channels;
    this->hw_sample_fmt  = AV_SAMPLE_FMT_S16;
    this->bytes_per_sec  = sample_rate * layout.nb_channels *
                           av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

    // 分配内部缓冲
    this->audio_buf      = (uint8_t *)av_malloc(AUDIO_BUF_SIZE);
    this->audio_buf_size = AUDIO_BUF_SIZE;
    this->audio_buf_index = 0;
    this->audio_buf_data  = 0;

    // 打开硬件
    bool ok = this->renderer->open(sample_rate, AV_SAMPLE_FMT_S16, layout,
                                    [this](uint8_t *buf, int len) {
                                        this->on_audio_callback(buf, len);
                                    });
    if (!ok)
    {
        av_free(this->audio_buf);
        this->audio_buf = nullptr;
        return false;
    }

    this->logger.open("../log/audio_output.log");
    this->logger.log(std::format("AudioOutput opened: {}Hz, {}ch", sample_rate, layout.nb_channels));
    return true;
}

void AudioOutput::close()
{
    if (this->renderer)
        this->renderer->close();

    if (this->swr)
    {
        swr_free(&this->swr);
        this->swr = nullptr;
    }

    av_free(this->audio_buf);
    this->audio_buf = nullptr;
    this->audio_buf_size = 0;
    this->audio_buf_data = 0;
    this->audio_buf_index = 0;
}

void AudioOutput::pause()
{
    if (this->renderer)
        this->renderer->pause();
    this->audclk.pause();
}

void AudioOutput::resume()
{
    this->audclk.resume();
    if (this->renderer)
        this->renderer->resume();
}

void AudioOutput::flush()
{
    this->audio_buf_index = 0;
    this->audio_buf_data  = 0;
    this->frame_pts       = 0.0;
    this->frame_serial    = -1;
    this->audclk.set(NAN, -1);
}

double AudioOutput::clock() const
{
    return this->audclk.get();
}

void AudioOutput::set_volume(double vol)
{
    if (this->renderer)
        this->renderer->set_volume(vol);
}

// ===========================================================================
// 音频回调（硬件线程）
// ===========================================================================

void AudioOutput::on_audio_callback(uint8_t *stream, int len)
{
    double callback_time = av_gettime_relative() / 1000000.0;

    while (len > 0)
    {
        if (this->audio_buf_index >= this->audio_buf_data)
        {
            // 需要新数据
            int audio_size = this->decode_audio_frame();
            if (audio_size < 0)
            {
                // 错误或没有数据，输出静音
                this->audio_buf_data  = this->hw_sample_rate / 4; // ~250ms
                this->audio_buf_data *= this->hw_channels *
                                        av_get_bytes_per_sample(this->hw_sample_fmt);
                memset(this->audio_buf, 0, this->audio_buf_data);
            }
            else
            {
                this->audio_buf_data = audio_size;
            }
            this->audio_buf_index = 0;
        }

        int copy_len = this->audio_buf_data - this->audio_buf_index;
        if (copy_len > len)
            copy_len = len;

        memcpy(stream, this->audio_buf + this->audio_buf_index, copy_len);
        len -= copy_len;
        stream += copy_len;
        this->audio_buf_index += copy_len;
    }

    // 计算音频时钟（硬件缓冲区位置反推）
    int hw_buf_size = this->renderer ? this->renderer->buffer_size() : 0;
    int write_buf   = this->audio_buf_data - this->audio_buf_index;

    if (!std::isnan(this->frame_pts))
    {
        double pts = this->frame_pts -
                     (double)(2 * hw_buf_size + write_buf) / this->bytes_per_sec;
        this->audclk.set(pts, this->frame_serial);
    }
}

// ===========================================================================
// 从滤镜队列拉一帧 → swresample → 存入 audio_buf
// ===========================================================================

int AudioOutput::decode_audio_frame()
{
    // 消耗队列中串号已过期的帧
    int current_serial = this->queue_serial_ptr ? *this->queue_serial_ptr : 0;
    while (this->source_queue->size() > 0)
    {
        auto frame = this->source_queue->peek();
        if (frame.is_null())
            break;

        // 丢弃旧帧（serial 检查简化：比较 PTS 是否连续）
        // 暂时不做，由 Demuxer 的 flush 保证
        break;
    }

    if (this->source_queue->size() == 0)
        return -1;

    auto frame = this->source_queue->dequeue();
    if (frame.is_null())
        return -1;

    AVFrame *avf = frame.get();

    // 记录当前帧的 PTS
    AVRational tb = avf->time_base;
    if (tb.num == 0 || tb.den == 0)
        tb = {1, avf->sample_rate};
    this->frame_pts    = avf->pts * av_q2d(tb);
    this->frame_serial = current_serial;

    // swresample → 硬件格式
    int out_samples = avf->nb_samples;
    if (out_samples <= 0)
        out_samples = 1024;

    int out_bytes = out_samples * this->hw_channels *
                    av_get_bytes_per_sample(this->hw_sample_fmt);

    if (out_bytes > this->audio_buf_size)
    {
        av_free(this->audio_buf);
        this->audio_buf_size = out_bytes + 4096;
        this->audio_buf = (uint8_t *)av_malloc(this->audio_buf_size);
    }

    uint8_t *out_buf = this->audio_buf;
    int ret = swr_convert(this->swr, &out_buf, out_samples,
                          (const uint8_t **)avf->data, avf->nb_samples);
    if (ret < 0)
    {
        this->logger.log("swr_convert failed");
        return -1;
    }

    return ret * this->hw_channels *
           av_get_bytes_per_sample(this->hw_sample_fmt);
}

} // namespace MediaPlayer
