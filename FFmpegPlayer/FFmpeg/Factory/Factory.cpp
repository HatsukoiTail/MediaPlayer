#include "Factory.h"

#include "Print.h"
#include "Util.h"

extern "C"
{
#include <libavutil/pixdesc.h>
}

#include <sstream>

AVFormatContext *open_format_context(const char *path)
{
    AVFormatContext *format_ctx = nullptr;
    int result = avformat_open_input(&format_ctx, path, nullptr, nullptr);
    if (result < 0)
    {
        print(Ansi::Red, "Fail to open avformat. {}", debug(result));
        if (format_ctx)
            avformat_close_input(&format_ctx);
        return nullptr;
    }
    if ((result = avformat_find_stream_info(format_ctx, nullptr)) < 0)
    {
        print(Ansi::Red, "fail to find stream info. {}", debug(result));
        avformat_close_input(&format_ctx);
        return nullptr;
    }
    return format_ctx;
}

AVFormatContext *open_format_context(void *opaque, ReadCallback read_callback, SeekCallback seek_callback)
{
    constexpr int buffer_size = 64 * 1024;

    int result = 0;
    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx)
    {
        print(Ansi::Red, "Fail to alloc avformat context.");
        return nullptr;
    }
    uint8_t *buffer = static_cast<uint8_t *>(av_malloc(buffer_size));
    if (!buffer)
    {
        print(Ansi::Red, "Fail to alloc avio buffer.");
        avformat_free_context(format_ctx);
        return nullptr;
    }
    AVIOContext *io_ctx = avio_alloc_context(buffer, buffer_size, 0, opaque, read_callback, nullptr, seek_callback);
    if (!io_ctx)
    {
        print(Ansi::Red, "Fail to alloc avio context.");
        av_free(buffer);
        avformat_free_context(format_ctx);
        return nullptr;
    }
    format_ctx->pb = io_ctx;

    if ((result = avformat_open_input(&format_ctx, nullptr, nullptr, nullptr)) < 0)
    {
        print(Ansi::Red, "Fail to open avformat. {}", debug(result));
        avio_context_free(&io_ctx);
        avformat_free_context(format_ctx);
        return nullptr;
    }

    if ((result = avformat_find_stream_info(format_ctx, nullptr)) < 0)
    {
        print(Ansi::Red, "fail to find stream info. {}", debug(result));
        avformat_close_input(&format_ctx);
        avio_context_free(&io_ctx);
        return nullptr;
    }
    return format_ctx;
}

AVCodecContext *open_codec_context(AVStream* stream)
{
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder)
    {
        print(Ansi::Red, "Can not find decoder.");
        return nullptr;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(decoder);
    if (!ctx)
    {
        print(Ansi::Red, "Fail to alloc codec context.");
        return nullptr;
    }

    int result = avcodec_parameters_to_context(ctx, stream->codecpar);
    if (result < 0)
    {
        print(Ansi::Red, "Fail to bind param to codec. {}", debug(result));
        avcodec_free_context(&ctx);
        return nullptr;
    }

    result = avcodec_open2(ctx, decoder, nullptr);
    if (result < 0)
    {
        print(Ansi::Red, "Fail to open codec. {}", debug(result));
        avcodec_free_context(&ctx);
        return nullptr;
    }

    if (ctx->time_base.num == 0)
    {
        ctx->time_base = stream->time_base;
    }

    return ctx;
}

SwsContext *open_sws_context(int src_width, int src_height, AVPixelFormat src_format, int dst_width, int dst_height, AVPixelFormat dst_format)
{
    SwsContext *sws_ctx = sws_getContext(src_width, src_height, src_format,
                                         dst_width, dst_height, dst_format,
                                         SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx)
    {
        print(Ansi::Red, "Impossible to create scale context for the conversion from {}, {}, {} to {}, {}, {}. ",
              av_get_pix_fmt_name(src_format), src_width, src_height,
              av_get_pix_fmt_name(dst_format), dst_width, dst_height);
        return nullptr;
    }
    return sws_ctx;
}

SwrContext *open_swr_context(AVChannelLayout *src_layout, AVSampleFormat src_format, int src_rate,
                             AVChannelLayout *dst_layout, AVSampleFormat dst_format, int dst_rate)
{
    SwrContext *swr_ctx = nullptr;
    int result = swr_alloc_set_opts2(&swr_ctx, dst_layout, dst_format, dst_rate,
                                     src_layout, src_format, src_rate,
                                     0, nullptr);
    if (result < 0)
    {
        print(Ansi::Red, "Fail to create swr context. {}", debug(result));
        return nullptr;
    }
    result = swr_init(swr_ctx);
    if (result < 0)
    {
        print(Ansi::Red, "Fail to init swr context. {}", debug(result));
        swr_free(&swr_ctx);
        return nullptr;
    }
    return swr_ctx;
}

AVFilterGraph *open_filter_graph(double speed, int sample_rate, AVSampleFormat sample_format, const AVChannelLayout *channel_layout)
{
    if (speed <= 0.0)
        return nullptr;

    if (!channel_layout || channel_layout->nb_channels < 0)
        return nullptr;

    std::string filter_desc;

    double remaining_speed = speed;
    std::ostringstream oss;

    while (remaining_speed < 0.5)
    {
        oss << "atempo=0.5,";
        remaining_speed /= 0.5;
    }
    while (remaining_speed > 2.0)
    {
        oss << "atempo=2.0,";
        remaining_speed /= 2.0;
    }
    oss << std::format("atempo={:.4f}", remaining_speed);
    filter_desc = oss.str();

    const char *filter_spec = filter_desc.c_str();

    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    if (!inputs || !outputs) {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return nullptr;
    }

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
    {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return nullptr;
    }

    // 创建 abuffer 和 abuffersink
    AVFilterContext *src_ctx = nullptr;
    AVFilterContext *sink_ctx = nullptr;

    char args[512];
    char channel_layout_str[64] = {0};

    int ret = 0;

    ret = av_channel_layout_describe(channel_layout, channel_layout_str, sizeof(channel_layout_str));
    if (ret < 0)
    {
        goto fail;
    }

    ret = snprintf(args, sizeof(args),
             "sample_rate=%d:sample_fmt=%s:channel_layout=%s",
             sample_rate,
             av_get_sample_fmt_name(sample_format),
             channel_layout_str);

    if (ret < 0 || static_cast<size_t>(ret) >= sizeof(args)) {
        goto fail;
    }

    ret = avfilter_graph_create_filter(&src_ctx, avfilter_get_by_name("abuffer"), "in", args, nullptr, graph);
    if (ret < 0 || !src_ctx) {
        print(Ansi::Red, "Fail to create src filter: {}", debug(ret));
        goto fail;
    }
    ret = avfilter_graph_create_filter(&sink_ctx, avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr, graph);
    if (ret < 0 || !sink_ctx) {
        goto fail;
    }

    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    outputs->name = av_strdup("in");
    outputs->filter_ctx = src_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    // 使用 FFmpeg 的 parser 创建 atempo filter chain
    ret = avfilter_graph_parse_ptr(graph, filter_spec, &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        print(Ansi::Red, "Fail to parse filter chain: {}", debug(ret));
        goto fail;
    }

    ret = avfilter_graph_config(graph, nullptr);
    if (ret < 0)
    {
        print(Ansi::Red, "Fail to config filter graph: {}", debug(ret));
        goto fail;
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return graph;

fail:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avfilter_graph_free(&graph);
    return nullptr;
}
