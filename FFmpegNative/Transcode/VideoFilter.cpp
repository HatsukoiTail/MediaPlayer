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

namespace Transcode
{

    // helpers
    static bool hardware_scale_available(AVBufferRef *hw_frames_ctx,
                                         AVPixelFormat target_format,
                                         int width, int height);

    static AVFilterContext *create_filter(AVFilterGraph *graph, const char *name, const char *inst, const char *args, AVBufferRef *hw_device_ctx);

    static void link(AVFilterContext *src, AVFilterContext *dst);

    static std::tuple<AVFilterContext *, AVFilterContext *> build_filter_chain(AVFilterGraph *graph, const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);

    VideoFilter::~VideoFilter()
    {
        if (this->is_opened())
        {
            this->close();
        }
    }

    void VideoFilter::open(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx)
    {
        assert(this->is_opened() == false);
        this->logger.open(std::format("../log/filter_{}.log", dec_ctx->codec->name));

        this->build_graph(dec_ctx, enc_ctx);
        for (unsigned i = 0; i < this->filter_graph->nb_filters; i++)
        {
            auto *f = filter_graph->filters[i];
            this->logger.log(std::format("graph filter[{}]: name='{}', filter='{}'",
                                         i, f->name, f->filter->name));
        }

        this->output_frame_queue = std::make_shared<FrameQueue>();
        this->output_frame_queue->start();
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
        // assert(this->is_opened());
        assert(this->is_running() == false);
        assert(this->input_queue != nullptr);
        this->output_frame_queue->start();
        this->filter_thread = std::jthread([this](std::stop_token token)
                                           { this->filter_thread_func(token); });
    }

    void VideoFilter::stop()
    {
        if (!this->filter_thread.joinable())
            return;
        this->logger.log("Starting stop.");
        this->filter_thread.request_stop();
        this->input_queue->stop();
        this->output_frame_queue->stop();
        if (filter_thread.joinable())
            this->filter_thread.join();
    }

    bool VideoFilter::is_opened() const
    {
        return this->filter_graph != nullptr;
    }

    bool VideoFilter::is_running() const
    {
        return this->is_thread_running;
    }

    void VideoFilter::build_graph(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx)
    {
        auto graph = AVFilterGraphPtr(avfilter_graph_alloc());
        if (!graph)
            throw FFmpegException("Failed to allocate filter graph");

        const AVFilter *buffersrc = avfilter_get_by_name("buffer");
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");

        // ---- buffersrc ----
        AVFilterContext *src_ctx = avfilter_graph_alloc_filter(graph.get(), buffersrc, "in");
        if (!src_ctx)
            throw FFmpegException("Failed to allocate buffer source");

        auto param_deleter = [](AVBufferSrcParameters *p)
        {
            av_buffer_unref(&p->hw_frames_ctx);
            av_free(p);
        };
        auto src_params = std::unique_ptr<AVBufferSrcParameters, decltype(param_deleter)>(
            av_buffersrc_parameters_alloc(), param_deleter);
        if (!src_params)
            throw FFmpegException("Failed to allocate buffer source parameters");

        src_params->format = dec_ctx->pix_fmt;
        src_params->width = dec_ctx->width;
        src_params->height = dec_ctx->height;
        src_params->time_base = dec_ctx->pkt_timebase;
        src_params->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;

        if (dec_ctx->hw_frames_ctx)
        {
            src_params->hw_frames_ctx = av_buffer_ref(dec_ctx->hw_frames_ctx);
            if (!src_params->hw_frames_ctx)
                throw FFmpegException("Failed to ref hw_frames_ctx");
        }

        int ret = av_buffersrc_parameters_set(src_ctx, src_params.get());
        if (ret < 0)
            throw FFmpegException(ret, "Failed to set buffer source parameters");
        ret = avfilter_init_dict(src_ctx, nullptr);
        if (ret < 0)
            throw FFmpegException(ret, "Failed to initialize buffer source");

        // ---- buffersink ----
        AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(graph.get(), buffersink, "out");
        if (!sink_ctx)
            throw FFmpegException("Failed to allocate buffer sink");

        AVPixelFormat pix_fmts[] = {enc_ctx->pix_fmt, AV_PIX_FMT_NONE};
        ret = av_opt_set_array(sink_ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_PIXEL_FMT, pix_fmts);
        if (ret < 0)
            throw FFmpegException(ret, "Failed to set output pixel format");

        if (enc_ctx->hw_device_ctx)
        {
            sink_ctx->hw_device_ctx = av_buffer_ref(enc_ctx->hw_device_ctx);
            if (!sink_ctx->hw_device_ctx)
                throw FFmpegException("Failed to ref encoder hw_device_ctx");
        }

        ret = avfilter_init_dict(sink_ctx, nullptr);
        if (ret < 0)
            throw FFmpegException(ret, "Failed to initialize buffer sink");

        // ================================================================
        //  manually build intermediate filter chain
        // ================================================================

        auto [in_ctx, out_ctx] = build_filter_chain(graph.get(), dec_ctx, enc_ctx);

        // ---- final link to sink ----
        if (in_ctx && out_ctx)
        {
            link(src_ctx, in_ctx);
            link(out_ctx, sink_ctx);
        }
        else
        {
            link(src_ctx, sink_ctx); // 零拷贝直连
        }

        // ---- configure ----
        ret = avfilter_graph_config(graph.get(), nullptr);
        if (ret < 0)
        {
            std::cout << FFmpegException::error_message(ret) << std::endl;
            throw FFmpegException(ret, "Failed to configure filter graph");
        }

        this->filter_graph = std::move(graph);
        this->buffersrc_ctx = src_ctx;
        this->buffersink_ctx = sink_ctx;
    }

