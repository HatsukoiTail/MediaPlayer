#ifndef MEDIAINFOCARD_H
#define MEDIAINFOCARD_H

#include "CircularProgressBar.h"
#include "ImageView.h"
#include "LineEdit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class MediaInfoCard : public QWidget
{
    Q_OBJECT
public:
    explicit MediaInfoCard(QWidget *parent = nullptr);

public:
    void setImage(const QImage& image);
    void setMediaName(const QString& fileName);
    void setLeftText(const QString& text);
    void setRightText(const QString& text);
    void setMediaType(const QString& type);
    void setProgress(double value);
    void hideProgress();
    QString name() const
    {
        return this->line_edit->text();
    }

signals:
    void requestPopupMenu(QWidget*);

private:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QImage draw_shadow_effect(const QRect& rect, int shadow_size, int radius);

private:
    bool is_selected {false};

private:
    ImageView* image_view;
    LineEdit* line_edit;
    QLabel* left_label;
    QLabel* right_label;
    QLabel* type_label {nullptr};
    QHBoxLayout* progress_layout {nullptr};
    CircularProgressBar* progress {nullptr};
};

#endif // MEDIAINFOCARD_H
