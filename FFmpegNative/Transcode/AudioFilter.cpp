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

namespace Transcode
{

    AudioFilter::~AudioFilter()
    {
        if (this->is_opened())
        {
            this->close();
        }
    }

    void AudioFilter::open(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx)
    {
        assert(this->is_opened() == false);
        this->init_graph(dec_ctx, enc_ctx);
        this->output_frame_queue = std::make_shared<FrameQueue>();
    }

    void AudioFilter::close()
    {
        if (this->is_running())
        {
            this->stop();
        }
        this->filter_graph.reset();
        this->buffersrc_ctx = nullptr;
        this->buffersink_ctx = nullptr;
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

    bool AudioFilter::is_opened() const
    {
        return this->filter_graph != nullptr;
    }

    bool AudioFilter::is_running() const
    {
        return this->is_thread_running;
    }

    void AudioFilter::set_logger(Logger logger)
    {
        this->logger = std::move(logger);
    }

    void AudioFilter::init_graph(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx)
    {
        auto graph = avfilter_graph_alloc();
        if (graph == nullptr)
        {
            throw FFmpegException("Failed to allocate filter graph");
        }
        this->filter_graph = AVFilterGraphPtr(graph);

        const AVFilter *buffersrc = avfilter_get_by_name("abuffer");
        const AVFilter *buffersink = avfilter_get_by_name("abuffersink");

        // --- Build buffersrc args from decoder context ---
        char ch_layout_buf[64];
        av_channel_layout_describe(&dec_ctx->ch_layout, ch_layout_buf, sizeof(ch_layout_buf));

        std::string args = std::format(
            "time_base={}/{}:sample_rate={}:sample_fmt={}:channel_layout={}",
            dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
            dec_ctx->sample_rate,
            av_get_sample_fmt_name(dec_ctx->sample_fmt),
            ch_layout_buf);

        AVFilterContext *src_ctx = nullptr;
        int ret = avfilter_graph_create_filter(&src_ctx, buffersrc, "in",
                                               args.c_str(), nullptr, graph);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to create audio buffer source");
        }
        this->buffersrc_ctx = src_ctx;

        // --- Build buffersink constrained to encoder input format ---
        AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(graph, buffersink, "out");
        if (sink_ctx == nullptr)
        {
            throw FFmpegException("Failed to allocate audio buffer sink");
        }

        ret = av_opt_set_bin(sink_ctx, "sample_fmts",
                             (uint8_t *)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to set output sample format");
        }

        av_channel_layout_describe(&enc_ctx->ch_layout, ch_layout_buf, sizeof(ch_layout_buf));
        ret = av_opt_set(sink_ctx, "ch_layouts", ch_layout_buf, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to set output channel layout");
        }

        ret = av_opt_set_bin(sink_ctx, "sample_rates",
                             (uint8_t *)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to set output sample rate");
        }

        ret = avfilter_init_dict(sink_ctx, nullptr);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to initialize audio buffer sink");
        }
        this->buffersink_ctx = sink_ctx;

        // --- Parse filter chain between src and sink ---
        std::string filter_desc = this->build_filter_desc(dec_ctx, enc_ctx);

        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs = avfilter_inout_alloc();
        if (outputs == nullptr || inputs == nullptr)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            throw FFmpegException("Failed to allocate filter I/O");
        }

        outputs->name = av_strdup("in");
        outputs->filter_ctx = src_ctx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = sink_ctx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        ret = avfilter_graph_parse_ptr(graph, filter_desc.c_str(),
                                       &inputs, &outputs, nullptr);
        if (ret < 0)
        {
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            throw FFmpegException(ret,
                                  std::format("Failed to parse filter graph: {}", filter_desc));
        }

        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);

        ret = avfilter_graph_config(graph, nullptr);
        if (ret < 0)
        {
            throw FFmpegException(ret, "Failed to configure filter graph");
        }
    }

    std::string AudioFilter::build_filter_desc(const AVCodecContext *dec_ctx,
                                               const AVCodecContext *enc_ctx)
    {
        bool need_rate = (enc_ctx->sample_rate != dec_ctx->sample_rate);
        bool need_format = (enc_ctx->sample_fmt != dec_ctx->sample_fmt);
        bool need_layout = (av_channel_layout_compare(&dec_ctx->ch_layout,
                                                      &enc_ctx->ch_layout) != 0);

        if (!need_rate && !need_format && !need_layout)
            return "anull";

        std::string desc = "aformat=";
        bool first = true;

        if (need_format)
        {
            desc += "sample_fmts=" + std::string(av_get_sample_fmt_name(enc_ctx->sample_fmt));
            first = false;
        }
        if (need_rate)
        {
            if (!first)
                desc += ":";
            desc += "sample_rates=" + std::to_string(enc_ctx->sample_rate);
            first = false;
        }
        if (need_layout)
        {
            if (!first)
                desc += ":";
            char buf[64];
            av_channel_layout_describe(&enc_ctx->ch_layout, buf, sizeof(buf));
            desc += "channel_layouts=" + std::string(buf);
        }

        return desc;
    }

    void AudioFilter::filter_thread_func(std::stop_token token)
    {
        this->is_thread_running = true;
        this->logger.log("AudioFilter thread started");

        int64_t frame_count = 0;
        while (!token.stop_requested())
        {
            auto frame = this->input_queue->dequeue();
            if (frame.get() == nullptr)
            {
                break;
            }

            if (this->push_frame(frame.get()) < 0)
            {
                break;
            }

            this->pull_frames();
            frame_count++;
        }

        // Drain filter graph
        this->push_frame(nullptr);
        this->pull_frames();

        this->output_frame_queue->stop();
        this->logger.log(std::format("AudioFilter thread exiting, total: {}", frame_count));
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
            auto filtered_frame = Frame::create();
            int ret = av_buffersink_get_frame(this->buffersink_ctx,
                                              filtered_frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            if (ret < 0)
            {
                break;
            }

            filtered_frame.get()->time_base =
                av_buffersink_get_time_base(this->buffersink_ctx);

            this->output_frame_queue->enqueue(std::move(filtered_frame));
        }
    }

} // namespace Transcode