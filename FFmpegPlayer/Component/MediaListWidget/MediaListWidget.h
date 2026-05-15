#ifndef MEDIALISTWIDGET_H
#define MEDIALISTWIDGET_H

#include <QScrollArea>
#include <QMap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class MediaListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MediaListWidget(QWidget* parent = nullptr);
    void remove(const QString& path);

signals:
    void toRemove(const QString& path);
    void selected(const QString& path);

private:
    void add_item(const QString& path);
    void open_folder();
    void stylise();

private:
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<QString, QWidget*> list;
    QWidget* selected_widget = nullptr;

private:
    QVBoxLayout* layout;
    QScrollArea* scroll_area;
    QWidget* scroll_widget;
    QVBoxLayout* scroll_layout;
    QFrame* divider;
    QHBoxLayout* new_button_layout;
    QPushButton* new_button;
};

#endif // MEDIALISTWIDGET_H
