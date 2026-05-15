#include "Decoder.h"

#include <cassert>
#include <stdexcept>

extern "C"
{
#include <libavutil/pixdesc.h>
}

#include "FFmpegException.h"
#include <iostream>

namespace Transcode
{

    Decoder::~Decoder()
    {
        this->close();
    }

    void Decoder::open(const AVStream *stream, AVBufferRef *hw_device_ctx)
    {
        assert(this->is_opened() == false);

        this->codec_ctx = open_decode_context(stream, hw_device_ctx);

        // 设置codec_ctx参数
        if (this->codec_ctx->pkt_timebase.num == 0 || this->codec_ctx->pkt_timebase.den == 0)
            this->codec_ctx->pkt_timebase = stream->time_base;

        this->output_frame_queue = std::make_shared<FrameQueue>();
        this->output_frame_queue->start();

        this->logger.open(std::format("../log/decode_{}.txt", this->codec_ctx.get()->codec->name));
    }

    void Decoder::close()
    {
        this->stop();
        this->codec_ctx.reset();
        this->input_queue.reset();
        this->output_frame_queue.reset();

        this->logger.log("=== Decoder closed ===");
    }

    void Decoder::set_input_queue(std::shared_ptr<PacketQueue> queue)
    {
        this->input_queue = std::move(queue);
    }

    void Decoder::set_hwctx_ready_callback(HwFramesReadyCallback cb)
    {
        this->hw_frames_ready_cb = std::move(cb);
    }

    AVBufferRef *Decoder::hw_device_ctx() const
    {
        assert(this->is_opened());
        return this->codec_ctx.get()->hw_device_ctx;
    }

    AVBufferRef *Decoder::hw_frames_ctx() const
    {
        assert(this->is_opened());
        assert(this->hw_frames_signaled);
        return this->codec_ctx.get()->hw_frames_ctx;
    }

    std::shared_ptr<FrameQueue> Decoder::output_queue() const
    {
        return this->output_frame_queue;
    }

    void Decoder::start()
    {
        assert(this->is_opened());
        assert(this->is_running() == false);
        assert(this->input_queue != nullptr);
        assert(this->output_frame_queue != nullptr);

        this->output_frame_queue->start();
        this->decode_thread = std::jthread([this](std::stop_token token)
                                           { this->decode_thread_func(token); });
    }

    void Decoder::stop()
    {
        if (!this->decode_thread.joinable())
            return;
        this->decode_thread.request_stop();
        this->input_queue->stop();
        this->output_frame_queue->stop();
        this->decode_thread.join();
    }

    bool Decoder::is_opened() const
    {
        return this->codec_ctx != nullptr;
    }

    bool Decoder::is_running() const
    {
        return this->is_thread_running;
    }

    AVCodecContext *Decoder::codec_context()
    {
        return this->codec_ctx.get();
    }

    void Decoder::decode_thread_func(std::stop_token token)
    {
        this->is_thread_running = true;
        this->logger.log("Decode thread started");

        while (!token.stop_requested())
        {
            this->receive_frames(token);

            auto packet = this->input_queue->dequeue();
            if (packet.get() == nullptr)
            {
                break;
            }

            static thread_local int log_count = 10;
            auto hw_frames_ctx = this->codec_ctx.get()->hw_frames_ctx ? reinterpret_cast<AVHWFramesContext *>(this->codec_ctx.get()->hw_frames_ctx->data) : nullptr;
            if (log_count-- > 0)
            {
                this->logger.log(std::format("hw frames ctx: {}", (size_t)hw_frames_ctx));
            }

            int ret = avcodec_send_packet(this->codec_ctx.get(), packet.get());
            if (ret < 0 && ret != AVERROR(EAGAIN))
            {
                throw FFmpegException(ret, "Failed to send packet to decoder");
            }
        }

        this->logger.log("Decode thread stopped");

        // Drain decoder
        auto flush_packet = Packet::create();
        avcodec_send_packet(this->codec_ctx.get(), nullptr);
        this->receive_frames(token);

        this->output_frame_queue->stop();
        this->is_thread_running = false;
    }

    void Decoder::receive_frames(std::stop_token token)
    {
        while (token.stop_requested() == false)
        {
            auto frame = Frame::create();
            auto codec_ctx = this->codec_ctx.get();
            int ret = avcodec_receive_frame(this->codec_ctx.get(), frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            if (ret < 0)
            {
                throw FFmpegException(ret, "Failed to receive frame");
            }

            // 首帧解码后 hw_frames_ctx 生效，通知外部
            if (!this->hw_frames_signaled && this->hw_frames_ready_cb)
            {
                this->logger.log(std::format("HW frames ready: {}", (size_t)this->codec_ctx.get()->hw_frames_ctx));
                this->hw_frames_signaled = true;
                this->hw_frames_ready_cb(this->codec_ctx.get());
            }

            frame.get()->pts = frame.get()->best_effort_timestamp;
            frame.get()->time_base = this->codec_ctx.get()->pkt_timebase;

            thread_local static uint64_t frame_count = 0;
            if (++frame_count % 50 == 0)
            {
                this->logger.log(std::format("Decoded {} frames, format = {}", frame_count, av_get_pix_fmt_name((AVPixelFormat)(frame.get()->format))));
            }

            this->output_frame_queue->enqueue(std::move(frame));
        }
    }

} // namespace Transcode