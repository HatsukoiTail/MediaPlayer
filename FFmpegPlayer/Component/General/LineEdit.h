#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QWidget>

class LineEdit : public QWidget
{
    Q_OBJECT
public:
    explicit LineEdit(QWidget *parent = nullptr);
    void setText(const QString& text);
    void setFont(QFont font);
    void setLineSpacing(int spacing);
    QString text() const;

private:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    QString _text;
    QFont font;
    int line_spacing {0};
};

#endif // LINEEDIT_H
