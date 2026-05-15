#include "Demuxer.h"

#include <cassert>
#include <stdexcept>

#include "FFmpegException.h"
#include "Packet.h"

namespace Transcode
{

    Demuxer::~Demuxer()
    {
        this->close();
    }

    void Demuxer::open(std::string_view file_path)
    {
        assert(this->is_opened() == false);
        this->logger.open("../log/demux.log");

        this->format_ctx.open_in(file_path);

        for (int i = 0; i < this->format_ctx.stream_count(); i++)
        {
            auto codec_name = std::string(avcodec_get_name(format_ctx.stream(i)->codecpar->codec_id));
            logger.log(std::format("Stream {}: {}, time_base: {}/{}", i, codec_name, format_ctx.stream(i)->time_base.num, format_ctx.stream(i)->time_base.den));
            this->stream_queues[i] = std::make_shared<PacketQueue>();
        }

        // 启动所有队列
        for (auto &[_, queue] : this->stream_queues)
        {
            queue->start();
        }
    }

    void Demuxer::close()
    {
        this->stop();
        this->stream_queues.clear();
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
        for (auto &[index, queue] : this->stream_queues)
        {
            queue->stop();
        }
        this->demux_thread.join();
    }

    bool Demuxer::is_opened() const
    {
        return this->format_ctx.is_opened();
    }

    bool Demuxer::is_running() const
    {
        return this->is_thread_running;
    }

    int Demuxer::stream_count() const
    {
        return this->format_ctx.stream_count();
    }

    AVStream *Demuxer::stream(int index)
    {
        return this->format_ctx.stream(index);
    }

    std::shared_ptr<PacketQueue> Demuxer::stream_queue(int index) const
    {
        auto it = this->stream_queues.find(index);
        assert(it != this->stream_queues.end());
        return it->second;
    }

    void Demuxer::demux_thread_func(std::stop_token token)
    {
        this->is_thread_running = true;

        this->logger.log(std::format("Demux thread started, {} streams",
                                     this->stream_queues.size()));

        int64_t packet_count = 0;
        int64_t video_packet_count = 0;
        int64_t audio_packet_count = 0;
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
                throw FFmpegException(this->format_ctx.get()->pb->error, "Failed to read frame");
            }

            if (packet.get()->stream_index == 0)
                video_packet_count++;
            else
                audio_packet_count++;

            auto it = this->stream_queues.find(packet.stream_index());
            if (it != this->stream_queues.end())
            {
                if (it->second->enqueue(std::move(packet)) == false)
                {
                    this->logger.log("fail to push packet");
                    break;
                }
            }
            else
            {
                throw std::out_of_range("Stream index out of range.");
            }

            if ((++packet_count % 100) == 0)
            {
                this->logger.log(std::format("Demux progress: {} packets read, {} vp, {} ap", packet_count, video_packet_count, audio_packet_count));
            }
        }

        for (auto &[index, queue] : this->stream_queues)
            queue->stop();

        this->logger.log("Demux thread exiting");
        this->is_thread_running = false;
    }

} // namespace Transcode