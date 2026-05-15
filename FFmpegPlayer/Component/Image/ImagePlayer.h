#ifndef IMAGEPLAYER_H
#define IMAGEPLAYER_H

#include "Image.h"
#include "ImageController.h"

#include <QHBoxLayout>
#include <QWidget>

class ImagePlayer : public QWidget
{
    Q_OBJECT
public:
    explicit ImagePlayer(QWidget *parent = nullptr);
    ~ImagePlayer() override;
    bool open(const QString& path);
    void close();
    void play();

signals:
    void requestExit();
    void requestLast();
    void requestNext();

private:
    void bind();
    void on_hover_change(bool hover);
    void on_timer_timeout();

private:
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QTimer* controller_timer {nullptr};
    bool mouse_hovering {false};

private:
    Image* image;
    ImageController* controller;
};

#endif // IMAGEPLAYER_H
