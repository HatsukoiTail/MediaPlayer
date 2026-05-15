#pragma once

#ifndef MEDIAPLAYER_VIDEOFILTER_H
#define MEDIAPLAYER_VIDEOFILTER_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>

extern "C"
{
#include <libavutil/pixfmt.h>
}

struct AVBufferRef;
struct AVCodecContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVFrame;

#include "FilterGraph.h"
#include "FrameQueue.h"
#include "HWDeviceContext.h"
#include "Logger.h"

namespace MediaPlayer
{

class VideoFilter
{
public:
    VideoFilter() = default;
    VideoFilter(const VideoFilter &) = delete;
    VideoFilter &operator=(const VideoFilter &) = delete;
    VideoFilter(VideoFilter &&) = delete;
    VideoFilter &operator=(VideoFilter &&) = delete;
    ~VideoFilter();

public:
    void open(AVCodecContext *dec_ctx);

    // 设置渲染端目标格式（如果渲染器端可以接受多种格式，选最优的）
    void set_target_format(AVPixelFormat fmt, int w = -1, int h = -1);

    // 可选：渲染器所在的 GPU 设备（用于硬件帧直通）
    void set_render_hw_device(AVBufferRef *device);

    void close();
    void set_input_queue(std::shared_ptr<FrameQueue> queue);
    std::shared_ptr<FrameQueue> output_queue() const;
    void start();
    void stop();
    void flush();   // seek 后重建滤镜图

public:
    bool is_opened() const;
    bool is_running() const;

private:
    void filter_thread_func(std::stop_token token);
    void build_graph(const AVFrame *sample_frame);
    bool format_matches(const Frame &frame) const;

    void push_frame(AVFrame *frame);
    void pull_frames(std::stop_token token);

private:
    AVFilterGraphPtr filter_graph;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;

    AVCodecContext *dec_ctx = nullptr;  // borrowed
    AVPixelFormat target_fmt = AV_PIX_FMT_YUV420P;
    int target_w = -1;
    int target_h = -1;
    AVBufferRef *render_hw_device = nullptr;  // borrowed

    std::shared_ptr<FrameQueue> input_queue;
    std::shared_ptr<FrameQueue> output_frame_queue;
    std::jthread filter_thread;
    std::atomic<bool> is_thread_running{false};

    // 格式匹配状态
    int last_fmt = -1;
    int last_w  = -1;
    int last_h  = -1;
    Logger logger;
};

} // namespace MediaPlayer

#endif // MEDIAPLAYER_VIDEOFILTER_H
