#include "AudioFilter.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include "FFmpegException.h"
#include "Frame.h"

namespace MediaPlayer
{

AudioFilter::~AudioFilter()
{
    if (this->is_opened())
        this->close();
}

void AudioFilter::open(AVCodecContext *decoder_ctx)
{
    assert(this->is_opened() == false);

    this->dec_ctx = decoder_ctx;

    this->output_frame_queue = std::make_shared<FrameQueue>();

    this->logger.open("../log/player_audio_filter.log");
    this->logger.log("AudioFilter opened (graph deferred to first frame)");
}

void AudioFilter::set_target_format(AVSampleFormat fmt, int sample_rate,
                                     const AVChannelLayout &layout)
{
    this->target_fmt  = fmt;
    this->target_rate = sample_rate;
    av_channel_layout_uninit(&this->target_layout);
    av_channel_layout_copy(&this->target_layout, &layout);
}

void AudioFilter::set_speed(double s)
{
    if (s <= 0.0)
        s = 1.0;
    if (this->speed == s)
        return;

    this->speed = s;
    // 如果图已构建，下次帧处理时自动检测重建
    if (this->filter_graph)
    {
        this->logger.log(std::format("Speed changed to {:.2f}, graph will rebuild", s));
    }
}

void AudioFilter::close()
{
    if (this->is_running())
        this->stop();

    this->filter_graph.reset();
    this->buffersrc_ctx = nullptr;
    this->buffersink_ctx = nullptr;
    av_channel_layout_uninit(&this->target_layout);
}

void AudioFilter::set_input_queue(std::shared_ptr<FrameQueue> queue)
{
    this->input_queue = std::move(queue);
}

std::shared_ptr<FrameQueue> AudioFilter::output_queue() const
{
    return this->output_frame_queue;
}

void AudioFilter::start()
{
    assert(this->is_opened());
    assert(this->is_running() == false);
    assert(this->input_queue != nullptr);
    this->output_frame_queue->start();
    this->is_thread_running = true;
    this->filter_thread = std::jthread([this](std::stop_token token)
                                       { this->filter_thread_func(token); });
}

void AudioFilter::stop()
{
    if (!this->filter_thread.joinable())
        return;
    this->filter_thread.request_stop();
    this->input_queue->stop();
    this->output_frame_queue->stop();
    this->filter_thread.join();
}

void AudioFilter::flush()
{
    assert(this->is_opened());

    bool was_running = this->is_running();
    if (was_running)
        this->stop();

    this->output_frame_queue->flush();
    this->filter_graph.reset();
    this->buffersrc_ctx = nullptr;
    this->buffersink_ctx = nullptr;

    this->logger.log("AudioFilter flushed");

    if (was_running)
        this->start();
}

bool AudioFilter::is_opened() const
{
    return this->dec_ctx != nullptr;
}

bool AudioFilter::is_running() const
{
    return this->is_thread_running.load();
}

// ===========================================================================

void AudioFilter::init_graph(const AVFrame *sample_frame)
{
    auto graph = avfilter_graph_alloc();
    if (!graph)
        throw FFmpegException("Failed to allocate filter graph");
    this->filter_graph = AVFilterGraphPtr(graph);

    const AVFilter *buffersrc  = avfilter_get_by_name("abuffer");
    const AVFilter *buffersink = avfilter_get_by_name("abuffersink");

    // ---- buffersrc: 基于第一帧的真实格式 ----
    char ch_layout_buf[64];
    av_channel_layout_describe(&sample_frame->ch_layout, ch_layout_buf, sizeof(ch_layout_buf));

    std::string args = std::format(
        "time_base={}/{}:sample_rate={}:sample_fmt={}:channel_layout={}",
        sample_frame->time_base.num, sample_frame->time_base.den,
        sample_frame->sample_rate,
        av_get_sample_fmt_name((AVSampleFormat)sample_frame->format),
        ch_layout_buf);

    AVFilterContext *src_ctx = nullptr;
    int ret = avfilter_graph_create_filter(&src_ctx, buffersrc, "in",
                                            args.c_str(), nullptr, graph);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to create audio buffer source");
    this->buffersrc_ctx = src_ctx;

    // ---- buffersink: 约束为目标格式 ----
    AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(graph, buffersink, "out");
    if (!sink_ctx)
        throw FFmpegException("Failed to allocate audio buffer sink");

    ret = av_opt_set_bin(sink_ctx, "sample_fmts",
                         (uint8_t *)&this->target_fmt, sizeof(this->target_fmt),
                         AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to set output sample format");

    char tgt_layout_buf[64];
    av_channel_layout_describe(&this->target_layout, tgt_layout_buf, sizeof(tgt_layout_buf));
    ret = av_opt_set(sink_ctx, "ch_layouts", tgt_layout_buf, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to set output channel layout");

    ret = av_opt_set_bin(sink_ctx, "sample_rates",
                         (uint8_t *)&this->target_rate, sizeof(this->target_rate),
                         AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to set output sample rate");

    ret = avfilter_init_dict(sink_ctx, nullptr);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to initialize audio buffer sink");
    this->buffersink_ctx = sink_ctx;

    // ---- parse filter chain ----
    std::string filter_desc = this->build_filter_desc(sample_frame);

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    if (!outputs || !inputs)
    {
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        throw FFmpegException("Failed to allocate filter I/O");
    }

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = src_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    ret = avfilter_graph_parse_ptr(graph, filter_desc.c_str(),
                                    &inputs, &outputs, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (ret < 0)
        throw FFmpegException(ret,
                              std::format("Failed to parse filter graph: {}", filter_desc));

    ret = avfilter_graph_config(graph, nullptr);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to configure filter graph");

    this->logger.log(std::format("Audio graph built: fmt={}, rate={}, layout={}, speed={:.2f}",
                                 av_get_sample_fmt_name(this->target_fmt),
                                 this->target_rate, tgt_layout_buf, this->speed));
}

std::string AudioFilter::build_filter_desc(const AVFrame *frame) const
{
    AVSampleFormat src_fmt  = (AVSampleFormat)frame->format;
    int src_rate            = frame->sample_rate;
    const AVChannelLayout *src_layout = &frame->ch_layout;

    bool need_rate   = (src_rate != this->target_rate);
    bool need_fmt    = (src_fmt != this->target_fmt);
    bool need_layout = (av_channel_layout_compare(src_layout, &this->target_layout) != 0);
    bool need_speed  = (std::abs(this->speed - 1.0) > 0.001);

    if (!need_rate && !need_fmt && !need_layout && !need_speed)
        return "anull";

    std::string desc;

    // aformat: 格式/采样率/声道布局转换
    if (need_rate || need_fmt || need_layout)
    {
        desc += "aformat=";
        bool first = true;

        if (need_fmt)
        {
            desc += "sample_fmts=" + std::string(av_get_sample_fmt_name(this->target_fmt));
            first = false;
        }
        if (need_rate)
        {
            if (!first) desc += ":";
            desc += "sample_rates=" + std::to_string(this->target_rate);
            first = false;
        }
        if (need_layout)
        {
            if (!first) desc += ":";
            char buf[64];
            av_channel_layout_describe(&this->target_layout, buf, sizeof(buf));
            desc += "channel_layouts=" + std::string(buf);
        }
    }

    // atempo: 倍速
    if (need_speed)
    {
        if (!desc.empty()) desc += ",";
        desc += std::format("atempo={:.2f}", this->speed);
    }

    return desc;
}

// ===========================================================================

void AudioFilter::filter_thread_func(std::stop_token token)
{
    this->logger.log("AudioFilter thread started");

    int64_t frame_count = 0;
    while (!token.stop_requested())
    {
        auto frame = this->input_queue->dequeue();
        if (frame.is_null())
            break;

        // 首帧构建图，后续若格式/速度变化则重建
        if (!this->filter_graph)
            this->init_graph(frame.get());

        frame_count++;

        if (this->push_frame(frame.get()) < 0)
            break;

        this->pull_frames();
    }

    // Drain
    if (this->buffersrc_ctx)
        av_buffersrc_close(this->buffersrc_ctx, AV_NOPTS_VALUE, 0);
    this->pull_frames();

    this->output_frame_queue->stop();
    this->logger.log(std::format("AudioFilter exiting, total: {}", frame_count));
    this->is_thread_running = false;
}

int AudioFilter::push_frame(AVFrame *frame)
{
    return av_buffersrc_add_frame_flags(this->buffersrc_ctx, frame,
                                         AV_BUFFERSRC_FLAG_KEEP_REF);
}

void AudioFilter::pull_frames()
{
    while (true)
    {
        auto filtered = Frame::create();
        int ret = av_buffersink_get_frame(this->buffersink_ctx, filtered.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;

        filtered.get()->time_base = av_buffersink_get_time_base(this->buffersink_ctx);

        this->output_frame_queue->enqueue(std::move(filtered));
    }
}

} // namespace MediaPlayer
