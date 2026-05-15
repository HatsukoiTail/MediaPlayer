#ifndef DECODER_H
#define DECODER_H

#include "DataModel.h"
#include "SmartStruct.h"

#include <functional>
#include <thread>

class Decoder
{
public:
    enum class State { Stopped, Running, Paused, Seeking, Closed };

public:
    Decoder(std::shared_ptr<Queue<AVPacketPointer>> packets);
    virtual ~Decoder();

public:
    bool open(AVStream* stream);
    void close();
    void run();
    void pause();
    void stop();
    void seek(int64_t timestamp, std::function<void(int64_t)> callback);

public:
    State state() const;
    std::shared_ptr<Queue<AVFramePointer>> data();

protected:
    virtual AVFramePointer process(AVFramePointer frame);

private:
    void decode_loop();
    bool send_packet(AVPacketPointer packet);

    AVFramePointer flush_remain(bool);

protected:
    int width() const;
    int height() const;
    AVPixelFormat pixel_format() const;
    int sample_rate() const;
    int channel_count() const;
    AVSampleFormat sample_format() const;

private:
    size_t frame_queue_size = 30;

private:
    int64_t seek_timestamp {-1};
    std::function<void(int64_t)> seek_callback;
    std::atomic<bool> is_need_flush {false}; // 刷新解码器
    State last_state; // 跳转之前的状态

private:
    std::atomic<State> decode_state {State::Closed};

private:
    std::thread decode_thread;

private:
    AVCodecContextPointer codec_ctx;
    std::shared_ptr<Queue<AVPacketPointer>> packets;
    std::shared_ptr<Queue<AVFramePointer>> frames;
};

#endif // DECODER_H
