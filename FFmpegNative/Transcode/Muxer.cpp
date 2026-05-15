#include "Muxer.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include "FFmpegException.h"
#include "Packet.h"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static AVCodecID detect_image_codec(std::span<const uint8_t> data)
{
    if (data.size() < 4)
        return AV_CODEC_ID_NONE;

    // JPEG
    if (data[0] == 0xFF && data[1] == 0xD8)
        return AV_CODEC_ID_MJPEG;

    // PNG
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return AV_CODEC_ID_PNG;

    return AV_CODEC_ID_NONE;
}

static void write_cover(AVFormatContext *fmt_ctx, std::span<const uint8_t> data);

// ===========================================================================

namespace Transcode
{

Muxer::~Muxer()
{
    if (this->is_opened())
        this->close();
}

void Muxer::open(std::string_view output_file, std::string_view format)
{
    assert(this->is_opened() == false);

    this->format_ctx.open_out(output_file, format);
    this->header_written = false;
    this->current_progress = std::numeric_limits<double>::quiet_NaN();
    this->has_eof = false;
    this->logger.open("../log/mux.log");
}

void Muxer::close()
{
    this->stop();
    this->format_ctx.close();
}

AVStream *Muxer::add_stream(const AVCodecContext *enc_ctx)
{
    assert(this->is_opened());
    return this->format_ctx.add_stream(enc_ctx);
}

void Muxer::set_input_queue(int stream_index, std::shared_ptr<PacketQueue> queue)
{
    assert(this->is_opened() && this->is_running() == false);
    this->input_queues[stream_index] = std::move(queue);
}

void Muxer::set_metadata(const std::map<std::string, std::string> &meta)
{
    assert(this->is_opened() && this->is_running() == false);
    for (const auto &[key, value] : meta)
        av_dict_set(&this->format_ctx.get()->metadata, key.c_str(), value.c_str(), 0);
}

void Muxer::set_cover(const std::vector<uint8_t> &data)
{
    assert(this->is_opened() && this->is_running() == false);
    this->cover_data = data;
}

void Muxer::start()
{
    assert(this->is_opened());
    assert(this->is_running() == false);
    this->mux_thread = std::jthread([this](std::stop_token token)
                                    { this->mux_thread_func(token); });
}

void Muxer::stop()
{
    if (!this->mux_thread.joinable())
        return;
    this->mux_thread.request_stop();
    for (auto &[index, queue] : this->input_queues)
        queue->stop();
    this->mux_thread.join();
}

bool Muxer::eof() const
{
    return this->has_eof;
}

double Muxer::progress() const
{
    return this->current_progress;
}

bool Muxer::is_opened() const
{
    return this->format_ctx.is_opened();
}

bool Muxer::is_running() const
{
    return this->is_thread_running;
}

void Muxer::mux_thread_func(std::stop_token token)
{
    this->is_thread_running = true;
    this->logger.log(std::format("Muxer thread started, {} streams", this->input_queues.size()));

    using namespace std::chrono_literals;

    // --- cover stream (before write_header) ---
    if (!this->cover_data.empty())
    {
        write_cover(this->format_ctx.get(), this->cover_data);
        this->cover_data.clear();
    }

    if (!this->header_written)
    {
        this->format_ctx.write_header();
        this->header_written = true;
    }

    int64_t packet_count = 0;
    while (!token.stop_requested())
    {
        bool wrote = false;

        for (auto &[stream_index, queue] : this->input_queues)
        {
            Packet packet;
            if (!queue->try_dequeue(packet))
                continue;

            this->write_packet(packet.get());
            wrote = true;
            packet_count++;
            break;
        }

        if (!wrote)
        {
            if (this->all_queues_drained())
                break;
            std::this_thread::sleep_for(1ms);
        }
    }

    this->format_ctx.write_trailer();
    this->has_eof = true;
    this->logger.log(std::format("Muxer thread exiting, {} packets written", packet_count));
    this->is_thread_running = false;
}

void Muxer::write_packet(AVPacket *packet)
{
    static int64_t v_count = 0;
    static int64_t a_count = 0;

    int stream_index = packet->stream_index;
    auto stream = this->format_ctx.stream(stream_index);

    int64_t &count = (stream_index == 0) ? v_count : a_count;

    if (count < 3 || count % 100 == 0)
    {
        this->logger.log(std::format(
            "mux pre-rescale #{}: stream={}, PTS={}, DTS={}, pkt_tb={}/{}, out_tb={}/{}",
            count, stream_index, packet->pts, packet->dts,
            packet->time_base.num, packet->time_base.den,
            stream->time_base.num, stream->time_base.den));
    }

    av_packet_rescale_ts(packet, packet->time_base, stream->time_base);
    packet->time_base = stream->time_base;

    if (count < 3 || count % 100 == 0)
    {
        this->logger.log(std::format(
            "mux post-rescale #{}: stream={}, PTS={}, DTS={}, tb={}/{}",
            count, stream_index, packet->pts, packet->dts,
            packet->time_base.num, packet->time_base.den));
    }

    count++;

    this->current_progress = static_cast<double>(packet->pts) * av_q2d(stream->time_base);
    int ret = av_interleaved_write_frame(this->format_ctx.get(), packet);
    if (ret < 0)
        throw FFmpegException(ret, "Failed to write packet");
}

bool Muxer::all_queues_drained() const
{
    for (auto &[index, queue] : this->input_queues)
    {
        if (!queue->is_stopped() || queue->count() > 0)
            return false;
    }
    return true;
}

} // namespace Transcode

