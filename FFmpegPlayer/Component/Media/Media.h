#ifndef MEDIA_H
#define MEDIA_H

#include "Model.h"

#include <QHBoxLayout>
#include <QWidget>

class Media : public QWidget
{
    Q_OBJECT
public:
    explicit Media(QWidget *parent = nullptr);

public:
    bool open(const QString& path, MetaType type);
    void play();
    void close();
    void setVideoList(const std::vector<QString>& list);

public:
    QString path() const;

signals:
    void requestExit();
    void requestLast();
    void requestNext();
    void requestFullScreen(bool);
    void requestVideoList();

private:
    void bind();

private:
    QHBoxLayout* layout;
    QWidget* media_widget {nullptr};
    MetaType media_type;
    QString media_path;
};

#endif // MEDIA_H
