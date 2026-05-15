#ifndef WINDOWTITLE_H
#define WINDOWTITLE_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class WindowTitle : public QWidget
{
    Q_OBJECT
public:
    explicit WindowTitle(QWidget *parent = nullptr);

signals:
    void minimize();
    void shift();
    void quit();

private:
    void stylise();
    void bind();

private:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QHBoxLayout* layout;
    QLabel* titleIcon;
    QLabel* titleName;
    QHBoxLayout* titleButtonLayout;
    QPushButton* minimizeButton;
    QPushButton* shiftButton;
    QPushButton* closeButton;
};

#endif // WINDOWTITLE_H
