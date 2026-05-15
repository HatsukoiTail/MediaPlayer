#include "Encoder.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include "FFmpegException.h"
#include "Frame.h"
#include "HWDeviceContext.h"
#include "Packet.h"
#include "TranscodeOptions.h"

static AVBufferRefPtr create_encoder_hw_device(const std::string &codec_name, AVBufferRef *existing_device);

namespace Transcode
{

    Encoder::~Encoder()
    {
        if (this->is_opened())
            this->close();
    }

    void Encoder::open(const VideoStreamOptions &options, const AVCodecContext *decoder_ctx)
    {
        assert(this->is_opened() == false);

        auto encoder = avcodec_find_encoder_by_name(options.codec.c_str());
        if (!encoder)
            throw FFmpegException(std::format("Encoder not found: {}", options.codec));

        auto ctx = avcodec_alloc_context3(encoder);
        if (!ctx)
            throw FFmpegException("Failed to allocate encoder context");
        this->codec_ctx = AVCodecContextPtr(ctx);

        this->configure_video(options, decoder_ctx);
        open_encode_context(this->codec_ctx.get(), this->codec_ctx->hw_device_ctx);

        this->output_packet_queue = std::make_shared<PacketQueue>();
        this->output_packet_queue->start();
        this->logger.open(std::format("../log/encoder_{}.log", this->codec_ctx->codec->name));
    }

    void Encoder::open(const AudioStreamOptions &options, AVBufferRef *hw_device_ctx)
    {
        assert(this->is_opened() == false);

        auto encoder = avcodec_find_encoder_by_name(options.codec.c_str());
        if (!encoder)
            throw FFmpegException(std::format("Encoder not found: {}", options.codec));

        auto ctx = avcodec_alloc_context3(encoder);
        if (!ctx)
            throw FFmpegException("Failed to allocate encoder context");
        this->codec_ctx = AVCodecContextPtr(ctx);

        this->configure_audio(options);
        open_encode_context(this->codec_ctx.get(), hw_device_ctx);

        this->output_packet_queue = std::make_shared<PacketQueue>();
        this->output_packet_queue->start();
        this->logger.open(std::format("../log/encoder_{}.log", this->codec_ctx->codec->name));
    }

    void Encoder::configure_video(const VideoStreamOptions &options, const AVCodecContext *decoder_ctx)
    {
        auto ctx = this->codec_ctx.get();

        bool is_hw_pix = is_hw_pix_fmt(options.pixel_format);
        AVPixelFormat sw_format = AV_PIX_FMT_NONE;
        if (is_hw_pix)
        {
            if (decoder_ctx->hw_frames_ctx == nullptr)
            {
                // 编码器为软解
                auto device = create_encoder_hw_device(options.codec, nullptr);
                ctx->hw_device_ctx = device.release();
                // sw_format = get_encoder_sw_pix_fmt(ctx->codec, ctx->hw_device_ctx);
                sw_format = AV_PIX_FMT_NV12;
            }
            else
            {
                auto hw_frames_ctx = reinterpret_cast<AVHWFramesContext *>(decoder_ctx->hw_frames_ctx->data);
                auto device = create_encoder_hw_device(options.codec, decoder_ctx->hw_device_ctx);
                ctx->hw_device_ctx = device.release();
                // 检查解码器的软件格式与编码器是否兼容
                if (encoder_support_format(ctx, nullptr, hw_frames_ctx->sw_format))
                {
                    sw_format = hw_frames_ctx->sw_format;
                }
                else
                {
                    sw_format = get_encoder_sw_pix_fmt(ctx->codec, ctx->hw_device_ctx);
                }
            }
        }

        ctx->width = options.width;
        ctx->height = options.height;
        ctx->pix_fmt = options.pixel_format;
        ctx->sw_pix_fmt = sw_format;
        ctx->gop_size = options.gop_size;

        ctx->framerate = av_d2q(options.frame_rate, 900000);
        ctx->time_base = {1, 90000};
        ctx->sample_aspect_ratio = {1, 1};

        if (options.bitrate >= 0)
            ctx->bit_rate = options.bitrate;
        if (options.crf >= 0)
            av_opt_set_int(ctx->priv_data, "crf", options.crf, 0);
    }

