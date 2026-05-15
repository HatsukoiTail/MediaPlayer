#include "MediaInfo.h"

#include <cassert>
#include <filesystem>

#include "CodecContext.h"
#include "Frame.h"
#include "Helper.h"
#include "Packet.h"

MediaInfo::MediaInfo(AVFormatContext *ctx, bool take_ownership)
    : format_ctx(ctx), format_ctx_owned(take_ownership), video_stream(nullptr), audio_stream(nullptr)
{
}

MediaInfo::MediaInfo(MediaInfo &&other) noexcept
    : format_ctx(other.format_ctx), format_ctx_owned(other.format_ctx_owned), video_stream(other.video_stream), audio_stream(other.audio_stream)
{
    other.format_ctx = nullptr;
    other.video_stream = nullptr;
    other.audio_stream = nullptr;
    other.format_ctx_owned = true;
}

MediaInfo &MediaInfo::operator=(MediaInfo &&other) noexcept
{
    assert(this != &other);
    close();
    format_ctx = other.format_ctx;
    video_stream = other.video_stream;
    audio_stream = other.audio_stream;
    format_ctx_owned = other.format_ctx_owned;
    other.format_ctx = nullptr;
    other.video_stream = nullptr;
    other.audio_stream = nullptr;
    other.format_ctx_owned = true;
    return *this;
}

MediaInfo::~MediaInfo()
{
    close();
}

void MediaInfo::open(std::string_view filename)
{
    auto format_ctx = open_format_context(filename);
    this->format_ctx = format_ctx.release();
    this->format_ctx_owned = true;

    int video_stream_index = av_find_best_stream(this->format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index >= 0)
        this->video_stream = this->format_ctx->streams[video_stream_index];
    else
        this->video_stream = nullptr;

    int audio_stream_index = av_find_best_stream(this->format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index >= 0)
        this->audio_stream = this->format_ctx->streams[audio_stream_index];
    else
        this->audio_stream = nullptr;
}

void MediaInfo::close()
{
    if (this->format_ctx == nullptr || this->format_ctx_owned == false)
        return;
    
    auto avio_ctx = (this->format_ctx->flags & AVFMT_FLAG_CUSTOM_IO) ? this->format_ctx->pb : nullptr;
    avformat_close_input(&this->format_ctx);
    if (avio_ctx != nullptr)
        avio_context_free(&avio_ctx);
}

bool MediaInfo::is_opened() const
{
    return this->format_ctx != nullptr && this->format_ctx->iformat != nullptr;
}

std::string MediaInfo::file_path() const
{
    assert(this->is_opened());
    if (this->format_ctx->url == nullptr)
        return std::string();
    return std::string(this->format_ctx->url);
}

std::string MediaInfo::file_name() const
{
    assert(this->is_opened());
    if (this->format_ctx->url == nullptr)
        return std::string();
    return std::filesystem::path(this->format_ctx->url).filename().string();
}

double MediaInfo::duration() const
{
    assert(this->is_opened());
    return static_cast<double>(this->format_ctx->duration) / AV_TIME_BASE;
}

int64_t MediaInfo::size() const
{
    assert(this->is_opened());

    auto av_size = avio_size(format_ctx->pb);
    if (av_size >= 0)
        return av_size;

    if (this->format_ctx->url == nullptr)
        return 0;

    return std::filesystem::file_size(this->format_ctx->url);
}

int64_t MediaInfo::bit_rate() const
{
    assert(this->is_opened());
    return this->format_ctx->bit_rate;
}

std::string MediaInfo::format_name() const
{
    assert(this->is_opened());
    return std::string(this->format_ctx->iformat->name);
}

int MediaInfo::stream_count() const
{
    assert(this->is_opened());
    return this->format_ctx->nb_streams;
}

AVStream* MediaInfo::stream(int index) const
{
    assert(this->is_opened());
    assert(index >= 0 && index < this->format_ctx->nb_streams);
    return this->format_ctx->streams[index];
}

double MediaInfo::start_time() const
{
    assert(this->is_opened());
    return static_cast<double>(this->format_ctx->start_time) / AV_TIME_BASE;
}

