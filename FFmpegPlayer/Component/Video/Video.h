#ifndef VIDEO_H
#define VIDEO_H

#include "DataStream.h"
#include "FFmpegPlayer.h"
#include "PaintRender.h"

#include <QHBoxLayout>
#include <QWidget>

class Video : public PaintRender
{
    Q_OBJECT
public:
    explicit Video(QWidget *parent = nullptr);
public:
    bool open(const QString& path);
    void play();
    void pause();
    void stop();
    void close();
    void seek(double rate);
    void setVolume(double volume);
    void setSpeed(double speed);

public:
    QString path() const;
    int64_t time() const;
    int64_t duration() const;

private:
    static int read_callback(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek_callback(void *opaque, int64_t offset, int whence);

signals:
    void startSeek();
    void finishSeek();

private:
    DataStream stream;
    FFmpegPlayer player;
};

#endif // VIDEO_H
