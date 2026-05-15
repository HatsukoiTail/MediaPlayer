#ifndef MEDIALISTITEM_H
#define MEDIALISTITEM_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class MediaListItem : public QWidget
{
    Q_OBJECT
public:
    explicit MediaListItem(QWidget *parent = nullptr);
    void setText(const QString& text);
    void select();
    void unselect();

signals:
    void closeButtonClicked(QWidget*);
    void selected(QWidget*);
    void unselected(QWidget*);

private:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool is_hovered = false;
    bool is_selected = false;
    QSize button_size;

private:
    QHBoxLayout* layout;
    QLabel* label;
    QPushButton* button;
};

#endif // MEDIALISTITEM_H
