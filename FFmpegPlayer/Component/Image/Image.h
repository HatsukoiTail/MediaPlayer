#ifndef IMAGE_H
#define IMAGE_H

#include <QWidget>

class Image : public QWidget
{
    Q_OBJECT
public:
    explicit Image(QWidget *parent = nullptr);
    bool open(const QString& path);
    void play();

private:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void center_image();
    void translate_image_pos();

private:
    QImage image;
    // 缩放
    QTransform trans;
    double scale {1.0};
    QPointF offset {0.0, 0.0};
    double min_scale {1.0};
    // 移动
    bool dragging {false};
    QPoint last_pos;

    bool fit_mode {true};
};

#endif // IMAGE_H
