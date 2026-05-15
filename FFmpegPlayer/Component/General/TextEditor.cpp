#include "TextEditor.h"

#include <QApplication>
#include <QPainter>
#include <QTextLayout>
#include <QTimer>

TextEdit::TextEdit(QWidget *parent)
    : QWidget(parent)
{
    QFontMetrics fm(font());
    setFixedHeight(fm.lineSpacing() * 2 + 6);
}

QString TextEdit::text() const
{
    return this->text_;
}

void TextEdit::setText(const QString &text)
{
    this->text_ = text;
    update();
}

void TextEdit::setLineCount(int count)
{
    this->lineCount = count;
}

void TextEdit::paintEvent(QPaintEvent *event)
{
    if (this->editing)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(Qt::black);
    painter.setFont(font());

    auto margins = contentsMargins();

    QFontMetrics fm(font());
    int lineHeight = fm.lineSpacing();
    int offset = (height() - lineHeight * this->lineCount) / 2;

    QStringList lines;

    int maxWidth = width() - margins.left() - margins.right();
    int widthSoFar = 0;

    QString str;
    int i = 0;
    while (lines.size() < this->lineCount - 1)
    {
        if (i == this->text_.size())
        {
            if (!str.isEmpty())
                lines.append(str);
            break;
        }
        int charWidth = fm.horizontalAdvance(this->text_[i]);
        if (widthSoFar + charWidth > maxWidth)
        {
            lines.append(str);
            str.clear();
            widthSoFar = 0;
            continue;
        }
        widthSoFar += charWidth;
        str += this->text_[i];
        ++i;
    }

    if (i != this->text_.size())
    {
        QString remaining = this->text_.mid(i);
        QString lastLine = fm.elidedText(remaining, Qt::ElideRight, maxWidth);
        lines.append(lastLine);
    }

    for (int i = 0; i < lines.size(); ++i)
    {
        const int x = (width() - fm.horizontalAdvance(lines[i])) / 2;
        const int y = offset + lineHeight * i + fm.ascent();
        painter.drawText(x, y, lines[i]);
    }

    if (!lines.isEmpty())
    {
        const int x = (width() - fm.horizontalAdvance(lines[0])) / 2;
        const int y = offset;
        const int width = maxWidth;
        const int height = lineHeight * lines.size();
        this->activeRect = QRect(x, y, width, height);
    }
}

void TextEdit::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
    if (this->editing && this->editor)
    {
        auto margins = contentsMargins();
        this->editor->setFixedSize(this->width() - margins.left() - margins.right(), this->height() - margins.top() - margins.bottom());
        this->editor->move(margins.left(), margins.top());
    }
}

void TextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (this->activeRect.contains(event->pos()))
    {
        this->edit();
    }
}

bool TextEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (this->editing && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF globalPos = mouseEvent->globalPosition();
        if (!this->editor->geometry().contains(this->editor->mapFromGlobal(globalPos).toPoint()))
        {
            this->finish();
            return true;
        }
    }
    if (watched == this->editor)
    {
        if (event->type() == QEvent::FocusOut)
        {
            QTimer::singleShot(0, this, &TextEdit::finish);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TextEdit::edit()
{
    if (this->editing)
        return;

    this->editing = true;
    update();

    this->editor = new QTextEdit(this);

    this->editor->setPlainText(this->text_);
    this->editor->setFont(font());
    this->editor->setFrameStyle(QFrame::NoFrame);
    this->editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->editor->setWordWrapMode(QTextOption::WordWrap);
    this->editor->setAlignment(Qt::AlignHCenter | Qt::AlignCenter);
    this->editor->setStyleSheet("background:transparent;");

    auto margins = contentsMargins();
    this->editor->setFixedSize(width() - margins.left() - margins.right(), height() - margins.top() - margins.bottom());
    this->editor->move(margins.left(), margins.top());

    this->editor->raise();
    this->editor->setFocus();
    this->editor->selectAll();

    this->editor->show();

    QApplication::instance()->installEventFilter(this);
}

void TextEdit::finish()
{
    if (!this->editing)
        return;

    this->editing = false;
    QApplication::instance()->removeEventFilter(this);
    const QString text = this->editor->toPlainText();
    this->editor->deleteLater();
    this->editor = nullptr;
    emit this->modified(text);
}
