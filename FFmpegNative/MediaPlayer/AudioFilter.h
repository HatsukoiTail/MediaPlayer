#pragma once

#ifndef MEDIAPLAYER_AUDIOFILTER_H
#define MEDIAPLAYER_AUDIOFILTER_H

#include <atomic>
#include <memory>
#include <thread>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

struct AVCodecContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVFrame;

#include "FilterGraph.h"
#include "FrameQueue.h"
#include "Logger.h"

namespace MediaPlayer
{

class AudioFilter
{
public:
    AudioFilter() = default;
    AudioFilter(const AudioFilter &) = delete;
    AudioFilter &operator=(const AudioFilter &) = delete;
    AudioFilter(AudioFilter &&) = delete;
    AudioFilter &operator=(AudioFilter &&) = delete;
    ~AudioFilter();

public:
    void open(AVCodecContext *dec_ctx);

    // 来自音频渲染器的目标格式
    void set_target_format(AVSampleFormat fmt, int sample_rate,
                           const AVChannelLayout &layout);
    void set_speed(double speed);

    void close();
    void set_input_queue(std::shared_ptr<FrameQueue> queue);
    std::shared_ptr<FrameQueue> output_queue() const;
    void start();
    void stop();
    void flush();

public:
    bool is_opened() const;
    bool is_running() const;

private:
    void filter_thread_func(std::stop_token token);
    void init_graph(const AVFrame *sample_frame);
    std::string build_filter_desc(const AVFrame *frame) const;

    int push_frame(AVFrame *frame);
    void pull_frames();

private:
    AVCodecContext *dec_ctx = nullptr;  // borrowed

    AVFilterGraphPtr filter_graph;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;

    AVSampleFormat target_fmt = AV_SAMPLE_FMT_S16;
    int target_rate = 44100;
    AVChannelLayout target_layout{};
    double speed = 1.0;

    std::shared_ptr<FrameQueue> input_queue;
    std::shared_ptr<FrameQueue> output_frame_queue;
    std::jthread filter_thread;
    std::atomic<bool> is_thread_running{false};
    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_AUDIOFILTER_H
