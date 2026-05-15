#ifndef MEDIAPREVIEWCARD_H
#define MEDIAPREVIEWCARD_H

#include "TextEditor.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class MediaPreviewCard : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPreviewCard(QWidget *parent = nullptr);
    void setImage(const QPixmap& image);
    void setName(const QString& name);
    void setSize(const qint64 size);
    void setDuration(const qint64 duration);
    // void modify();

public:
    QString name() const;
    qint64 size() const;
    qint64 duration() const;

private:
    void styled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

signals:
    void doubleClicked(MediaPreviewCard*);
    void rightClicked(MediaPreviewCard*);
    void modified(const QString& name);

private:
    // 与布局相关的成员变量
    QVBoxLayout* main_layout;
    QHBoxLayout* bottom_layout;

private:
    QPixmap image;
    QLabel* image_view;
    TextEdit* name_view;
    QLabel* size_view;
    QLabel* duration_view;

    qint64 size_data;
    qint64 duration_data;
};

#endif // MEDIAPREVIEWCARD_H
