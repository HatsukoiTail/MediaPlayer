#ifndef WINDOW_H
#define WINDOW_H

#include <QVBoxLayout>
#include <QWidget>

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
};

#endif // WINDOW_H
