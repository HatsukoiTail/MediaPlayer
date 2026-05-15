#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QWidget>

class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);
    void setImage(const QImage& image);
    void fitToSize();

private:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage image;
};

#endif // IMAGEVIEW_H
