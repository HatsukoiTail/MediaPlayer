#ifndef AUDIORENDER_H
#define AUDIORENDER_H

#include "DataModel.h"
#include "Format.h"
#include "SmartStruct.h"

#include <atomic>

class AudioRender
{
public:
    enum class State { Running, Stopped, Closed, Paused };
public:
    AudioRender(std::shared_ptr<Queue<AVFramePointer>> frames);
    ~AudioRender();

public:
    bool open(const AudioFormat& format);
    void close();
    void run();
    void stop();
    void pause();
    void setVolume(double volume);

public:
    int64_t clock() const;
    State state() const;

private:
    PaStreamCallbackResult fill_audio(void *buffer, unsigned long frames);
    static int pa_callback(const void *input, void *output, unsigned long frame_count,
                           const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags,
                           void *user_data);
    void apply_volume(void* buffer, unsigned long frames);

private:
    AudioFormat format;
    int64_t time {0};

private:
    std::atomic<State> audio_state {State::Closed};
    std::atomic<double> volume { 1.0 };

public:
    PaStreamPointer stream;
    std::shared_ptr<Queue<AVFramePointer>> frames;
    std::unique_ptr<RingBuffer> buffer;
};

#endif // AUDIORENDER_H