AVFramePtr MediaInfo::media_cover() const
{
    assert(this->is_opened());
    if ((this->video_stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != true)
    {
        return nullptr;
    }
    AVPacket *packet = &this->video_stream->attached_pic;

    // 打开解码器
    auto codec_ctx = open_codec_context(this->video_stream);

    if (avcodec_send_packet(codec_ctx.get(), packet) < 0)
    {
        return nullptr;
    }

    AVFramePtr frame(av_frame_alloc());
    if (frame == nullptr)
        return nullptr;

    if (avcodec_receive_frame(codec_ctx.get(), frame.get()) < 0)
    {
        return nullptr;
    }

    return frame;
}

std::map<std::string, std::string> MediaInfo::metadata() const
{
    assert(this->is_opened());
    auto metadata = this->format_ctx->metadata;
    if (metadata == nullptr)
    {
        return {};
    }
    auto result = std::map<std::string, std::string>();
    AVDictionaryEntry *tag = nullptr;
    while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)) != nullptr)
    {
        if (tag->key == nullptr)
        {
            continue;
        }
        result.emplace(tag->key, tag->value);
    }
    return result;
}

std::string MediaInfo::metadata(std::string_view key) const
{
    assert(this->is_opened());
    auto metadata = this->format_ctx->metadata;
    if (metadata == nullptr)
    {
        return {};
    }
    auto tag = av_dict_get(metadata, key.data(), nullptr, 0);
    return tag == nullptr ? std::string() : std::string(tag->value);
}

std::vector<MediaChapter> MediaInfo::chapters() const
{
    assert(this->is_opened());
    std::vector<MediaChapter> result;
    for (int i = 0; i < this->format_ctx->nb_chapters; i++)
    {
        auto chapter = this->format_ctx->chapters[i];
        MediaChapter media_chapter;
        media_chapter.id = chapter->id;
        media_chapter.start_time = chapter->start * av_q2d(chapter->time_base);
        media_chapter.end_time = chapter->end * av_q2d(chapter->time_base);
        media_chapter.title = search_metadata(chapter->metadata, "title");
        result.push_back(media_chapter);
    }
    return result;
}

std::string MediaInfo::video_codec_name() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return std::string();

    return avcodec_get_name(this->video_stream->codecpar->codec_id);
}

AVPixelFormat MediaInfo::pixel_format() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return AV_PIX_FMT_NONE;

    return static_cast<AVPixelFormat>(this->video_stream->codecpar->format);
}

int MediaInfo::width() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return 0;

    return this->video_stream->codecpar->width;
}

int MediaInfo::height() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return 0;

    return this->video_stream->codecpar->height;
}

double MediaInfo::frame_rate() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return 0.0;

    return this->video_stream->avg_frame_rate.num / (double)this->video_stream->avg_frame_rate.den;
}

double MediaInfo::video_start_time() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return std::numeric_limits<double>::quiet_NaN();

    return this->video_stream->start_time * av_q2d(this->video_stream->time_base);
}

double MediaInfo::video_duration() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return std::numeric_limits<double>::quiet_NaN();

    return this->video_stream->duration * av_q2d(this->video_stream->time_base);
}

int64_t MediaInfo::video_frame_count() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return 0;

    return this->video_stream->nb_frames;
}

int64_t MediaInfo::video_bit_rate() const
{
    assert(this->is_opened());

    if (this->video_stream == nullptr)
        return 0;

    return this->video_stream->codecpar->bit_rate;
}

std::string MediaInfo::audio_codec_name() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return std::string();

    return avcodec_get_name(this->audio_stream->codecpar->codec_id);
}

AVSampleFormat MediaInfo::sample_format() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return AV_SAMPLE_FMT_NONE;

    return (AVSampleFormat)this->audio_stream->codecpar->format;
}

std::string MediaInfo::channel_layout() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return std::string();

    std::string layout_name(64, '\0');
    int result = av_channel_layout_describe(&this->audio_stream->codecpar->ch_layout, layout_name.data(), layout_name.size());
    if (result < 0)
        return std::string();

    return layout_name.substr(0, result);
}

int MediaInfo::channel_count() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return 0;

    return this->audio_stream->codecpar->ch_layout.nb_channels;
}

int MediaInfo::sample_rate() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return 0;

    return this->audio_stream->codecpar->sample_rate;
}

double MediaInfo::audio_start_time() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return std::numeric_limits<double>::quiet_NaN();

    return this->audio_stream->start_time * av_q2d(this->audio_stream->time_base);
}

int64_t MediaInfo::audio_frame_count() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return 0;

    return this->audio_stream->nb_frames;
}

int64_t MediaInfo::audio_bit_rate() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return 0;

    return this->audio_stream->codecpar->bit_rate;
}

int MediaInfo::btyes_per_sample() const
{
    assert(this->is_opened());

    if (this->audio_stream == nullptr)
        return 0;

    return av_get_bytes_per_sample((AVSampleFormat)this->audio_stream->codecpar->format);
}
