#include "LineEdit.h"

#include <QPainter>

LineEdit::LineEdit(QWidget *parent)
    : QWidget{parent}
{}

void LineEdit::setText(const QString &text)
{
    if (text != this->_text)
    {
        this->_text = text;
        update();
    }
}

void LineEdit::setFont(QFont font)
{
    this->font = font;
    updateGeometry();
    update();
}

void LineEdit::setLineSpacing(int spacing)
{
    this->line_spacing = spacing;
    updateGeometry();
    update();
}

QString LineEdit::text() const
{
    return this->_text;
}

void LineEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(this->font);

    QFontMetrics metrics(this->font);
    int width = this->width();
    int line_height = metrics.height();

    QString line1, line2;
    if (metrics.horizontalAdvance(this->_text) <= width)
    {
        line1 = this->_text;
    }
    else
    {
        int low = 0;
        int high = this->_text.length();
        int mid = high;
        while (low < high)
        {
            mid = (low + high) / 2;
            QString sub = this->_text.left(mid);
            if (metrics.horizontalAdvance(sub) <= width)
                low = mid + 1;
            else
                high = mid;
        }

        line1 = this->_text.left(mid - 1);
        QString remain = this->_text.mid(line1.length());
        if (!remain.isEmpty())
            line2 = metrics.elidedText(remain, Qt::ElideRight, width);
    }

    painter.drawText(QRect(0, 0, width, line_height), Qt::AlignHCenter | Qt::AlignVCenter, line1);
    painter.drawText(QRect(0, line_height + this->line_spacing, width, line_height), Qt::AlignHCenter | Qt::AlignVCenter, line2);
}

QSize LineEdit::sizeHint() const
{
    QFontMetrics metrics(this->font);
    int height = metrics.height();
    int total_height = height * 2 + this->line_spacing;
    int total_width  = width();
    return {total_width, total_height};
}
