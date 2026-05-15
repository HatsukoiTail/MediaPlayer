#ifndef MEDIAINFO_H
#define MEDIAINFO_H

#include "DataStream.h"
#include "Model.h"
#include "SmartStruct.h"

#include <QImage>
#include <QString>

class MediaInfo
{
public:
    explicit MediaInfo(const QString& path);
    ~MediaInfo();

public:
    static MediaData mediaData(const QString& path);

public:
    bool open(const QString& path);
    void close();
    bool isOpen() const;
    QImage thumbnail();
    QImage image();
    MediaData mediaData();

private:
    enum class DecodeResult;
    AVPacketPointer read_packet();
    std::pair<DecodeResult, AVFramePointer> read_frame(AVPacketPointer packet);
    AVFramePointer sws_frame(AVFramePointer frame);

private:
    static int read_callback(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek_callback(void *opaque, int64_t offset, int whence);

private:
    bool is_opened = false;
    MetaType type {MetaType::Unknown};

private:
    AVFormatContextPointer format_ctx;
    AVCodecContextPointer codec_ctx;
    SwsContextPointer sws_ctx;
    DataStream data_stream;
    int video_index;
};

#endif // MEDIAINFO_H
