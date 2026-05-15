#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H

#include "AudioDecoder.h"
#include "AudioRender.h"
#include "Demuxer.h"
#include "VideoDecoder.h"
#include "VideoRender.h"

class FFmpegPlayer
{
public:
    enum class State { Closed, Running, Stopped, Seeking, Paused };
public:
    FFmpegPlayer() = default;
    FFmpegPlayer(std::string_view path);

public:
    bool open(std::string_view path);
    bool open(void* udata, ReadCallback read_fn, SeekCallback seek_fn);
    void run();
    void pause();
    void stop();
    void close();
    void seek(const int64_t timestamp, std::function<void(int64_t)> callback);
    void speedup(double speed);
    void setVolume(double volume);

public:
    void setVideoFrameHandler(std::function<void(AVFramePointer)> callback);
    void setVideoFormat(const VideoFormat& video_format);
    void setAudioFormat(const AudioFormat& audio_format);
    VideoFormat defaultVideoFormat() const;
    AudioFormat defaultAudioFormat() const;
    int64_t duration() const;

public:
    State state() const;
    int64_t position() const;
    double speed() const;

private:
    std::atomic<State> status {State::Closed};
    double play_speed {1.0};

private:
    std::unique_ptr<Demuxer> demuxer;
    std::unique_ptr<AudioDecoder> audio_decoder;
    std::unique_ptr<VideoDecoder> video_decoder;
    std::unique_ptr<AudioRender> audio_render;
    std::unique_ptr<VideoRender> video_render; // VideoRender依赖于AudioRender的clock()方法，因此应先于AudioRender销毁
};

#endif // FFMPEGPLAYER_H
