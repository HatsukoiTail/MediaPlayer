#ifndef CIRCULARPROGRESSBAR_H
#define CIRCULARPROGRESSBAR_H

#include <QWidget>

class CircularProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit CircularProgressBar(QWidget *parent = nullptr);
    void setValue(double value);

private:
    void paintEvent(QPaintEvent* event) override;

private:
    double value;
};

#endif // CIRCULARPROGRESSBAR_H
