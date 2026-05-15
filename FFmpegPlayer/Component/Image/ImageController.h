#ifndef IMAGECONTROLLER_H
#define IMAGECONTROLLER_H

#include "ControlTitleBar.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

class ImageController : public QWidget
{
    Q_OBJECT
public:
    explicit ImageController(QWidget *parent = nullptr);
    void setText(const QString& text);

signals:
    void requestExit();
    void requestLast();
    void requestNext();
    void mouseHover(bool);

private:
    void bind();
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool hovering {false};

private:
    ControlTitleBar* title;
    QPushButton* last_button;
    QPushButton* next_button;
};

#endif // IMAGECONTROLLER_H
