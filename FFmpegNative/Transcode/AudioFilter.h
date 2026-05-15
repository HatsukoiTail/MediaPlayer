#pragma once

#ifndef FFMPEG_AUDIOFILTER_H
#define FFMPEG_AUDIOFILTER_H

#include <memory>
#include <string>
#include <thread>

#include "FilterGraph.h"
#include "FrameQueue.h"
#include "Logger.h"

struct AVCodecContext;

namespace Transcode
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
        void open(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);
        void close();
        void set_input_queue(std::shared_ptr<FrameQueue> queue);
        std::shared_ptr<FrameQueue> output_queue() const;
        void start();
        void stop();

    public:
        bool is_opened() const;
        bool is_running() const;
        void set_logger(Logger logger);

    private:
        void filter_thread_func(std::stop_token token);
        void init_graph(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);
        std::string build_filter_desc(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);
        int push_frame(AVFrame *frame);
        void pull_frames();

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

#endif // FFMPEG_AUDIOFILTER_H
