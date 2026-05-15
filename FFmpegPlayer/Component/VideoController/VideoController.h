#ifndef VIDEOCONTROLLER_H
#define VIDEOCONTROLLER_H

#include "ControlTitleBar.h"
#include "VideoControlPanel.h"

#include <QVBoxLayout>
#include <QWidget>

class VideoController : public QWidget
{
    Q_OBJECT
public:
    explicit VideoController(QWidget *parent = nullptr);
    void setTitle(const QString& title);
    void setTime(int64_t time, int64_t total);
    void setPlayState(bool playing);
    void setVideoList(const std::vector<QString>& list);

signals:
    void exit();
    void readySeek();
    void seek(double);
    void play();
    void pause();
    void requestVideoList();
    void switchToPlay(const QString&);
    void volumeChange(int);
    void speedChange(double);
    void requestFullScreen(bool);
    void mouseHover(bool);

private:
    void bind();
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    bool hovering {false};

private:
    ControlTitleBar* title;
    VideoControlPanel* control_panel;
};

#endif // VIDEOCONTROLLER_H
