#include "Media.h"

#include "ImagePlayer.h"
#include "VideoPlayer.h"

Media::Media(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QHBoxLayout(this);
    this->layout->setContentsMargins(0, 0, 0, 0);
}

bool Media::open(const QString &path, MetaType type)
{
    if (type == MetaType::Image)
    {
        auto image = new ImagePlayer(this);
        if (!image->open(path))
        {
            image->deleteLater();
            return false;
        }
        this->media_widget = image;
    }
    else if (type == MetaType::Video)
    {
        auto video = new VideoPlayer(this);
        if (!video->open(path))
        {
            video->deleteLater();
            return false;
        }
        this->media_widget = video;
    }
    else
    {
        return false;
    }

    this->layout->addWidget(this->media_widget);
    this->media_type = type;
    this->media_path = path;
    this->bind();
    return true;
}

void Media::play()
{
    if (!this->media_widget)
        return;
    if (this->media_type == MetaType::Image)
    {
        auto player = qobject_cast<ImagePlayer*>(this->media_widget);
        player->play();
    }
    else if (this->media_type == MetaType::Video)
    {
        auto player = qobject_cast<VideoPlayer*>(this->media_widget);
        player->play();
    }
}

void Media::close()
{
    this->layout->removeWidget(this->media_widget);
    this->media_widget->deleteLater();
    this->media_widget = nullptr;
    this->media_type = MetaType::Unknown;
}

void Media::setVideoList(const std::vector<QString> &list)
{
    if (this->media_type != MetaType::Video)
        return;
    auto player = qobject_cast<VideoPlayer*>(this->media_widget);
    player->setVideoList(list);
}

QString Media::path() const
{
    return this->media_path;
}

void Media::bind()
{
    if (this->media_type == MetaType::Image)
    {
        auto player = qobject_cast<ImagePlayer*>(this->media_widget);
        connect(player, &ImagePlayer::requestExit, this, [this]{ emit this->requestExit(); });
        connect(player, &ImagePlayer::requestLast, this, [this]{ emit this->requestLast(); });
        connect(player, &ImagePlayer::requestNext, this, [this]{ emit this->requestNext(); });
    }
    else if (this->media_type == MetaType::Video)
    {
        auto player = qobject_cast<VideoPlayer*>(this->media_widget);
        connect(player, &VideoPlayer::requestExit, this, [this]{ emit this->requestExit(); });
        connect(player, &VideoPlayer::requestVideoList, this, [this]{ emit this->requestVideoList(); });
        connect(player, &VideoPlayer::requestFullscreen, this, [this](bool fullscreen){ emit this->requestFullScreen(fullscreen); });
    }
}
