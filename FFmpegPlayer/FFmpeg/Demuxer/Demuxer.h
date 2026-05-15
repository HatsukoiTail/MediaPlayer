#ifndef DEMUXER_H
#define DEMUXER_H

#include "DataModel.h"
#include "Factory.h"
#include "SmartStruct.h"

#include <thread>

class Demuxer
{
public:
    enum class State { Running, Seeking, Stopped, Paused, Closed };
    enum class StreamType { Video, Audio };
public:
    Demuxer() = default;
    Demuxer(std::string_view path);
    Demuxer(void* udata, ReadCallback read_fn, SeekCallback seek_fn);
    ~Demuxer();

public:
    // 初始化相关资源，打开FFmpeg上下文，Opened
    bool open(std::string_view path);
    bool open(void* udata, ReadCallback read_fn, SeekCallback seek_fn);
    // 释放资源，Closed
    void close();
    // 启动解码线程，Running
    void run();
    // 停止解码线程，但不释放资源，随时可以启动并继续进行，Stopped
    void stop();
    // 暂停解码器，但不停止解码线程，而是阻塞线程，可以快速恢复，Paused
    void pause();
    void seek(int64_t timestamp);

public:
    State state() const;
    bool has_stream(StreamType type) const;
    std::shared_ptr<Queue<AVPacketPointer>> packets(StreamType type) const;
    AVStream *stream(StreamType type);
    int64_t duration() const;

private:
    void demux_loop();
    void do_seek();

private:
    size_t audio_queue_size = 30;
    size_t video_queue_size = 30;

private:
    State last_state;
    int64_t seek_timestamp;

private:
    std::atomic<State> demux_state { State::Closed };

private:
    std::thread demux_thread;

private:
    AVFormatContextPointer format_ctx;
    std::mutex ctx_mutex;
    std::shared_ptr<Queue<AVPacketPointer>> audio_packets;
    std::shared_ptr<Queue<AVPacketPointer>> video_packets;
    int audio_idx {-1};
    int video_idx {-1};
};

#endif // DEMUXER_H
