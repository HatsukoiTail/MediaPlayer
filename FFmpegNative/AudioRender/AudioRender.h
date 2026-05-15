#ifndef AUDIORENDER_H
#define AUDIORENDER_H

#include "DataModel.h"
#include "Format.h"
#include "SmartStruct.h"

#include <atomic>

class AudioRender
{
public:
    enum class State
    {
        Running,
        Stopped,
        Closed
    };

public:
    AudioRender(std::shared_ptr<Queue<AVFramePointer>> frames);
    ~AudioRender();

public:
    virtual void start();

protected:
    void update();
    bool open(const AudioFormat &format);
    void close();
    void run();
    void stop();

public:
    int64_t clock() const;
    State state() const;

private:
private:
    AudioFormat format;
    int64_t time{0};

private:
    std::atomic<State> audio_state{State::Closed};

public:
    // PaStreamPointer stream;
    std::shared_ptr<Queue<AVFramePointer>> frames;
    std::unique_ptr<RingBuffer> buffer;
};

#endif // AUDIORENDER_H
