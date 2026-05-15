#ifndef WIDGET_H
#define WIDGET_H

#include "Media.h"
#include "MediaListWidget.h"
#include "PageManager.h"

#include <QSplitter>
#include <QStackedLayout>
#include <QWidget>

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);

private:
    void bind();
    QWidget* make_preview_page(const QString& path);
    void set_player_show_mode(bool fullscreen);
    void set_video_list();
    void play_last_image();
    void play_next_image();

signals:
    void requestPlay(const QString&, MetaType);
    void newMediaCreated(const QString& path);

private:
    void paintEvent(QPaintEvent* event) override;
    void on_folder_selected(const QString& path);
    void on_folder_remove(const QString& path);
    void on_play_media(const QString&, MetaType);
    void on_media_moved(const QString& src, const QString& dst);
    void on_media_exit();

private:
    QList<int> spliter_size;

private:
    QHBoxLayout* layout;
    QSplitter* splitter;
    MediaListWidget* folder_list;
    PageManager* page_manager;
    Media* media;
};
#endif // WIDGET_H
