#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include <QHBoxLayout>
#include <QTextEdit>
#include <QWidget>

class TextEdit : public QWidget {
    Q_OBJECT
public:
    explicit TextEdit(QWidget* parent = nullptr);
    QString text() const;
    void setText(const QString& text);
    void setLineCount(int count);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void edit();
    void finish();

signals:
    void modified(const QString& text);

private:
    QRect activeRect;
    QString text_;
    QTextEdit* editor;
    bool editing = false;
    int lineCount = 1;
};

#endif // TEXTEDITOR_H
