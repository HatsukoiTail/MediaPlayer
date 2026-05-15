#pragma once

#ifndef TRANSCODE_DECODER_H
#define TRANSCODE_DECODER_H

#include <functional>
#include <memory>
#include <thread>

#include "CodecContext.h"
#include "FrameQueue.h"
#include "Logger.h"
#include "PacketQueue.h"

namespace Transcode
{
    class Decoder
    {
    public:
        Decoder() = default;
        Decoder(const Decoder &) = delete;
        Decoder &operator=(const Decoder &) = delete;
        Decoder(Decoder &&) = delete;
        Decoder &operator=(Decoder &&) = delete;
        ~Decoder();

    public:
        void open(const AVStream *stream, AVBufferRef *hw_device_ctx = nullptr);
        void close();
        using HwFramesReadyCallback = std::function<void(const AVCodecContext *)>;
        void set_hwctx_ready_callback(HwFramesReadyCallback cb);
        AVBufferRef *hw_device_ctx() const;
        AVBufferRef *hw_frames_ctx() const;

        void set_input_queue(std::shared_ptr<PacketQueue> queue);
        std::shared_ptr<FrameQueue> output_queue() const;
        void start();
        void stop();

    public:
        bool is_opened() const;
        bool is_running() const;
        AVCodecContext *codec_context();

    private:
        void decode_thread_func(std::stop_token token);
        void receive_frames(std::stop_token token);

    private:
        AVCodecContextPtr codec_ctx;
        Logger logger;
        std::shared_ptr<PacketQueue> input_queue;
        std::shared_ptr<FrameQueue> output_frame_queue;
        std::jthread decode_thread;
        HwFramesReadyCallback hw_frames_ready_cb;
        bool hw_frames_signaled = false;
        volatile bool is_thread_running = false;
    };

} // namespace Transcode

#endif // TRANSCODE_DECODER_H
