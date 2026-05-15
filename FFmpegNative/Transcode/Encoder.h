#pragma once

#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include <memory>
#include <thread>

#include "CodecContext.h"
#include "FrameQueue.h"
#include "Logger.h"
#include "PacketQueue.h"

struct AVBufferRef;
struct AVCodecContext;
struct VideoStreamOptions;
struct AudioStreamOptions;

namespace Transcode
{
    class Encoder
    {
    public:
        Encoder() = default;
        Encoder(const Encoder &) = delete;
        Encoder &operator=(const Encoder &) = delete;
        Encoder(Encoder &&) = delete;
        Encoder &operator=(Encoder &&) = delete;
        ~Encoder();

    public:
        void open(const VideoStreamOptions &options, const AVCodecContext *decoder_ctx);
        void open(const AudioStreamOptions &options, AVBufferRef *hw_device_ctx = nullptr);
        void close();
        void set_stream_index(int index);
        void set_input_queue(std::shared_ptr<FrameQueue> queue);
        std::shared_ptr<PacketQueue> output_queue() const;
        void start();
        void stop();

    public:
        bool is_opened() const;
        bool is_running() const;
        AVCodecContext *codec_context();

    private:
        void encode_thread_func(std::stop_token token);
        void receive_packets(std::stop_token token);

        void configure_video(const VideoStreamOptions &options, const AVCodecContext *decoder_ctx);
        void configure_audio(const AudioStreamOptions &options);

    private:
        AVCodecContextPtr codec_ctx;
        Logger logger;
        int stream_index = -1;

        std::shared_ptr<FrameQueue> input_queue;
        std::shared_ptr<PacketQueue> output_packet_queue;
        std::jthread encode_thread;
        volatile bool is_thread_running = false;
    };

} // namespace Transcode

#endif // FFMPEG_ENCODER_H
