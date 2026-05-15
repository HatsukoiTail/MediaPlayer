#include "AudioRender.h"

#include "Print.h"

#include <cassert>
#include <thread>

// int sample_size(PaSampleFormat format)
// {
//     switch (format)
//     {
//     case paUInt8:
//         return 1;
//     case paInt16:
//         return 2;
//     case paInt32:
//         return 4;
//     case paFloat32:
//         return 4;
//     default:
//         return -1;
//     }
// };

AudioRender::AudioRender(std::shared_ptr<Queue<AVFramePointer>> frames)
    : frames{frames}
{}

AudioRender::~AudioRender()
{
    this->close();
    print(Ansi::BgGreen, "AudioRender delete!");
}

void AudioRender::start()
{
    // 该函数允许被继承
}

bool AudioRender::open(const AudioFormat &format)
{
    assert(this->audio_state.load() == State::Closed && "AudioRender is opened");
    // if (Pa_Initialize())
    // {
    //     print(Ansi::Red, "Fail to initialize PortAudio.");
    //     return false;
    // }
    // PaStream *stream;
    // auto error = Pa_OpenDefaultStream(&stream, 0, format.channels, to_pa_format(format.sampleFormat), format.sampleRate, paFramesPerBufferUnspecified, pa_callback, this);
    // if (error != paNoError)
    // {
    //     print(Ansi::Red, "Fail to open PortAudio.");
    //     return false;
    // }

    // // 缓存1s的样本
    // const size_t buffer_size = format.sampleRate * format.channels * sample_size(format.sampleFormat) / 10;
    // this->buffer = std::make_unique<RingBuffer>(buffer_size);

    // this->stream = PaStreamPointer(stream);
    // this->format = format;
    // this->audio_state.store(State::Stopped);
    return true;
}

void AudioRender::close()
{
    assert(this->audio_state.load() != State::Closed && "AudioRender has not been opened.");
    // this->stop();
    // if (Pa_Terminate() != paNoError)
    // {
    //     print(Ansi::Red, "Fail to terminate PortAudio.");
    // }
    // this->buffer.reset();
    // this->stream.reset();
    // this->is_opened = false;
    this->audio_state.store(State::Closed);
}

void AudioRender::run()
{
    // Pa_StartStream(this->stream.get());
    this->audio_state.store(State::Running);
}

void AudioRender::stop()
{
    // Pa_StopStream(this->stream.get());
    this->audio_state.store(State::Stopped);
}

int64_t AudioRender::clock() const
{
    return this->time;
}

AudioRender::State AudioRender::state() const
{
    return this->audio_state.load();
}
