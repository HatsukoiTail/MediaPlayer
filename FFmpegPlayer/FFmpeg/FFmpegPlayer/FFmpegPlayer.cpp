#include "FFmpegPlayer.h"

#include "Print.h"

FFmpegPlayer::FFmpegPlayer(std::string_view path)
{
    this->open(path);
}

bool FFmpegPlayer::open(std::string_view path)
{
    if (this->status.load() != State::Closed)
    {
        print(Ansi::Red, "Must be closed before opening.");
        return false;
    }
    this->demuxer = std::make_unique<Demuxer>(path);
    if (this->demuxer->state() == Demuxer::State::Closed)
    {
        print(Ansi::Red, "Fail to open Demuxer.");
        return false;
    }
    if (this->demuxer->has_stream(Demuxer::StreamType::Audio))
    {
        this->audio_decoder = std::make_unique<AudioDecoder>(this->demuxer->packets(Demuxer::StreamType::Audio));
        if(!this->audio_decoder->open(this->demuxer->stream(Demuxer::StreamType::Audio)))
        {
            print(Ansi::Red, "Fail to open AudioDecoder.");
        }
        this->audio_render = std::make_unique<AudioRender>(this->audio_decoder->data());
    }
    if (this->demuxer->has_stream(Demuxer::StreamType::Video))
    {
        this->video_decoder = std::make_unique<VideoDecoder>(this->demuxer->packets(Demuxer::StreamType::Video));
        if(!this->video_decoder->open(this->demuxer->stream(Demuxer::StreamType::Video)))
        {
            print(Ansi::Red, "Fail to open VideoDecoder.");
        }
        this->video_render = std::make_unique<VideoRender>(this->video_decoder->data());
        if (this->audio_render)
            this->video_render->set_clock([this](){ return this->audio_render->clock(); });
    }
    this->status.store(State::Stopped);
    return true;
}

bool FFmpegPlayer::open(void *udata, ReadCallback read_fn, SeekCallback seek_fn)
{
    if (this->status.load() != State::Closed)
    {
        print(Ansi::Red, "Must be closed before opening.");
        return false;
    }
    this->demuxer = std::make_unique<Demuxer>(udata, read_fn, seek_fn);
    if (this->demuxer->state() == Demuxer::State::Closed)
    {
        print(Ansi::Red, "Fail to open Demuxer.");
        return false;
    }
    if (this->demuxer->has_stream(Demuxer::StreamType::Audio))
    {
        this->audio_decoder = std::make_unique<AudioDecoder>(this->demuxer->packets(Demuxer::StreamType::Audio));
        if(!this->audio_decoder->open(this->demuxer->stream(Demuxer::StreamType::Audio)))
        {
            print(Ansi::Red, "Fail to open AudioDecoder.");
        }
        this->audio_render = std::make_unique<AudioRender>(this->audio_decoder->data());
    }
    if (this->demuxer->has_stream(Demuxer::StreamType::Video))
    {
        this->video_decoder = std::make_unique<VideoDecoder>(this->demuxer->packets(Demuxer::StreamType::Video));
        if(!this->video_decoder->open(this->demuxer->stream(Demuxer::StreamType::Video)))
        {
            print(Ansi::Red, "Fail to open VideoDecoder.");
        }
        this->video_render = std::make_unique<VideoRender>(this->video_decoder->data());
        if (this->audio_render)
            this->video_render->set_clock([this](){ return this->audio_render->clock(); });
    }
    this->status.store(State::Stopped);
    return true;
}

void FFmpegPlayer::run()
{
    assert(this->status.load() != State::Closed);
    if (this->status.load() == State::Running)
        return;
    this->demuxer->run();
    if (this->audio_decoder)
        this->audio_decoder->run();
    if (this->video_decoder)
        this->video_decoder->run();
    if (this->audio_render)
        this->audio_render->run();
    if (this->video_render)
        this->video_render->run();
    this->status.store(State::Running);
}

void FFmpegPlayer::pause()
{
    assert(this->status.load() != State::Closed);
    if (this->status.load() == State::Running)
    {
        this->demuxer->pause();
        if (this->audio_decoder)
            this->audio_decoder->pause();
        if (this->video_decoder)
            this->video_decoder->pause();
        if (this->audio_render)
            this->audio_render->pause();
        if (this->video_render)
            this->video_render->pause();
        this->status.store(State::Paused);
    }
}

