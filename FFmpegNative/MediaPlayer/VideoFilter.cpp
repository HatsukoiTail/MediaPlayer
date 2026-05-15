#include "VideoFilter.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include "FFmpegException.h"
#include "Frame.h"
#include <iostream>

namespace MediaPlayer
{

// ---- helpers ---------------------------------------------------------------

static AVFilterContext *create_filter(AVFilterGraph *graph, const char *name,
                                       const char *inst, const char *args,
                                       AVBufferRef *hw_device_ctx)
{
    const AVFilter *f = avfilter_get_by_name(name);
    if (!f)
        throw FFmpegException(std::format("Filter '{}' not found", name));

    AVFilterContext *ctx = avfilter_graph_alloc_filter(graph, f, inst);
    if (!ctx)
        throw FFmpegException(std::format("Failed to allocate filter: {}", name));

    if (hw_device_ctx)
        ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    int ret = avfilter_init_str(ctx, args);
    if (ret < 0)
        throw FFmpegException(ret, std::format("Failed to initialize filter: {}", name));

    return ctx;
}

static void link(AVFilterContext *src, AVFilterContext *dst)
{
    int ret = avfilter_link(src, 0, dst, 0);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to link filters");
}

// ===========================================================================

VideoFilter::~VideoFilter()
{
    if (this->is_opened())
        this->close();
}

void VideoFilter::open(AVCodecContext *decoder_ctx)
{
    assert(this->is_opened() == false);

    this->dec_ctx = decoder_ctx;

    this->output_frame_queue = std::make_shared<FrameQueue>();
    this->output_frame_queue->start();

    this->logger.open(std::format("../log/player_filter_{}.log", decoder_ctx->codec->name));
    this->logger.log("VideoFilter opened (graph deferred to first frame)");
}

void VideoFilter::set_target_format(AVPixelFormat fmt, int w, int h)
{
    this->target_fmt = fmt;
    this->target_w   = w > 0 ? w : -1;
    this->target_h   = h > 0 ? h : -1;
}

void VideoFilter::set_render_hw_device(AVBufferRef *device)
{
    this->render_hw_device = device;
}

void VideoFilter::close()
{
    this->stop();

    if (this->buffersrc_ctx)
        av_buffer_unref(&this->buffersrc_ctx->hw_device_ctx);
    if (this->buffersink_ctx)
        av_buffer_unref(&this->buffersink_ctx->hw_device_ctx);

    this->filter_graph.reset();
    this->buffersrc_ctx = nullptr;
    this->buffersink_ctx = nullptr;
    this->dec_ctx = nullptr;
}

void VideoFilter::set_input_queue(std::shared_ptr<FrameQueue> queue)
{
    this->input_queue = std::move(queue);
}

std::shared_ptr<FrameQueue> VideoFilter::output_queue() const
{
    return this->output_frame_queue;
}

void VideoFilter::start()
{
    assert(this->input_queue != nullptr);
    this->output_frame_queue->start();
    this->is_thread_running = true;
    this->filter_thread = std::jthread([this](std::stop_token token)
                                       { this->filter_thread_func(token); });
}

void VideoFilter::stop()
{
    if (!this->filter_thread.joinable())
        return;
    this->logger.log("Stopping...");
    this->filter_thread.request_stop();
    this->input_queue->stop();
    this->output_frame_queue->stop();
    this->filter_thread.join();
}

void VideoFilter::flush()
{
    assert(this->is_opened());

    bool was_running = this->is_running();
    if (was_running)
        this->stop();

    this->output_frame_queue->flush();

    this->filter_graph.reset();
    this->buffersrc_ctx = nullptr;
    this->buffersink_ctx = nullptr;
    this->last_fmt = this->last_w = this->last_h = -1;

    this->logger.log("Filter graph flushed");

    if (was_running)
        this->start();
}

bool VideoFilter::is_opened() const
{
    return this->dec_ctx != nullptr;
}

bool VideoFilter::is_running() const
{
    return this->is_thread_running.load();
}

bool VideoFilter::format_matches(const Frame &frame) const
{
    return frame.format() == this->last_fmt &&
           frame.width()  == this->last_w &&
           frame.height() == this->last_h;
}

// ===========================================================================

void VideoFilter::build_graph(const AVFrame *sample_frame)
{
    assert(this->dec_ctx);

    auto graph = AVFilterGraphPtr(avfilter_graph_alloc());
    if (!graph)
        throw FFmpegException("Failed to allocate filter graph");

    // ---- buffersrc: 基于第一帧的真实格式 ----
    AVFilterContext *src_ctx = avfilter_graph_alloc_filter(
        graph.get(), avfilter_get_by_name("buffer"), "in");
    if (!src_ctx)
        throw FFmpegException("Failed to allocate buffer source");

    auto param_deleter = [](AVBufferSrcParameters *p) {
        av_buffer_unref(&p->hw_frames_ctx);
        av_free(p);
    };
    auto src_params = std::unique_ptr<AVBufferSrcParameters, decltype(param_deleter)>(
        av_buffersrc_parameters_alloc(), param_deleter);
    if (!src_params)
        throw FFmpegException("Failed to allocate buffer source parameters");

    src_params->format               = sample_frame->format;
    src_params->width                = sample_frame->width;
    src_params->height               = sample_frame->height;
    src_params->time_base             = this->dec_ctx->pkt_timebase;
    src_params->sample_aspect_ratio   = this->dec_ctx->sample_aspect_ratio;

    if (this->dec_ctx->hw_frames_ctx)
    {
        src_params->hw_frames_ctx = av_buffer_ref(this->dec_ctx->hw_frames_ctx);
        if (!src_params->hw_frames_ctx)
            throw FFmpegException("Failed to ref hw_frames_ctx");
    }

    int ret = av_buffersrc_parameters_set(src_ctx, src_params.get());
    if (ret < 0)
        throw FFmpegException(ret, "Failed to set buffer source parameters");
    ret = avfilter_init_dict(src_ctx, nullptr);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to initialize buffer source");

    // ---- buffersink: 约束为渲染器目标格式 ----
    AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(
        graph.get(), avfilter_get_by_name("buffersink"), "out");
    if (!sink_ctx)
        throw FFmpegException("Failed to allocate buffer sink");

    AVPixelFormat pix_fmts[] = { this->target_fmt, AV_PIX_FMT_NONE };
    ret = av_opt_set_array(sink_ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                           0, 1, AV_OPT_TYPE_PIXEL_FMT, pix_fmts);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to set output pixel format");

    if (this->render_hw_device)
    {
        sink_ctx->hw_device_ctx = av_buffer_ref(this->render_hw_device);
        if (!sink_ctx->hw_device_ctx)
            throw FFmpegException("Failed to ref render hw_device");
    }

    ret = avfilter_init_dict(sink_ctx, nullptr);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to initialize buffer sink");

    // ---- build player filter chain ----
    const bool dec_hw = (this->dec_ctx->hw_device_ctx != nullptr);
    const AVPixelFormat dec_fmt = (AVPixelFormat)sample_frame->format;
    const AVPixelFormat dec_sw_fmt = this->dec_ctx->hw_frames_ctx
        ? reinterpret_cast<AVHWFramesContext *>(this->dec_ctx->hw_frames_ctx->data)->sw_format
        : AV_PIX_FMT_NONE;

    AVFilterContext *prev = src_ctx;

    if (!dec_hw)
    {
        // 软件解码：格式不同则加 format 转换
        if (dec_fmt != this->target_fmt)
        {
            auto *f = create_filter(graph.get(), "format", "fmt",
                                     av_get_pix_fmt_name(this->target_fmt), nullptr);
            link(prev, f);
            prev = f;
        }
    }
    else
    {
        // 硬件解码：下载 → 格式转换
        auto *dl = create_filter(graph.get(), "hwdownload", "download",
                                  nullptr, nullptr);
        link(prev, dl);
        prev = dl;

        // 显式声明下载后的格式
        auto *fmt = create_filter(graph.get(), "format", "fmt1",
                                   av_get_pix_fmt_name(dec_sw_fmt), nullptr);
        link(prev, fmt);
        prev = fmt;

        if (dec_sw_fmt != this->target_fmt)
        {
            // 下载后格式与渲染目标不同，再转一次
            auto *fmt2 = create_filter(graph.get(), "format", "fmt2",
                                        av_get_pix_fmt_name(this->target_fmt), nullptr);
            link(prev, fmt2);
            prev = fmt2;
        }
    }

    link(prev, sink_ctx);

    // ---- configure ----
    ret = avfilter_graph_config(graph.get(), nullptr);
    if (ret < 0)
    {
        std::cout << FFmpegException::error_message(ret) << std::endl;
        throw FFmpegException(ret, "Failed to configure filter graph");
    }

    this->filter_graph   = std::move(graph);
    this->buffersrc_ctx  = src_ctx;
    this->buffersink_ctx = sink_ctx;

    this->last_fmt = sample_frame->format;
    this->last_w   = sample_frame->width;
    this->last_h   = sample_frame->height;

    this->logger.log(std::format("Graph built: {} filters, fmt={}→{}",
                                 this->filter_graph->nb_filters,
                                 av_get_pix_fmt_name((AVPixelFormat)sample_frame->format),
                                 av_get_pix_fmt_name(this->target_fmt)));
}

// ===========================================================================

void VideoFilter::filter_thread_func(std::stop_token token)
{
    this->logger.log("VideoFilter thread started");

    int64_t frame_count = 0;
    while (!token.stop_requested())
    {
        auto frame = this->input_queue->dequeue();
        if (frame.is_null())
            break;

        // 首帧或格式变化 → 构建/重建滤镜图
        if (!this->filter_graph || !this->format_matches(frame))
            this->build_graph(frame.get());

        frame_count++;
        if (frame_count % 100 == 0)
        {
            this->logger.log(std::format("{}, receive: fmt={} {}x{} pts={}",
                                         frame_count, frame.format(),
                                         frame.width(), frame.height(), frame.pts()));
        }

        this->push_frame(frame.get());
        this->pull_frames(token);
    }

    // Drain
    if (this->buffersrc_ctx)
        av_buffersrc_close(this->buffersrc_ctx, AV_NOPTS_VALUE, 0);
    this->pull_frames(token);

    this->output_frame_queue->stop();
    this->logger.log(std::format("VideoFilter exiting, total: {}", frame_count));
    this->is_thread_running = false;
}

void VideoFilter::push_frame(AVFrame *frame)
{
    int ret = av_buffersrc_add_frame_flags(this->buffersrc_ctx, frame,
                                            AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to push frame to filter graph");
}

void VideoFilter::pull_frames(std::stop_token token)
{
    while (!token.stop_requested())
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