static void write_cover(AVFormatContext *fmt_ctx, std::span<const uint8_t> data)
{
    AVCodecID codec_id = detect_image_codec(data);
    if (codec_id != AV_CODEC_ID_NONE)
    {
        auto *cover_stream = avformat_new_stream(fmt_ctx, nullptr);
        if (cover_stream)
        {
            cover_stream->disposition |= AV_DISPOSITION_ATTACHED_PIC;
            cover_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            cover_stream->codecpar->codec_id = codec_id;

            // Decode header to get dimensions
            auto decoder = avcodec_find_decoder(codec_id);
            if (decoder)
            {
                AVCodecContext *dec_ctx = avcodec_alloc_context3(decoder);
                if (dec_ctx && avcodec_open2(dec_ctx, decoder, nullptr) >= 0)
                {
                    AVPacket *pkt = av_packet_alloc();
                    av_new_packet(pkt, data.size());
                    memcpy(pkt->data, data.data(), data.size());

                    if (avcodec_send_packet(dec_ctx, pkt) >= 0)
                    {
                        AVFrame *frame = av_frame_alloc();
                        if (avcodec_receive_frame(dec_ctx, frame) >= 0)
                        {
                            cover_stream->codecpar->width = frame->width;
                            cover_stream->codecpar->height = frame->height;
                            cover_stream->codecpar->format = frame->format;
                            av_frame_free(&frame);
                        }
                    }
                    av_packet_free(&pkt);
                    avcodec_free_context(&dec_ctx);
                }
            }

            // Fallback dimensions if decoding failed
            if (cover_stream->codecpar->width <= 0)
                cover_stream->codecpar->width = 640;
            if (cover_stream->codecpar->height <= 0)
                cover_stream->codecpar->height = 640;

            // Set attached_pic with a fresh, ref-counted copy
            AVPacket *cover_pkt = av_packet_alloc();
            av_new_packet(cover_pkt, (int)data.size());
            memcpy(cover_pkt->data, data.data(), data.size());

            av_packet_unref(&cover_stream->attached_pic);
            av_packet_move_ref(&cover_stream->attached_pic, cover_pkt);
            av_packet_free(&cover_pkt);
        }
    }
}