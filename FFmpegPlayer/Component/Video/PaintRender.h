#ifndef PAINTRENDER_H
#define PAINTRENDER_H

#include "SmartStruct.h"

#include <QWidget>

class PaintRender : public QWidget
{
    Q_OBJECT
public:
    explicit PaintRender(QWidget *parent = nullptr);
    void draw(AVFramePointer frame);

private:
    void paintEvent(QPaintEvent* event) override;
    QRect calc_draw_rect();

private:
    std::mutex mutex;
    AVFramePointer frame;
    QImage image;
};

#endif // PAINTRENDER_H
