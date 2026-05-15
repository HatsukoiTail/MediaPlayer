#ifndef SPEEDMENU_H
#define SPEEDMENU_H

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class SpeedMenu : public QWidget
{
    Q_OBJECT
public:
    explicit SpeedMenu(QWidget *parent = nullptr);

signals:
    void speedSelected(double);
    void mouseEnter();
    void mouseLeave();

private:
    void addSpeedItem(const QString& speed_str);
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QVBoxLayout* layout;
};

#endif // SPEEDMENU_H
