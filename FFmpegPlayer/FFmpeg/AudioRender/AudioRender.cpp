#include "AudioRender.h"

#include "Print.h"

#include <cassert>
#include <thread>

AudioRender::AudioRender(std::shared_ptr<Queue<AVFramePointer>> frames)
    : frames{frames}
{}

AudioRender::~AudioRender()
{
    this->close();
    print(Ansi::BgGreen, "AudioRender delete!");
}

bool AudioRender::open(const AudioFormat &format)
{
    assert(this->audio_state.load() == State::Closed && "AudioRender is opened");
    if (Pa_Initialize())
    {
        print(Ansi::Red, "Fail to initialize PortAudio.");
        return false;
    }
    PaStream *stream;
    auto error = Pa_OpenDefaultStream(&stream, 0, format.channels, to_pa_format(format.sampleFormat), format.sampleRate, paFramesPerBufferUnspecified, pa_callback, this);
    if (error != paNoError)
    {
        print(Ansi::Red, "Fail to open PortAudio.");
        return false;
    }

    // 缓存1s的样本
    const size_t buffer_size = format.sampleRate * format.channels * sample_size(format.sampleFormat) / 10;
    this->buffer = std::make_unique<RingBuffer>(buffer_size);

    this->stream = PaStreamPointer(stream);
    this->format = format;
    this->audio_state.store(State::Stopped);
    return true;
}

void AudioRender::close()
{
    assert(this->audio_state.load() != State::Closed && "AudioRender has not been opened.");
    this->stop();
    this->audio_state.store(State::Closed);
    Pa_AbortStream(this->stream.get());
    Pa_CloseStream(this->stream.get());
    Pa_Terminate();
    this->buffer.reset();
    this->stream.reset();
}

void AudioRender::run()
{
    auto state = this->audio_state.load();
    if (state == State::Stopped)
    {
        Pa_StartStream(this->stream.get());
    }
    this->audio_state.store(State::Running);
}

void AudioRender::stop()
{
    Pa_StopStream(this->stream.get());
    this->audio_state.store(State::Stopped);
}

void AudioRender::pause()
{
    this->audio_state.store(State::Paused);
}

void AudioRender::setVolume(double volume)
{
    this->volume.store(volume);
}

int64_t AudioRender::clock() const
{
    return this->time;
}

AudioRender::State AudioRender::state() const
{
    return this->audio_state.load();
}

PaStreamCallbackResult AudioRender::fill_audio(void *buffer, unsigned long frames)
{
    const auto total_size = frames * this->format.channels * sample_size(this->format.sampleFormat);

    auto state = this->audio_state.load();
    if (state == State::Paused)
    {
        memset(buffer, 0, total_size);
        return paContinue;
    }

    size_t written = 0;

    written += this->buffer->read(static_cast<uint8_t *>(buffer), total_size);

    while (written < total_size)
    {
        auto optional_frame = this->frames->pop();
        if (!optional_frame)
        {
            if (this->frames->eof())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto frame = std::move(optional_frame.value());

        AVDictionaryEntry *dict = av_dict_get(frame->metadata, "speed", nullptr, 0);
        double speed = dict ? std::stod(dict->value) : 1.0;

        int bytes_per_sample = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
        size_t frame_size = frame->nb_samples * frame->ch_layout.nb_channels * bytes_per_sample;

        size_t to_write = std::min(total_size - written, frame_size);
        std::copy(frame->data[0], frame->data[0] + to_write, static_cast<uint8_t *>(buffer) + written);
        written += to_write;

        if (to_write < frame_size)
        {
            this->buffer->write(frame->data[0] + to_write, frame_size - to_write);

            // 更新音频时钟
            int64_t audio_time = frame->pts;
            int64_t offset_time = to_write * 1000 * speed / (frame->ch_layout.nb_channels * bytes_per_sample * frame->sample_rate);
            audio_time += offset_time;
            const PaStreamInfo *info = Pa_GetStreamInfo(this->stream.get());
            const double latency = info ? info->outputLatency : 0.0;
            this->time = audio_time - latency;
        }
    }

    if (written < total_size)
        std::fill_n(static_cast<uint8_t *>(buffer) + written, total_size - written, 0);

    this->apply_volume(buffer, frames);

    if (this->frames->eof() && this->buffer->available() == 0)
        return paComplete;

    return paContinue;
}

int AudioRender::pa_callback(const void *input, void *output, unsigned long frame_count, const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags, void *user_data)
{
    auto render = static_cast<AudioRender*>(user_data);
    if (render->audio_state.load() == State::Closed)
    {
        return paComplete;
    }
    auto result = render->fill_audio(output, frame_count);
    return result;
}

void AudioRender::apply_volume(void *buffer, unsigned long frames)
{
    auto volume = this->volume.load(std::memory_order_relaxed);
    if (volume == 1.0)
        return;

    // 假设采样格式为SInt16
    int16_t* pcm = static_cast<int16_t*>(buffer);
    size_t samples = frames * this->format.channels;

    for (size_t i = 0; i < samples; ++i)
    {
        double v = pcm[i] * volume;
        v = std::max(-32768.0, std::min(32767.0, v));
        pcm[i] = static_cast<int16_t>(v);
    }
}
