#include "Video.h"

Video::Video(QWidget *parent)
    : PaintRender{parent}
{}

bool Video::open(const QString &path)
{
    auto result = this->stream.open(path.toStdString());
    if (!result)
        return false;
    result = this->player.open(this, read_callback, seek_callback);
    if (!result)
        return false;

    const auto default_audio_format = this->player.defaultAudioFormat();
    if (!isFormatSupport(default_audio_format.sampleFormat))
        this->player.setAudioFormat(AudioFormat{.sampleRate = default_audio_format.sampleRate,
                                                .channels = default_audio_format.channels,
                                                .sampleFormat = SampleFormat::SInt16 });
    const auto default_video_format = this->player.defaultVideoFormat();
    if (!isFormatSupport(default_video_format.pixelFormat))
        this->player.setVideoFormat(VideoFormat{.width = default_video_format.width,
                                                .height = default_video_format.height,
                                                .pixelFormat = PixelFormat::RGB24 });

    this->player.setVideoFrameHandler([this](AVFramePointer frame){ this->draw(std::move(frame)); });
    return true;
}

void Video::play()
{
    this->player.run();
}

void Video::pause()
{
    this->player.pause();
}

void Video::stop()
{
    this->player.stop();
}

void Video::close()
{
    this->player.close();
    this->stream.close();
    this->draw(nullptr);
}

void Video::seek(double rate)
{
    emit this->startSeek();
    this->player.seek(rate * this->duration(), [this](int64_t){
        emit this->finishSeek();
    });
}

void Video::setVolume(double volume)
{
    this->player.setVolume(volume);
}

void Video::setSpeed(double speed)
{
    this->player.speedup(speed);
}

QString Video::path() const
{
    return QString::fromStdString(this->stream.filePath());
}

int64_t Video::time() const
{
    return this->player.position();
}

int64_t Video::duration() const
{
    return this->player.duration();
}

int Video::read_callback(void *opaque, uint8_t *buf, int buf_size)
{
    Video* media_data = reinterpret_cast<Video*>(opaque);
    const auto read_size = media_data->stream.read(buf, buf_size);
    if (read_size == 0)
        return AVERROR_EOF;
    else if (read_size < 0)
        return AVERROR(EIO);
    else
        return static_cast<int>(read_size);
}

int64_t Video::seek_callback(void *opaque, int64_t offset, int whence)
{
    Video* media_data = reinterpret_cast<Video*>(opaque);
    auto& media_stream = media_data->stream;
    if (whence == AVSEEK_SIZE)
        return media_stream.fileSize();

    std::ios_base::seekdir dir;
    switch (whence) {
    case SEEK_SET: dir = std::ios::beg; break;
    case SEEK_CUR: dir = std::ios::cur; break;
    case SEEK_END: dir = std::ios::end; break;
    default: return AVERROR(EINVAL);
    }

    if (!media_stream.seek(offset, dir))
        return AVERROR(EIO);

    return media_stream.pos();
}