    void VideoFilter::filter_thread_func(std::stop_token token)
    {
        this->is_thread_running = true;
        this->logger.log("VideoFilter thread started");

        int64_t frame_count = 0;
        while (!token.stop_requested())
        {
            auto frame = this->input_queue->dequeue();
            if (frame.get() == nullptr)
            {
                break;
            }
            frame_count++;
            if (frame_count % 100 == 0)
            {
                logger.log(std::format("{},  receive frame: {}: {} {}x{}", frame_count, frame.get()->pts,
                                       frame.get()->format, frame.get()->width, frame.get()->height));
            }

            this->push_frame(frame.get());

            this->pull_frames(token);
        }

        // Drain filter graph
        this->push_frame(nullptr);
        this->pull_frames(token);

        this->output_frame_queue->stop();
        this->logger.log(std::format("VideoFilter thread exiting, total: {}", frame_count));
        this->is_thread_running = false;
    }

    void VideoFilter::push_frame(AVFrame *frame)
    {
        int result = av_buffersrc_add_frame_flags(this->buffersrc_ctx, frame,
                                                  AV_BUFFERSRC_FLAG_KEEP_REF);
        if (result < 0)
        {
            throw FFmpegException(result, "Failed to push frame to filter graph");
        }
    }

    void VideoFilter::pull_frames(std::stop_token token)
    {
        static int64_t total_frame = 0;
        while (!token.stop_requested())
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

            if (++total_frame % 100 == 0)
            {
                this->logger.log(std::format("{}, push frame: {}: {} {}x{}", total_frame, filtered_frame.get()->pts,
                                             filtered_frame.get()->format, filtered_frame.get()->width,
                                             filtered_frame.get()->height));
            }

            this->output_frame_queue->enqueue(std::move(filtered_frame));
        }
    }

    // ---------------------------------------------------------------------------
    // manual filter graph construction helpers
    // ---------------------------------------------------------------------------

    static bool hardware_scale_available(AVBufferRef *hw_frames_ctx,
                                         AVPixelFormat target_format,
                                         int width, int height)
    {
        auto *hwfc = reinterpret_cast<AVHWFramesContext *>(hw_frames_ctx->data);

        // 检查该硬件设备是否有对应的 GPU scale 滤镜
        std::string scaler = gpu_scale_filter(hwfc->device_ctx->type);
        if (scaler.empty())
            return false;

        AVFilterGraphPtr graph(avfilter_graph_alloc());
        if (!graph)
            return false;

        // ---- buffersrc ----
        AVFilterContext *src_ctx = avfilter_graph_alloc_filter(
            graph.get(), avfilter_get_by_name("buffer"), "src");
        if (!src_ctx)
            return false;

        auto param_deleter = [](AVBufferSrcParameters *p)
        {
            av_free(p);
        };
        auto params = std::unique_ptr<AVBufferSrcParameters, decltype(param_deleter)>(
            av_buffersrc_parameters_alloc(), param_deleter);
        if (!params)
            return false;

        params->format = hwfc->format; // 硬件格式（如 D3D11）
        params->width = width;
        params->height = height;
        params->time_base = {1, 90000};
        params->hw_frames_ctx = hw_frames_ctx; // 借用引用

        if (av_buffersrc_parameters_set(src_ctx, params.get()) < 0)
            return false;
        if (avfilter_init_dict(src_ctx, nullptr) < 0)
            return false;

        // ---- buffersink ----
        AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(
            graph.get(), avfilter_get_by_name("buffersink"), "sink");
        if (!sink_ctx)
            return false;

        AVPixelFormat pix_fmts[] = {target_format, AV_PIX_FMT_NONE};
        if (av_opt_set_array(sink_ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                             0, 1, AV_OPT_TYPE_PIXEL_FMT, pix_fmts) < 0)
            return false;
        if (avfilter_init_dict(sink_ctx, nullptr) < 0)
            return false;

        // ---- build chain ----
        const char *target_name = av_get_pix_fmt_name(target_format);

        auto *gpu_scale = create_filter(graph.get(), scaler.c_str(), "gpuscale",
                                        std::format("format={}", target_name).c_str(),
                                        nullptr);
        if (!gpu_scale)
            return false;
        link(src_ctx, gpu_scale);

        AVFilterContext *prev = gpu_scale;

        if (!is_hw_pix_fmt(target_format))
        {
            // 目标是软件格式 → 测试 GPU scale + hwdownload 完整链路
            auto *download = create_filter(graph.get(), "hwdownload", "download",
                                           nullptr, nullptr);
            if (!download)
                return false;
            link(prev, download);
            prev = download;
        }

        link(prev, sink_ctx);

        // ---- configure ----
        return avfilter_graph_config(graph.get(), nullptr) >= 0;
    }

    static AVFilterContext *create_filter(AVFilterGraph *graph, const char *name, const char *inst, const char *args,
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

    static std::tuple<AVFilterContext *, AVFilterContext *> build_filter_chain(AVFilterGraph *graph, const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx)
    {
        const bool dec_hw = (dec_ctx->hw_device_ctx != nullptr);
        const bool enc_hw = (enc_ctx->hw_device_ctx != nullptr);

        const AVPixelFormat dec_fmt = dec_ctx->pix_fmt;
        const AVPixelFormat enc_fmt = enc_ctx->pix_fmt;
        const AVPixelFormat dec_sw_fmt = dec_ctx->hw_frames_ctx ? reinterpret_cast<AVHWFramesContext *>(dec_ctx->hw_frames_ctx->data)->sw_format : AV_PIX_FMT_NONE;
        const AVPixelFormat enc_sw_fmt = enc_ctx->hw_device_ctx ? reinterpret_cast<AVHWFramesContext *>(enc_ctx->hw_frames_ctx->data)->sw_format : AV_PIX_FMT_NONE;

        const AVHWDeviceType dec_type = get_hwdevice_type(dec_ctx);
        const AVHWDeviceType enc_type = get_hwdevice_type(enc_ctx);

        AVFilterContext *in_ctx = nullptr;
        AVFilterContext *out_ctx = nullptr;

        // ── 路径1: 纯软件 ──────────────────────────────────────────────
        if (!dec_hw && !enc_hw)
        {
            if (dec_fmt != enc_fmt)
            {
                auto *format_filter = create_filter(graph, "format", "cpuformat", av_get_pix_fmt_name(enc_fmt), nullptr);
                in_ctx = format_filter;
                out_ctx = format_filter;
            }
        }
        // ── 路径2: 软解 + 硬编 ─────────────────────────────────────────
        else if (!dec_hw && enc_hw)
        {
            if (dec_fmt == enc_sw_fmt)
            {
                // CPU帧的格式正是编码器所需要的，无需转换
                auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                in_ctx = upload_filter;
                out_ctx = upload_filter;
            }
            else
            {
                // 不能直接上传，需要在CPU进行格式转换
                auto *format_filter = create_filter(graph, "format", "gpuformat", "extra_hw_frames=64", nullptr);
                auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                link(format_filter, upload_filter);
                in_ctx = format_filter;
                out_ctx = upload_filter;
            }
        }
        // ── 路径3: 硬解 + 软编 ─────────────────────────────────────────
        else if (dec_hw && !enc_hw)
        {
            // 首先检查解码器帧下载后的格式是否是编码器所需的格式
            if (dec_sw_fmt == enc_fmt)
            {
                auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                auto *format_filter = create_filter(graph, "format", "fmt", av_get_pix_fmt_name(enc_fmt), nullptr);
                link(download_filter, format_filter);
                in_ctx = download_filter;
                out_ctx = format_filter;
            }
            else
            {
                // 下载后的格式不是编码器需要的
                // 尝试在GPU进行格式转换
                if (!hardware_scale_available(dec_ctx->hw_frames_ctx,
                                              enc_fmt, dec_ctx->width, dec_ctx->height))
                {
                    // 无法在GPU进行格式转换，只能在CPU进行格式转换
                    auto download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                    auto download_format_filter = create_filter(graph, "format", "dlformat", av_get_pix_fmt_name(dec_sw_fmt), nullptr);
                    auto format_filter = create_filter(graph, "format", "gpuformat", av_get_pix_fmt_name(enc_fmt), nullptr);
                    link(download_filter, download_format_filter);
                    link(download_format_filter, format_filter);
                    in_ctx = download_filter;
                    out_ctx = format_filter;
                }
                else
                {
                    // 可以在GPU进行格式转换
                    auto gpu_scaler = gpu_scale_filter(dec_type);
                    auto gpu_format_filter = create_filter(graph, gpu_scaler.data(), "gpuformat",
                                                           std::format("format={}", av_get_pix_fmt_name(enc_fmt)).c_str(),
                                                           dec_ctx->hw_device_ctx);
                    auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                    link(gpu_format_filter, download_filter);
                    in_ctx = gpu_format_filter;
                    out_ctx = download_filter;
                }
            }
        }
        // ── 两端都是硬件 ──────────────────────────────────────────────
        else
        {
            // 路径4: 同 API
            if (dec_type == enc_type)
            {
                // 首先检查编码器帧的格式是否是解码器需要的格式
                if (dec_fmt == enc_fmt)
                {
                    // 零拷贝，无需转换和上传下载
                }
                else
                {
                    // 需要进行格式转换，尝试在GPU进行转换
                    if (!hardware_scale_available(dec_ctx->hw_frames_ctx,
                                                  enc_fmt, dec_ctx->width, dec_ctx->height))
                    {
                        // 无法在GPU进行格式转换，只能在CPU进行格式转换
                        auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                        auto *format_filter = create_filter(graph, "format", "cpuformat", av_get_pix_fmt_name(enc_sw_fmt), nullptr);
                        auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                        link(download_filter, format_filter);
                        link(format_filter, upload_filter);
                        in_ctx = download_filter;
                        out_ctx = upload_filter;
                    }
                    else
                    {
                        // 可以在GPU进行格式转换
                        const auto gpu_scaler = gpu_scale_filter(dec_type);
                        auto gpu_format_filter = create_filter(graph, gpu_scaler.data(), "gpuformat",
                                                               std::format("format={}", av_get_pix_fmt_name(enc_fmt)).c_str(),
                                                               dec_ctx->hw_device_ctx);
                        in_ctx = gpu_format_filter;
                        out_ctx = gpu_format_filter;
                    }
                }
            }
            // 路径5: 跨 API ─ CPU 中转
            else
            {
                // 首先检查解码器帧下载后的格式是否是编码器所需的格式
                if (dec_sw_fmt == enc_sw_fmt)
                {
                    // 下载 → 显式声明格式 → 上传
                    auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                    auto *format_filter = create_filter(graph, "format", "fmt", av_get_pix_fmt_name(dec_sw_fmt), nullptr);
                    auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                    link(download_filter, format_filter);
                    link(format_filter, upload_filter);
                    in_ctx = download_filter;
                    out_ctx = upload_filter;
                }
                else
                {
                    // 格式不一致
                    // 首先尝试下载前在GPU上进行转换
                    if (hardware_scale_available(dec_ctx->hw_frames_ctx,
                                                 enc_sw_fmt, dec_ctx->width, dec_ctx->height))
                    {
                        // GPU 格式转换 → 下载 → 上传
                        const auto gpu_scaler = gpu_scale_filter(dec_type);
                        auto gpu_format_filter = create_filter(graph, gpu_scaler.data(), "gpuformat",
                                                               std::format("format={}", av_get_pix_fmt_name(enc_sw_fmt)).c_str(),
                                                               dec_ctx->hw_device_ctx);
                        auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                        auto *format_filter = create_filter(graph, "format", "format", av_get_pix_fmt_name(dec_sw_fmt), nullptr);
                        auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                        link(gpu_format_filter, download_filter);
                        link(download_filter, format_filter);
                        link(format_filter, upload_filter);
                        in_ctx = gpu_format_filter; // 链头是 GPU 滤镜，不是 download
                        out_ctx = upload_filter;
                    }
                    else
                    {
                        // 无法在下载前转换格式
                        // 尝试在上传后转换格式
                        // 只能在CPU进行格式转换
                        auto *download_filter = create_filter(graph, "hwdownload", "download", nullptr, nullptr);
                        auto *format_filter = create_filter(graph, "format", "cpuformat", av_get_pix_fmt_name(enc_sw_fmt), nullptr);
                        auto *upload_filter = create_filter(graph, "hwupload", "upload", "extra_hw_frames=64", enc_ctx->hw_device_ctx);
                        link(download_filter, format_filter);
                        link(format_filter, upload_filter);
                        in_ctx = download_filter;
                        out_ctx = upload_filter;
                    }
                }
            }
        }

        return {in_ctx, out_ctx};
    }

} // namespace Transcode
