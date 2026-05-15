#include "VideoController.h"

#include <QDebug>
#include <QMouseEvent>

VideoController::VideoController(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QVBoxLayout(this);
    this->title = new ControlTitleBar(this);
    this->control_panel = new VideoControlPanel(this);
    layout->addWidget(this->title);
    layout->addStretch();
    layout->addWidget(this->control_panel);

    // setMouseTracking(true);
    // this->title->setMouseTracking(true);
    // this->control_panel->setMouseTracking(true);
    this->bind();
}

void VideoController::setTitle(const QString &title)
{
    this->title->setText(title);
}

void VideoController::setTime(int64_t time, int64_t total)
{
    this->control_panel->setTime(time, total);
}

void VideoController::setPlayState(bool playing)
{
    this->control_panel->setPlayState(playing);
}

void VideoController::setVideoList(const std::vector<QString> &list)
{
    this->control_panel->setVideoList(list);
}

void VideoController::bind()
{
    connect(this->title, &ControlTitleBar::requestExit, this, [this](){ emit this->exit(); });

    connect(this->control_panel, &VideoControlPanel::readySeek, this, [this](){ emit this->readySeek(); });
    connect(this->control_panel, &VideoControlPanel::seek, this, [this](double rate){ emit this->seek(rate); });

    connect(this->control_panel, &VideoControlPanel::requestPlay, this, [this](){ emit this->play(); });
    connect(this->control_panel, &VideoControlPanel::requestPause, this, [this](){ emit this->pause(); });

    connect(this->control_panel, &VideoControlPanel::requestVideoList, this, [this](){ emit this->requestVideoList(); });
    connect(this->control_panel, &VideoControlPanel::switchToPlay, this, [this](const QString& path){ emit this->switchToPlay(path); });

    connect(this->control_panel, &VideoControlPanel::volumeChange, this, [this](int value){ emit this->volumeChange(value); });
    connect(this->control_panel, &VideoControlPanel::speedChange, this, [this](double value){ emit this->speedChange(value); });
    connect(this->control_panel, &VideoControlPanel::requestFullScreen, this, [this](bool is){ emit this->requestFullScreen(is); });

    connect(this->title, &ControlTitleBar::mouseHover, this, [this](bool hover){ emit this->mouseHover(hover); });
    connect(this->control_panel, &VideoControlPanel::mouseHover, this, [this](bool hover){ emit this->mouseHover(hover); });
}

void VideoController::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->pos();
    auto title_rect = QRect(this->title->mapTo(this, QPoint(0, 0)), this->title->size());
    auto panel_rect = QRect(this->control_panel->mapTo(this, QPoint(0, 0)), this->control_panel->size());
    bool inside = title_rect.contains(pos) || panel_rect.contains(pos);

    if (inside != this->hovering)
    {
        this->hovering = inside;
        if (inside)
        {
            // emit this->mouseEnter();
        }
        else
        {
            // emit this->mouseLeave();
        }
    }
    QWidget::mouseMoveEvent(event);
}
