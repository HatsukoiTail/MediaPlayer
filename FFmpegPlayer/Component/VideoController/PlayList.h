#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class PlayList : public QWidget
{
    Q_OBJECT
public:
    explicit PlayList(QWidget *parent = nullptr);
    void setList(const std::vector<QString>& list);
    void addItem(const QString& item);
    void removeItem(const QString& item);

signals:
    void selected(const QString& item);
    void mouseEnter();

private:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    std::unordered_map<QWidget*, QString> list;

private:
    QVBoxLayout* layout;
    QWidget* scroll_widget;
    QVBoxLayout* scroll_layout;
    QScrollArea* scroll_area;
};

#endif // PLAYLIST_H
