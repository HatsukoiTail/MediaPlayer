#include "Demuxer.h"

#include "Factory.h"
#include "Print.h"
#include "Util.h"

#include <cassert>

Demuxer::Demuxer(std::string_view path)
{
    this->open(path);
}

Demuxer::Demuxer(void *udata, ReadCallback read_fn, SeekCallback seek_fn)
{
    this->open(udata, read_fn, seek_fn);
}

Demuxer::~Demuxer()
{
    this->close();
    print(Ansi::BgGreen, "Demuxer delete!");
}

// 不可重入函数，需确保调用open前为Closed状态
bool Demuxer::open(std::string_view path)
{
    assert(this->demux_state.load() == State::Closed && "Cannot be opened again.");
    this->format_ctx = AVFormatContextPointer(open_format_context(path.data()));
    if (!this->format_ctx)
        return false;
    this->audio_idx = av_find_best_stream(this->format_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    this->video_idx = av_find_best_stream(this->format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (this->audio_idx < 0 && this->video_idx < 0)
    {
        print(Ansi::Red, "Fail to find stream info. Audio idx = {}, Video idx = {}.", this->audio_idx, this->video_idx);
        this->audio_idx = 0;
        this->video_idx = 0;
        return false;
    }
    if (this->audio_idx >= 0)
    {
        this->audio_packets = std::make_shared<Queue<AVPacketPointer>>(this->audio_queue_size);
    }
    if (this->video_idx >= 0)
    {
        this->video_packets = std::make_shared<Queue<AVPacketPointer>>(this->video_queue_size);
    }
    this->demux_state.store(State::Stopped);
    return true;
}

bool Demuxer::open(void *udata, ReadCallback read_fn, SeekCallback seek_fn)
{
    assert(this->demux_state.load() == State::Closed && "Cannot be opened again.");
    this->format_ctx = AVFormatContextPointer(open_format_context(udata, read_fn, seek_fn));
    if (!this->format_ctx)
        return false;
    this->audio_idx = av_find_best_stream(this->format_ctx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    this->video_idx = av_find_best_stream(this->format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (this->audio_idx < 0 && this->video_idx < 0)
    {
        print(Ansi::Red, "Fail to find stream info. Audio idx = {}, Video idx = {}.", this->audio_idx, this->video_idx);
        this->audio_idx = 0;
        this->video_idx = 0;
        return false;
    }
    if (this->audio_idx >= 0)
    {
        this->audio_packets = std::make_shared<Queue<AVPacketPointer>>(this->audio_queue_size);
    }
    if (this->video_idx >= 0)
    {
        this->video_packets = std::make_shared<Queue<AVPacketPointer>>(this->video_queue_size);
    }
    this->demux_state.store(State::Stopped);
    return true;
}

// 可重入函数
void Demuxer::close()
{
    this->stop();
    this->audio_packets.reset();
    this->video_packets.reset();
    this->format_ctx.reset();
    this->audio_idx = -1;
    this->video_idx = -1;
    this->demux_state.store(State::Closed);
}

void Demuxer::run()
{
    assert(this->demux_state.load() != State::Closed && "Needs to be opened first.");
    switch (this->demux_state.load()) {
    case State::Paused:
    {
        this->demux_state.store(State::Running);
        break;
    }
    case State::Stopped:
    {
        this->demux_thread = std::thread(&Demuxer::demux_loop, this);
        break;
    }
    default:
        break;
    }
}

void Demuxer::stop()
{
    this->demux_state.store(State::Stopped);

    if (this->demux_thread.joinable())
        this->demux_thread.join();
}

void Demuxer::pause()
{
    this->demux_state.store(State::Paused);
}

void Demuxer::seek(int64_t timestamp)
{
    assert(this->demux_state.load() != State::Closed && "Needs to be opened first.");
    if (this->demux_state.load() == State::Seeking)
        return;
    this->last_state = this->demux_state.load();
    this->seek_timestamp = timestamp;
    this->demux_state.store(State::Seeking);
}

bool Demuxer::has_stream(StreamType type) const
{
    assert(this->demux_state.load() != State::Closed);
    if (type == StreamType::Audio)
        return this->audio_idx >= 0;
    if (type == StreamType::Video)
        return this->video_idx >= 0;
    return false;
}

std::shared_ptr<Queue<AVPacketPointer> > Demuxer::packets(StreamType type) const
{
    switch (type)
    {
    case StreamType::Audio:
        return this->audio_packets;
    case StreamType::Video:
        return this->video_packets;
    default:
        return nullptr;
    }
}

AVStream *Demuxer::stream(StreamType type)
{
    assert(this->demux_state.load() != State::Closed);
    switch (type)
    {
    case StreamType::Audio:
    {
        if (this->audio_idx < 0)
            return nullptr;
        return this->format_ctx->streams[this->audio_idx];
    }
    case StreamType::Video:
    {
        if (this->video_idx < 0)
            return nullptr;
        return this->format_ctx->streams[this->video_idx];
    }
    default:
        return nullptr;
    }
}

int64_t Demuxer::duration() const
{
    assert(this->demux_state.load() != State::Closed);
    return this->format_ctx->duration;
}

Demuxer::State Demuxer::state() const
{
    return this->demux_state.load();
}

void Demuxer::demux_loop()
{
    this->demux_state.store(State::Running);
    AVPacketPointer packet;
    while (true)
    {
        auto state = this->demux_state.load();
        if (state == State::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (state == State::Stopped)
            break;

        if (state == State::Seeking)
        {
            this->do_seek();
            this->demux_state.store(this->last_state);
            packet.reset();
        }

        if (!packet)
        {
            packet = AVPacketPointer(av_packet_alloc());
            int result = 0;

            {
                std::lock_guard<std::mutex> locker(this->ctx_mutex);
                result = av_read_frame(this->format_ctx.get(), packet.get());
            }

            if (result == AVERROR_EOF)
            {
                if (this->audio_packets)
                    this->audio_packets->set_eof(true);
                if (this->video_packets)
                    this->video_packets->set_eof(true);
                break;
            }
            if (result < 0)
            {
                this->demux_state.store(State::Stopped);
                print(Ansi::Red, "Fail to read frame while demuxer running, {}", debug(result));
                break;
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (packet->stream_index == this->audio_idx)
        {
            this->audio_packets->push(std::move(packet));
        }
        else if (packet->stream_index == this->video_idx)
        {
            this->video_packets->push(std::move(packet));
        }
        else
        {
            print(Ansi::Red, "Unknown packet, stream_index = {}", packet->stream_index);
            packet.reset();
        }
    }
    this->demux_state.store(State::Stopped);
}

void Demuxer::do_seek()
{
    if (this->video_idx >= 0)
    {
        int64_t target_pts = av_rescale_q(this->seek_timestamp, AVRational{1, 1000}, this->format_ctx->streams[this->video_idx]->time_base);
        av_seek_frame(this->format_ctx.get(), this->video_idx, target_pts, AVSEEK_FLAG_BACKWARD);
    }
    else
    {
        int64_t target_pts = av_rescale_q(this->seek_timestamp, AVRational{1, 1000}, this->format_ctx->streams[this->audio_idx]->time_base);
        av_seek_frame(this->format_ctx.get(), this->audio_idx, target_pts, AVSEEK_FLAG_BACKWARD);
    }

    if (this->audio_packets)
        this->audio_packets->clear();
    if (this->video_packets)
        this->video_packets->clear();

    if (this->audio_packets)
        this->audio_packets->set_eof(false);
    if (this->video_packets)
        this->video_packets->set_eof(false);
}
