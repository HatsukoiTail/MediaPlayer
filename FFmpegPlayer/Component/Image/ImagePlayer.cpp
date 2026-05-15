#include "ImagePlayer.h"

#include "Tool.h"

#include <QTimer>

ImagePlayer::ImagePlayer(QWidget *parent)
    : QWidget{parent}
{
    auto layout = new QHBoxLayout(this);
    this->image = new Image(this);
    this->controller = new ImageController(this->image);
    QHBoxLayout* controller_layout = new QHBoxLayout(this->image);
    layout->addWidget(this->image);
    controller_layout->addWidget(this->controller);

    layout->setContentsMargins(0, 0, 0, 0);
    controller_layout->setContentsMargins(20, 0, 20, 0);

    this->controller_timer = new QTimer(this);

    this->bind();

    setMouseTracking(true);
    this->image->setMouseTracking(true);
    this->controller_timer->setInterval(1500);
}

ImagePlayer::~ImagePlayer()
{
    ImagePlayer::close();
}

bool ImagePlayer::open(const QString &path)
{
    this->controller_timer->start();
    this->controller->setText(formatFileName(path));
    return this->image->open(path);
}

void ImagePlayer::close()
{
    this->controller_timer->stop();
    this->controller->setText("");
}

void ImagePlayer::play()
{
    this->image->play();
}

void ImagePlayer::bind()
{
    connect(this->controller, &ImageController::mouseHover, this, &ImagePlayer::on_hover_change);
    connect(this->controller_timer, &QTimer::timeout, this, &ImagePlayer::on_timer_timeout);

    connect(this->controller, &ImageController::requestExit, this, [this]{ emit this->requestExit(); });
    connect(this->controller, &ImageController::requestLast, this, [this]{ emit this->requestLast(); });
    connect(this->controller, &ImageController::requestNext, this, [this]{ emit this->requestNext(); });
}

void ImagePlayer::on_hover_change(bool hover)
{
    this->mouse_hovering = hover;
    if (hover)
        this->controller_timer->start();
}

void ImagePlayer::on_timer_timeout()
{
    if (!this->mouse_hovering)
    {
        this->controller->hide();
        this->controller_timer->stop();
    }
}

void ImagePlayer::mouseMoveEvent(QMouseEvent *event)
{
    this->controller_timer->start();
    this->controller->show();
    QWidget::mouseMoveEvent(event);
}
