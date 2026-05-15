#ifndef MEDIAPREVIEWWIDGET_H
#define MEDIAPREVIEWWIDGET_H

#include "CryptoTaskManager.h"
#include "MediaDataLoader.h"
#include "MediaFinder.h"
#include "MediaPreviewArea.h"
#include "MediaPreviewControlBar.h"
#include "Model.h"

#include "MediaInfoList.h"

#include <QVBoxLayout>
#include <QWidget>

class MediaPreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPreviewWidget(QWidget *parent = nullptr);
    ~MediaPreviewWidget() override;

public slots:
    void open(const QString& path);
    void close();
    void addItem(const QString& path);

public:
    bool isOpen() const;
    std::vector<QString> mediaList(MetaType type = MetaType::Unknown) const;
    QString lastMediaPath(const QString& path) const;
    QString nextMediaPath(const QString& path) const;

signals:
    void mediaMoved(const QString&, const QString&);
    void requestPlay(const QString&, MetaType);

private:
    void bind();
    void sort(SortingCriteria criteria, SortingOrder order);
    void filter(const std::unordered_set<MetaType>& types);
    void on_media_found(const QString& path);
    void on_receive_data(QWidget* widget, std::shared_ptr<MediaData> data);
    void on_widget_hiden(QWidget* widget);
    void on_progress_change(const QString& path, size_t cur, size_t total);
    void on_crypto_finish(const QString& path, CryptoResult res);
    void show_media_menu(QWidget* widget);
    QString open_file_dialog(const QString& path);
    void on_rename(QWidget* widget);

private:
    MediaInfoList media_list;
    std::unordered_map<QString, QString> tasks;

private:
    bool is_opened = false;
    QString root;

private:
    MediaDataLoader* media_data_loader;
    MediaFinder* media_finder;
    CryptoTaskManager* crypto_manager;

private:
    QVBoxLayout* layout;
    MediaPreviewControlBar* control_bar;
    MediaPreviewArea* preview_area;
};

#endif // MEDIAPREVIEWWIDGET_H
