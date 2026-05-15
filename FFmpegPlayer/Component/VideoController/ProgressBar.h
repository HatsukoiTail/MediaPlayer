#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <QHBoxLayout>
#include <QSlider>
#include <QWidget>

class ProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit ProgressBar(QWidget *parent = nullptr);
    void setProgress(double rate);

signals:
    void readySeek();
    void seek(double rate);
    void mouseHover(bool);

private:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QSlider* progress_bar;
};

#endif // PROGRESSBAR_H