    void Encoder::configure_audio(const AudioStreamOptions &options)
    {
        auto ctx = this->codec_ctx.get();

        ctx->sample_rate = options.sample_rate;
        ctx->sample_fmt = options.sample_format;
        ctx->time_base = {1, options.sample_rate};

        av_channel_layout_from_string(&ctx->ch_layout, options.channel_layout.c_str());

        if (options.bitrate >= 0)
            ctx->bit_rate = options.bitrate;
    }

    void Encoder::close()
    {
        if (this->is_running())
            this->stop();
        this->codec_ctx.reset();
    }

    void Encoder::set_stream_index(int index)
    {
        this->stream_index = index;
    }

    void Encoder::set_input_queue(std::shared_ptr<FrameQueue> queue)
    {
        this->input_queue = std::move(queue);
    }

    std::shared_ptr<PacketQueue> Encoder::output_queue() const
    {
        return this->output_packet_queue;
    }

    void Encoder::start()
    {
        assert(this->is_opened());
        assert(this->is_running() == false);
        assert(this->input_queue != nullptr);
        assert(this->stream_index >= 0);
        this->output_packet_queue->start();
        this->encode_thread = std::jthread([this](std::stop_token token)
                                           { this->encode_thread_func(token); });
    }

    void Encoder::stop()
    {
        if (!this->encode_thread.joinable())
            return;
        this->encode_thread.request_stop();
        this->input_queue->stop();
        this->output_packet_queue->stop();
        this->encode_thread.join();
    }

    bool Encoder::is_opened() const
    {
        return this->codec_ctx != nullptr;
    }

    bool Encoder::is_running() const
    {
        return this->is_thread_running;
    }

    AVCodecContext *Encoder::codec_context()
    {
        return this->codec_ctx.get();
    }

    void Encoder::encode_thread_func(std::stop_token token)
    {
        this->is_thread_running = true;

        while (!token.stop_requested())
        {
            auto frame = this->input_queue->dequeue();
            if (frame.get() == nullptr)
                break;

            if (frame.get()->pts != AV_NOPTS_VALUE)
            {
                frame.get()->pts = av_rescale_q(frame.get()->pts,
                                                frame.get()->time_base,
                                                this->codec_ctx->time_base);
            }

            int ret = avcodec_send_frame(this->codec_ctx.get(), frame.get());
            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;

            this->receive_packets(token);
        }

        // Drain encoder
        avcodec_send_frame(this->codec_ctx.get(), nullptr);
        this->receive_packets(token);

        this->logger.log("=== Encoder thread stopped ===");

        this->output_packet_queue->stop();
        this->is_thread_running = false;
    }

    void Encoder::receive_packets(std::stop_token token)
    {
        while (true)
        {
            auto packet = Packet::create();
            int ret = avcodec_receive_packet(this->codec_ctx.get(), packet.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            packet.get()->stream_index = this->stream_index;
            packet.get()->time_base = this->codec_ctx->time_base;

            static int64_t packet_count = 0;
            if (++packet_count % 100 == 0)
            {
                this->logger.log(std::format("Encoded packet: pts={}", packet.get()->pts));
            }
            this->output_packet_queue->enqueue(std::move(packet));
        }
    }

} // namespace Transcode

static AVBufferRefPtr create_encoder_hw_device(const std::string &codec_name, AVBufferRef *existing_device)
{
    if (!is_hw_encoder(codec_name))
        return nullptr;

    auto encoder = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!encoder)
        return nullptr;

    // 确定编码器的主（原生）硬件设备类型
    AVHWDeviceType enc_hw_type = AV_HWDEVICE_TYPE_NONE;
    for (int i = 0;; i++)
    {
        const AVCodecHWConfig *cfg = avcodec_get_hw_config(encoder, i);
        if (!cfg)
            break;
        if (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            cfg->device_type != AV_HWDEVICE_TYPE_NONE)
        {
            enc_hw_type = cfg->device_type;
            break;
        }
    }

    if (enc_hw_type == AV_HWDEVICE_TYPE_NONE)
        return nullptr;

    // 仅当解码器设备类型与编码器主设备类型完全一致时才共享
    if (existing_device)
    {
        auto *hwdc = reinterpret_cast<AVHWDeviceContext *>(existing_device->data);
        if (hwdc->type == enc_hw_type)
            return AVBufferRefPtr(av_buffer_ref(existing_device));
    }

    AVBufferRef *hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, enc_hw_type, nullptr, nullptr, 0) < 0)
    {
        throw FFmpegException("Failed to allocate hw frames context");
    }
    return AVBufferRefPtr(hw_device_ctx);
}
