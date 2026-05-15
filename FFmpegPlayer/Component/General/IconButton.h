#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include <QPropertyAnimation>
#include <QPushButton>

class IconButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale WRITE setScale)
public:
    explicit IconButton(QWidget* parent = nullptr);
    IconButton(const QIcon &icon, QWidget *parent = nullptr);

public:
    void setIcon(const QIcon& icon, const QSize& size);
    void setAnimationDuration(int duration);
    qreal scale() const;
    void setScale(qreal scale);

private:
    void startAnimation(qreal from, qreal to);

private:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QIcon icon;
    QSize icon_size;
    qreal scale_rate;
    QPropertyAnimation* animation;
};

#endif // ICONBUTTON_H
