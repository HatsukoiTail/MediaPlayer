#include "Widget.h"

#include "MediaPreviewWidget.h"

#include <QPainter>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    this->layout = new QHBoxLayout(this);
    this->splitter = new QSplitter(this);
    this->folder_list = new MediaListWidget(this);
    this->page_manager = new PageManager(this);
    this->media = new Media(this);

    this->layout->addWidget(this->splitter);
    this->splitter->addWidget(this->folder_list);
    this->splitter->addWidget(this->page_manager);

    this->layout->setContentsMargins(0, 0, 0, 0);
    this->splitter->setHandleWidth(2);

    this->splitter->setSizes({ 180, 900 });
    this->media->hide();

    this->bind();
}

void Widget::bind()
{
    connect(this->folder_list, &MediaListWidget::selected, this, &Widget::on_folder_selected);
    connect(this->folder_list, &MediaListWidget::toRemove, this, &Widget::on_folder_remove);

    connect(this->media, &Media::requestExit, this, &Widget::on_media_exit);
    connect(this->media, &Media::requestFullScreen, this, &Widget::set_player_show_mode);
    connect(this->media, &Media::requestVideoList, this, &Widget::set_video_list);
    connect(this->media, &Media::requestLast, this, &Widget::play_last_image);
    connect(this->media, &Media::requestNext, this, &Widget::play_next_image);
}

QWidget *Widget::make_preview_page(const QString &path)
{
    auto page = new MediaPreviewWidget();
    connect(page, &MediaPreviewWidget::requestPlay, this, &Widget::on_play_media);
    connect(page, &MediaPreviewWidget::mediaMoved, this, &Widget::on_media_moved);
    connect(this, &Widget::newMediaCreated, page, &MediaPreviewWidget::addItem);
    page->open(path);
    return page;
}

void Widget::set_video_list()
{
    auto page = this->page_manager->widget(this->page_manager->currentPageId());
    if (!page)
        return;
    auto widget = qobject_cast<MediaPreviewWidget*>(page);
    this->media->setVideoList(widget->mediaList(MetaType::Video));
}

void Widget::play_last_image()
{
    QString current_path = this->media->path();
    auto page = this->page_manager->widget(this->page_manager->currentPageId());
    if (!page)
        return;
    auto widget = qobject_cast<MediaPreviewWidget*>(page);
    const auto last = widget->lastMediaPath(current_path);
    if (last.isEmpty())
        return;
    this->media->close();
    this->media->open(last, MetaType::Image);
}

void Widget::play_next_image()
{
    QString current_path = this->media->path();
    auto page = this->page_manager->widget(this->page_manager->currentPageId());
    if (!page)
        return;
    auto widget = qobject_cast<MediaPreviewWidget*>(page);
    const auto next = widget->nextMediaPath(current_path);
    if (next.isEmpty())
        return;
    this->media->close();
    this->media->open(next, MetaType::Image);
}

void Widget::set_player_show_mode(bool fullscreen)
{
    if (fullscreen)
    {
        this->spliter_size = this->splitter->sizes();
        this->media->hide();
        this->media->setParent(nullptr);
        this->media->setWindowFlag(Qt::Window, true);
        this->media->showFullScreen();
    }
    else
    {
        this->media->hide();
        this->media->setWindowFlag(Qt::Window, false);
        this->media->setParent(this);
        this->splitter->addWidget(this->media);
        this->media->showNormal();
        this->splitter->setSizes(this->spliter_size);
    }
}

void Widget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.fillRect(this->rect(), QColor(224, 229, 233));
}

void Widget::on_folder_selected(const QString &path)
{
    if (this->splitter->widget(1) == this->media)
    {
        this->media->close();
        this->media->hide();
        this->splitter->replaceWidget(1, this->page_manager);
    }
    this->page_manager->show();
    this->page_manager->addPage(path, [path, this](){
        return this->make_preview_page(path);
    });
    this->page_manager->swithToPage(path);
}

void Widget::on_folder_remove(const QString &path)
{
    QWidget* widget = this->page_manager->widget(path);
    widget->close();
    this->page_manager->removePage(path);
    this->folder_list->remove(path);
}

void Widget::on_play_media(const QString &path, MetaType type)
{
    if (!this->media->open(path, type))
        return;
    if (this->splitter->widget(1) != this->media)
    {
        this->page_manager->hide();
        this->splitter->replaceWidget(1, this->media);
    }
    this->media->show();
    this->media->play();
}

void Widget::on_media_moved(const QString &src, const QString &dst)
{
    emit this->newMediaCreated(dst);
}

void Widget::on_media_exit()
{
    if (this->media->isFullScreen())
    {
        this->set_player_show_mode(false);
    }
    this->media->close();
    this->media->hide();
    this->splitter->replaceWidget(1, this->page_manager);
    this->page_manager->show();
}