void FFmpegPlayer::stop()
{
    assert(this->status.load() != State::Closed);
    if (this->status.load() != State::Stopped)
    {
        this->demuxer->stop();
        if (this->audio_decoder)
            this->audio_decoder->stop();
        if (this->video_decoder)
            this->video_decoder->stop();
        if (this->audio_render)
            this->audio_render->stop();
        if (this->video_render)
            this->video_render->stop();
        this->status.store(State::Stopped);
    }
}

void FFmpegPlayer::close()
{
    assert(this->status.load() != State::Closed);
    this->demuxer.reset();
    if (this->audio_decoder)
        this->audio_decoder.reset();
    if (this->video_decoder)
        this->video_decoder.reset();
    if (this->video_render)
        this->video_render.reset();
    if (this->audio_render)
        this->audio_render.reset();
    this->status.store(State::Closed);
}

void FFmpegPlayer::seek(const int64_t timestamp, std::function<void (int64_t)> callback)
{
    assert(this->status.load() != State::Closed);
    if (this->status.load() == State::Seeking)
        return;
    auto state = this->status.load();
    this->status.store(State::Seeking);
    this->demuxer->seek(timestamp);
    if (this->audio_decoder)
        this->audio_decoder->seek(timestamp, [this, callback, state](int64_t audio_time){
            if (this->video_decoder->state() != VideoDecoder::State::Seeking)
            {
                if (callback)
                    callback(audio_time);
                this->status.store(state);
            }
        });
    if (this->video_decoder)
        this->video_decoder->seek(timestamp, [this, callback, state](int64_t video_time){
            print(Ansi::Green, "VideoDecoder seek success, {}", video_time);
            if (this->video_decoder->state() != VideoDecoder::State::Seeking)
            {
                if (callback)
                    callback(video_time);
                this->status.store(state);
            }
        });
}

void FFmpegPlayer::speedup(double speed)
{
    speed = std::min(10.0, std::max(0.1, speed));
    if (this->audio_decoder)
        this->audio_decoder->set_speed(speed);
    if (this->video_render)
        this->video_render->set_speed(speed);
    this->play_speed = speed;
}

void FFmpegPlayer::setVolume(double volume)
{
    if (this->audio_render)
        this->audio_render->setVolume(volume);
}

void FFmpegPlayer::setVideoFrameHandler(std::function<void (AVFramePointer)> callback)
{
    if (this->video_render)
        this->video_render->set_callback(std::move(callback));
}

void FFmpegPlayer::setVideoFormat(const VideoFormat &video_format)
{
    if (!this->video_decoder || this->video_decoder->state() != VideoDecoder::State::Stopped)
    {
        print(Ansi::Red, "Cannot set video format because of VideoDecoder's state.");
        return;
    }
    this->video_decoder->set_format(video_format);
}

void FFmpegPlayer::setAudioFormat(const AudioFormat &audio_format)
{
    if (!this->audio_decoder || this->audio_decoder->state() != Decoder::State::Stopped)
    {
        print(Ansi::Red, "Cannot set audio format because of AudioDecoder's state.");
        return;
    }
    if (!this->audio_render || this->audio_render->state() != AudioRender::State::Closed)
    {
        print(Ansi::Red, "Cannot set audio format because of AudioRender's state.");
        return;
    }
    this->audio_decoder->set_format(audio_format);
    if (!this->audio_render->open(audio_format))
    {
        print(Ansi::Red, "Fail to open AudioRender.");
        return;
    }
}

VideoFormat FFmpegPlayer::defaultVideoFormat() const
{
    if (this->video_decoder)
        return this->video_decoder->default_format();
    return {};
}

AudioFormat FFmpegPlayer::defaultAudioFormat() const
{
    if (this->audio_decoder)
        return this->audio_decoder->default_format();
    return {};
}

int64_t FFmpegPlayer::duration() const
{
    if (this->demuxer)
        return this->demuxer->duration() / 1000;
    return -1;
}

int64_t FFmpegPlayer::position() const
{
    if (this->audio_render)
        return this->audio_render->clock();
    else
        return this->video_render->clock();
}

double FFmpegPlayer::speed() const
{
    return this->play_speed;
}

FFmpegPlayer::State FFmpegPlayer::state() const
{
    return this->status.load();
}
