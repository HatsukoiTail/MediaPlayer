#ifndef VOLUMEBAR_H
#define VOLUMEBAR_H

#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

class VolumeBar : public QWidget
{
    Q_OBJECT
public:
    explicit VolumeBar(QWidget *parent = nullptr);

signals:
    void volumeChange(int);
    void mouseEnter();
    void mouseLeave();

private:
    void on_slider_change(int value);
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QVBoxLayout* layout;
    QLabel* label;
    QSlider* slider;
};

#endif // VOLUMEBAR_H
