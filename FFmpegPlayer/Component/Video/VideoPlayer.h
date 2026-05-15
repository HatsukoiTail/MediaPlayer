#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QWidget>

#include "VideoController.h"
#include "Video.h"

#include <QWidget>

class VideoPlayer : public QWidget
{
    Q_OBJECT
public:
    explicit VideoPlayer(QWidget *parent = nullptr);

public:
    bool open(const QString& path);
    void play();
    void close();
    void setTitle(const QString& text);
    void setVideoList(const std::vector<QString>& list);

signals:
    void requestExit();
    void requestVideoList();
    void requestFullscreen(bool);

private:
    void bind();
    void on_progress_timer_timeout();
    void on_switch_to_play(const QString& path);
    void on_hover_change(bool hover);
    void on_control_timer_timeout();

private:
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QTimer* controller_timer {nullptr};
    QTimer* progress_timer {nullptr};
    bool mouse_hovering {false};

private:
    Video* video;
    VideoController* controller;
};

#endif // VIDEOPLAYER_H
