#include "Demuxer.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "FFmpegException.h"
#include "Packet.h"

namespace MediaPlayer
{

Demuxer::~Demuxer()
{
    this->close();
}

void Demuxer::open(std::string_view file_path)
{
    assert(this->is_opened() == false);
    this->logger.open("../log/demux_player.log");

    this->format_ctx.open_in(file_path);

    for (int i = 0; i < this->format_ctx.stream_count(); i++)
    {
        auto *st = this->format_ctx.stream(i);

        StreamInfo info;
        info.index      = st->index;
        info.type       = st->codecpar->codec_type;
        info.codec_id   = st->codecpar->codec_id;
        info.codec_name = avcodec_get_name(st->codecpar->codec_id);
        info.time_base  = st->time_base;

        this->stream_info.push_back(info);
        this->stream_queues[i] = std::make_shared<PacketQueue>();
        this->active_streams.insert(i);  // 默认所有流活跃

        this->logger.log(std::format("Stream {}: {}({}), time_base: {}/{}",
                                     i, info.codec_name, av_get_media_type_string(info.type),
                                     info.time_base.num, info.time_base.den));
    }

    for (auto &[_, queue] : this->stream_queues)
        queue->start();
}

void Demuxer::close()
{
    this->stop();
    this->stream_queues.clear();
    this->stream_info.clear();
    this->active_streams.clear();
    this->format_ctx.close();
    this->logger.log("Demuxer closed");
}

void Demuxer::start()
{
    assert(this->is_opened());
    assert(!this->is_running());
    for (auto &[_, queue] : this->stream_queues)
        queue->start();
    this->demux_thread = std::jthread([this](std::stop_token token)
                                      { this->demux_thread_func(token); });
}

void Demuxer::stop()
{
    if (!this->demux_thread.joinable())
        return;
    this->demux_thread.request_stop();
    for (auto &[_, queue] : this->stream_queues)
        queue->stop();
    this->demux_thread.join();
}

void Demuxer::seek(double seconds)
{
    assert(this->is_opened());

    int64_t target = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int ret = av_seek_frame(this->format_ctx.get(), -1, target, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
        throw FFmpegException(ret, std::format("Seek failed to {}", seconds));

    this->flush();
    this->current_position = seconds;
    this->logger.log(std::format("Seeked to {}s", seconds));
}

void Demuxer::flush()
{
    for (auto &[_, queue] : this->stream_queues)
        queue->flush();
}

double Demuxer::duration() const
{
    assert(this->is_opened());
    return static_cast<double>(this->format_ctx.get()->duration) / AV_TIME_BASE;
}

double Demuxer::position() const
{
    return this->current_position.load();
}

std::vector<StreamInfo> Demuxer::streams() const
{
    return this->stream_info;
}

std::vector<StreamInfo> Demuxer::streams(AVMediaType type) const
{
    std::vector<StreamInfo> result;
    for (const auto &s : this->stream_info)
        if (s.type == type)
            result.push_back(s);
    return result;
}

AVStream *Demuxer::stream(int index)
{
    return this->format_ctx.stream(index);
}

void Demuxer::set_stream_active(int index, bool active)
{
    if (active)
        this->active_streams.insert(index);
    else
    {
        this->active_streams.erase(index);
        auto it = this->stream_queues.find(index);
        if (it != this->stream_queues.end())
            it->second->flush();
    }
}

bool Demuxer::is_stream_active(int index) const
{
    return this->active_streams.count(index) > 0;
}

std::shared_ptr<PacketQueue> Demuxer::stream_queue(int index) const
{
    auto it = this->stream_queues.find(index);
    assert(it != this->stream_queues.end());
    return it->second;
}

bool Demuxer::is_opened() const
{
    return this->format_ctx.is_opened();
}

bool Demuxer::is_running() const
{
    return this->is_thread_running.load();
}

// ===========================================================================

void Demuxer::demux_thread_func(std::stop_token token)
{
    this->is_thread_running = true;

    this->logger.log(std::format("Demux thread started, {} streams, {} active",
                                 this->stream_queues.size(), this->active_streams.size()));

    int64_t packet_count = 0;
    while (!token.stop_requested())
    {
        auto packet = Packet::create();
        int result = av_read_frame(this->format_ctx.get(), packet.get());
        if (result == AVERROR_EOF)
        {
            this->logger.log(std::format("Demux EOF, total packets: {}", packet_count));
            break;
        }
        if (result < 0)
        {
            throw FFmpegException(result, "Failed to read frame");
        }

        int stream_index = packet.stream_index();

        // 不活跃的流直接丢弃包
        if (this->active_streams.count(stream_index) == 0)
            continue;

        auto it = this->stream_queues.find(stream_index);
        if (it == this->stream_queues.end())
            continue;

        if (!it->second->try_enqueue(std::move(packet)))
        {
            this->logger.log("fail to push packet");
            break;
        }

        if ((++packet_count % 100) == 0)
        {
            this->logger.log(std::format("Demux progress: {} packets read", packet_count));
        }
    }

    for (auto &[_, queue] : this->stream_queues)
        queue->stop();

    this->logger.log("Demux thread exiting");
    this->is_thread_running = false;
}

} // namespace MediaPlayer
