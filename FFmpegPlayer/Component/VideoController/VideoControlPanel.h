#ifndef VIDEOCONTROLPANEL_H
#define VIDEOCONTROLPANEL_H

#include "VideoControlBar.h"
#include "ProgressBar.h"

#include <QVBoxLayout>
#include <QWidget>

class VideoControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VideoControlPanel(QWidget *parent = nullptr);
    void setTime(int64_t current, int64_t total);
    void setPlayState(bool playing);
    void setVideoList(const std::vector<QString>& list);

signals:
    void readySeek();
    void seek(double);
    void requestPlay();
    void requestPause();
    void requestVideoList();
    void switchToPlay(const QString&);
    void volumeChange(int);
    void speedChange(double);
    void requestFullScreen(bool);
    void mouseHover(bool);

private:
    void bind();

private:
    ProgressBar* progress_bar;
    VideoControlBar* control_bar;
};

#endif // VIDEOCONTROLPANEL_H
