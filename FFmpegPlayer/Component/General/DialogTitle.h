#ifndef DIALOGTITLE_H
#define DIALOGTITLE_H

#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

class DialogTitle : public QWidget
{
    Q_OBJECT
public:
    explicit DialogTitle(QWidget *parent = nullptr);
    void setTitleName(const QString& text);
    void setCornerRadius(const int radius);
    void setBackgroundColor(const QColor& color);

signals:
    void closeButtonClicked();

private:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int radius = 10;
    QColor background_color = QColor(232, 248, 244);

private:
    QHBoxLayout* layout;
    QLabel* title_icon;
    QLabel* title_name;
    QPushButton* close_button;
};

#endif // DIALOGTITLE_H
