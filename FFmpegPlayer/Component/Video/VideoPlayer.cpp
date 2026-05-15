#include "VideoPlayer.h"

#include "Tool.h"

#include <QEvent>
#include <QPainter>
#include <QTimer>

VideoPlayer::VideoPlayer(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QGridLayout(this);
    this->video = new Video(this);
    this->controller = new VideoController(this);

    layout->addWidget(this->video, 0, 0);
    layout->addWidget(this->controller, 0, 0);
    layout->setContentsMargins(0, 0, 0, 0);

    this->controller_timer = new QTimer(this);
    this->controller_timer->setInterval(1800);
    this->progress_timer = new QTimer(this);
    this->progress_timer->setInterval(100);
    this->bind();
}

bool VideoPlayer::open(const QString &path)
{
    if (!this->video->open(path))
        return false;
    const auto file_name = formatFileName(path);
    this->controller->setTitle(file_name);
    this->controller->setPlayState(true);
    this->progress_timer->start();
    this->controller_timer->start();
    return true;
}

void VideoPlayer::play()
{
    this->video->play();
}

void VideoPlayer::close()
{
    this->video->close();
    this->progress_timer->stop();
    this->controller_timer->stop();
    this->controller->setPlayState(false);
}

void VideoPlayer::setTitle(const QString &text)
{
    this->controller->setTitle(text);
}

void VideoPlayer::setVideoList(const std::vector<QString> &list)
{
    this->controller->setVideoList(list);
}

void VideoPlayer::bind()
{
    setMouseTracking(true);
    this->video->setMouseTracking(true);

    connect(this->progress_timer, &QTimer::timeout, this, &VideoPlayer::on_progress_timer_timeout);
    connect(this->controller, &VideoController::exit, this, [this]{ emit this->requestExit(); });
    connect(this->controller, &VideoController::play, this->video, &Video::play);
    connect(this->controller, &VideoController::pause, this->video, &Video::pause);
    connect(this->controller, &VideoController::requestVideoList, this, [this](){ emit this->requestVideoList(); });
    connect(this->controller, &VideoController::switchToPlay, this, &VideoPlayer::on_switch_to_play);
    connect(this->controller, &VideoController::volumeChange, this, [this](int value){ this->video->setVolume(value / 100.0); });
    connect(this->controller, &VideoController::speedChange, this->video, &Video::setSpeed);
    connect(this->controller, &VideoController::requestFullScreen, this, [this](bool fullscreen){ emit this->requestFullscreen(fullscreen); });

    connect(this->controller, &VideoController::readySeek, this->progress_timer, &QTimer::stop);
    connect(this->controller, &VideoController::seek, this->video, &Video::seek);
    connect(this->video, &Video::finishSeek, this, [this]{ this->progress_timer->start(); });

    connect(this->controller, &VideoController::mouseHover, this, &VideoPlayer::on_hover_change);
    connect(this->controller_timer, &QTimer::timeout, this, &VideoPlayer::on_control_timer_timeout);
}

void VideoPlayer::on_progress_timer_timeout()
{
    auto cur = this->video->time();
    auto total = this->video->duration();
    this->controller->setTime(cur, total);
}

void VideoPlayer::on_switch_to_play(const QString &path)
{
    this->close();
    if (this->open(path))
        this->video->play();
}

void VideoPlayer::on_hover_change(bool hover)
{
    this->mouse_hovering = hover;
    if (hover)
        this->controller_timer->start();
}

void VideoPlayer::on_control_timer_timeout()
{
    if (!this->mouse_hovering)
    {
        this->controller->hide();
        this->setCursor(Qt::BlankCursor);
    }
}

void VideoPlayer::mouseMoveEvent(QMouseEvent *event)
{
    this->controller->show();
    this->controller_timer->start();
    this->unsetCursor();
    QWidget::mouseMoveEvent(event);
}
