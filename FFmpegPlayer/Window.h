#ifndef WINDOW_H
#define WINDOW_H

#include "Widget.h"
#include "WindowTitle.h"

#include <QVBoxLayout>

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);

public:
    void shift();
    void moveToCenter();

private:
    void bindEvent();

private:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    QVBoxLayout* layout;
    WindowTitle* title;
    Widget* widget;
};

#endif // WINDOW_H
