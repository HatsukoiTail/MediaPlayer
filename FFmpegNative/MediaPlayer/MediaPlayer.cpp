#include "MediaPlayer.h"

#include <cassert>
#include <format>
#include <stdexcept>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include "FFmpegException.h"

namespace MediaPlayer
{

MediaPlayer::MediaPlayer()
{
    this->audclk.set_queue_serial(&this->queue_serial);
    this->vidclk.set_queue_serial(&this->queue_serial);
    this->extclk.set_queue_serial(&this->queue_serial);
}

MediaPlayer::~MediaPlayer()
{
    this->stop();
}

void MediaPlayer::set_video_renderer(IVideoRenderer *r)
{
    this->video_renderer = r;
}

void MediaPlayer::set_audio_renderer(IAudioRenderer *r)
{
    this->audio_renderer = r;
}

void MediaPlayer::set_hw_device(AVBufferRef *device)
{
    this->hw_device = device;
}

// ===========================================================================

void MediaPlayer::open(const char *file_path)
{
    this->demuxer.open(file_path);

    // 选择最佳流
    auto vstreams = this->demuxer.streams(AVMEDIA_TYPE_VIDEO);
    auto astreams = this->demuxer.streams(AVMEDIA_TYPE_AUDIO);

    if (!vstreams.empty())
        this->video_stream_index = vstreams[0].index;
    if (!astreams.empty())
        this->audio_stream_index = astreams[0].index;

    this->build_pipeline();
    this->logger.open("../log/player.log");
    this->logger.log(std::format("Opened: {} (v={}, a={})", file_path,
                                 this->video_stream_index, this->audio_stream_index));
}

void MediaPlayer::build_pipeline()
{
    // --- 视频管线 ---
    if (this->video_stream_index >= 0)
    {
        auto *vst = this->demuxer.stream(this->video_stream_index);

        this->video_decoder = std::make_unique<Decoder>();
        this->video_decoder->open(vst, this->hw_device);
        this->video_decoder->set_input_queue(
            this->demuxer.stream_queue(this->video_stream_index));

        this->video_filter = std::make_unique<VideoFilter>();
        this->video_filter->open(this->video_decoder->codec_context());
        this->video_filter->set_input_queue(this->video_decoder->output_queue());

        if (this->video_renderer)
        {
            this->video_filter->set_target_format(
                this->video_renderer->preferred_format(),
                this->video_renderer->width(),
                this->video_renderer->height());
        }

        this->video_scheduler.set_source_queue(this->video_filter->output_queue());
        this->video_scheduler.set_renderer(this->video_renderer);
        this->video_scheduler.set_master_clock([this]() { return this->audclk.get(); });
        this->video_scheduler.set_queue_serial(&this->queue_serial);
    }

    // --- 音频管线 ---
    if (this->audio_stream_index >= 0)
    {
        auto *ast = this->demuxer.stream(this->audio_stream_index);

        this->audio_decoder = std::make_unique<Decoder>();
        this->audio_decoder->open(ast, nullptr);
        this->audio_decoder->set_input_queue(
            this->demuxer.stream_queue(this->audio_stream_index));

        this->audio_filter = std::make_unique<AudioFilter>();
        this->audio_filter->open(this->audio_decoder->codec_context());
        this->audio_filter->set_input_queue(this->audio_decoder->output_queue());

        // 默认目标格式：S16, 立体声, 44100Hz
        this->audio_filter->set_target_format(AV_SAMPLE_FMT_S16, 44100,
                                               AV_CHANNEL_LAYOUT_STEREO);

        this->audio_output.set_source_queue(this->audio_filter->output_queue());
        this->audio_output.set_renderer(this->audio_renderer);
        this->audio_output.set_queue_serial(&this->queue_serial);
    }
}

void MediaPlayer::stop_pipeline()
{
    if (this->video_decoder)   this->video_decoder->stop();
    if (this->audio_decoder)   this->audio_decoder->stop();
    if (this->video_filter)    this->video_filter->stop();
    if (this->audio_filter)    this->audio_filter->stop();
    this->video_scheduler.stop();
    this->audio_output.close();
    this->demuxer.stop();
}

void MediaPlayer::flush_pipeline()
{
    if (this->video_decoder)   this->video_decoder->flush();
    if (this->audio_decoder)   this->audio_decoder->flush();
    if (this->video_filter)    this->video_filter->flush();
    if (this->audio_filter)    this->audio_filter->flush();
    this->video_scheduler.flush();
    this->audio_output.flush();
}

// ===========================================================================

void MediaPlayer::play()
{
    if (this->state_ == Playing)
        return;

    this->queue_serial++;

    if (this->state_ == Stopped)
    {
        this->demuxer.start();

        if (this->video_decoder) this->video_decoder->start();
        if (this->audio_decoder) this->audio_decoder->start();
        if (this->video_filter)  this->video_filter->start();
        if (this->audio_filter)  this->audio_filter->start();

        // 音频输出：注册回调，开始拉数据
        if (this->audio_output.is_opened() == false && this->audio_renderer)
            this->audio_output.open(44100, AV_SAMPLE_FMT_S16,
                                     AV_CHANNEL_LAYOUT_STEREO);

        if (this->video_renderer)
            this->video_scheduler.start();
    }
    else // Paused → resume
    {
        this->audio_output.resume();
        if (this->video_renderer)
            this->video_scheduler.start();
    }

    this->state_ = Playing;
    this->logger.log("Playing");
}

void MediaPlayer::pause()
{
    if (this->state_ != Playing)
        return;

    this->audio_output.pause();
    this->video_scheduler.stop();
    this->state_ = Paused;
    this->logger.log("Paused");
}

void MediaPlayer::stop()
{
    this->stop_pipeline();
    this->demuxer.close();
    this->state_ = Stopped;
    this->logger.log("Stopped");
}

void MediaPlayer::seek(double seconds)
{
    bool was_playing = (this->state_ == Playing);
    if (was_playing)
        this->stop_pipeline();

    this->demuxer.seek(seconds);
    this->queue_serial++;
    this->flush_pipeline();

    if (was_playing)
        this->play();
}

void MediaPlayer::set_speed(double s)
{
    if (s <= 0.0) s = 1.0;
    this->speed = s;

    this->audclk.set_speed(s);
    this->vidclk.set_speed(s);

    if (this->audio_filter)
        this->audio_filter->set_speed(s);
}

void MediaPlayer::set_volume(double vol)
{
    this->audio_output.set_volume(vol);
}

MediaPlayer::State MediaPlayer::state() const
{
    return this->state_;
}

double MediaPlayer::duration() const
{
    return this->demuxer.duration();
}

double MediaPlayer::position() const
{
    double c = this->audclk.get();
    if (!std::isnan(c)) return c;
    c = this->vidclk.get();
    if (!std::isnan(c)) return c;
    return 0.0;
}

std::vector<StreamInfo> MediaPlayer::video_streams() const
{
    return this->demuxer.streams(AVMEDIA_TYPE_VIDEO);
}

std::vector<StreamInfo> MediaPlayer::audio_streams() const
{
    return this->demuxer.streams(AVMEDIA_TYPE_AUDIO);
}

int MediaPlayer::current_video_stream() const
{
    return this->video_stream_index;
}

int MediaPlayer::current_audio_stream() const
{
    return this->audio_stream_index;
}

void MediaPlayer::select_stream(int index)
{
    auto info = this->demuxer.streams();
    if (index < 0 || index >= (int)info.size())
        throw std::out_of_range("Stream index out of range");

    bool was_playing = (this->state_ == Playing);
    if (was_playing)
        this->stop_pipeline();

    AVMediaType type = info[index].type;
    if (type == AVMEDIA_TYPE_VIDEO)
    {
        this->video_stream_index = index;
        this->video_decoder.reset();
        this->video_filter.reset();
    }
    else if (type == AVMEDIA_TYPE_AUDIO)
    {
        this->audio_stream_index = index;
        this->audio_decoder.reset();
        this->audio_filter.reset();
    }

    this->build_pipeline();

    if (was_playing)
        this->play();
}

} // namespace MediaPlayer
