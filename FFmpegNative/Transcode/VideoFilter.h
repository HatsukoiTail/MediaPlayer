#pragma once

#ifndef MEDIAPLAYER_VIDEOFILTER_H
#define MEDIAPLAYER_VIDEOFILTER_H

#include <memory>
#include <string>
#include <thread>

struct AVBufferRef;
struct AVCodecContext;
struct AVFilterContext;
struct AVFilterGraph;

#include "FrameQueue.h"
#include "HWDeviceContext.h"
#include "Logger.h"
#include "FilterGraph.h"

namespace Transcode
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
        void open(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx);
        void close();
        void set_input_queue(std::shared_ptr<FrameQueue> queue);
        std::shared_ptr<FrameQueue> output_queue() const;
        void start();
        void stop();

    public:
        bool is_opened() const;
        bool is_running() const;

    private:
        void filter_thread_func(std::stop_token token);
        void build_graph(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);
        void push_frame(AVFrame *frame);
        void pull_frames(std::stop_token token);

    private:
        AVFilterGraphPtr filter_graph;
        AVFilterContext *buffersrc_ctx = nullptr;
        AVFilterContext *buffersink_ctx = nullptr;

        Logger logger;
        std::shared_ptr<FrameQueue> input_queue;
        std::shared_ptr<FrameQueue> output_frame_queue;
        std::jthread filter_thread;
        bool is_thread_running = false;
    };

} // namespace Transcode

#endif // MEDIAPLAYER_VIDEOFILTER_H
