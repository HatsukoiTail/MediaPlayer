#include "VideoControlPanel.h"

VideoControlPanel::VideoControlPanel(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QVBoxLayout(this);
    this->progress_bar = new ProgressBar(this);
    this->control_bar = new VideoControlBar(this);

    layout->addWidget(this->progress_bar);
    layout->addWidget(this->control_bar);

    this->bind();
}

void VideoControlPanel::setTime(int64_t current, int64_t total)
{
    this->progress_bar->setProgress(static_cast<double>(current) / total);
    this->control_bar->setTime(current, total);
}

void VideoControlPanel::setPlayState(bool playing)
{
    this->control_bar->setPlayState(playing);
}

void VideoControlPanel::setVideoList(const std::vector<QString> &list)
{
    this->control_bar->setVideoList(list);
}

void VideoControlPanel::bind()
{
    connect(this->control_bar, &VideoControlBar::requestPlay, this, [this](){ emit this->requestPlay(); });
    connect(this->control_bar, &VideoControlBar::requestPause, this, [this](){ emit this->requestPause(); });
    connect(this->control_bar, &VideoControlBar::requestVideoList, this, [this](){ emit this->requestVideoList(); });
    connect(this->control_bar, &VideoControlBar::switchToPlay, this, [this](const QString& path){ emit this->switchToPlay(path); });
    connect(this->control_bar, &VideoControlBar::volumeChange, this, [this](int value){ emit this->volumeChange(value); });
    connect(this->control_bar, &VideoControlBar::speedChange, this, [this](double value){ emit this->speedChange(value); });
    connect(this->control_bar, &VideoControlBar::requestFullScreen, this, [this](bool is){ emit this->requestFullScreen(is); });
    connect(this->control_bar, &VideoControlBar::mouseHover, this, [this](bool hover){ emit this->mouseHover(hover); });
    connect(this->progress_bar, &ProgressBar::readySeek, this, [this](){ emit this->readySeek(); });
    connect(this->progress_bar, &ProgressBar::seek, this, [this](double rate){ emit this->seek(rate); });
    connect(this->progress_bar, &ProgressBar::mouseHover, this, [this](bool hover){ emit this->mouseHover(hover); });
}
