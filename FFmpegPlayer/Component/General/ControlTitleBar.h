#ifndef CONTROLTITLEBAR_H
#define CONTROLTITLEBAR_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class ControlTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit ControlTitleBar(QWidget *parent = nullptr);
    void setText(const QString& name);

signals:
    void requestExit();
    void mouseHover(bool);

private:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QPushButton* button;
    QLabel* label;
};

#endif // CONTROLTITLEBAR_H
