// #include "PreviewCard.h"

// #include <QMouseEvent>
// #include <QPainter>
// #include <QPainterPath>

// PreviewCard::PreviewCard(QWidget *parent)
//     : QWidget{parent}
// {
//     this->layout = new QHBoxLayout(this);

//     // 创建文件信息的布局容器
//     QVBoxLayout* valuesLayout = new QVBoxLayout();
//     this->thumbnail = new QLabel(this);
//     this->layout->addWidget(this->thumbnail, 0, Qt::AlignCenter);
//     this->layout->addLayout(valuesLayout);

//     this->fileNameLabel = new QLabel(this);
//     this->fileSizeLabel = new QLabel(this);
//     this->lastModifiedLabel = new QLabel(this);
//     this->durationLabel = new QLabel(this);

//     valuesLayout->setContentsMargins(0, 0, 0, 0);
//     valuesLayout->setSpacing(0);
//     valuesLayout->addWidget(this->fileNameLabel);
//     valuesLayout->addWidget(this->fileSizeLabel);
//     valuesLayout->addWidget(this->lastModifiedLabel);
//     valuesLayout->addWidget(this->durationLabel);

//     QFont font;
//     font.setFamilies({"Calibri", "Microsoft YaHei"});
//     font.setPointSize(10);
//     this->thumbnail->setFixedHeight(60);
//     this->fileNameLabel->setFont(font);
//     this->fileSizeLabel->setFont(font);
//     this->lastModifiedLabel->setFont(font);
//     this->durationLabel->setFont(font);

//     this->setMaximumSize(300, 100);
//     this->setMaximumHeight(60);
//     this->layout->setContentsMargins(3, 3, 3, 3);
//     this->setCursor(Qt::PointingHandCursor);
// }

// void PreviewCard::setFileName(const QString& fileName)
// {
//     this->fileNameLabel->setText(fileName);
//     this->setToolTip(fileName);
// }

// void PreviewCard::setFileSize(qint64 size)
// {
//     this->fileSize = size;
//     this->fileSizeLabel->setText(formatFileSize(size));
// }

// void PreviewCard::setLastModified(qint64 lastModified)
// {
//     this->lastModified = lastModified;
//     this->lastModifiedLabel->setText(formatLastModified(lastModified));
// }

// void PreviewCard::setDuration(qint64 msec)
// {
//     this->duration = msec;
//     this->durationLabel->setText(formatDuration(msec));
// }

// void PreviewCard::setThumbnail(const QImage &image)
// {
//     this->thumbnail->setPixmap(QPixmap::fromImage(image.scaledToHeight(this->thumbnail->height())));
// }

// QString PreviewCard::getFileName() const
// {
//     return this->fileNameLabel->text();
// }

// qint64 PreviewCard::getFileSize() const
// {
//     return this->fileSize;
// }

// qint64 PreviewCard::getLastModified() const
// {
//     return this->lastModified;
// }

// qint64 PreviewCard::getDuration() const
// {
//     return this->duration;
// }

// QPixmap PreviewCard::getThumbnail() const
// {
//     return this->thumbnail->pixmap();
// }

// void PreviewCard::setProgress(qreal progress)
// {
//     this->progress = progress;
//     QWidget::update();
// }

// void PreviewCard::select()
// {
//     this->isSelected = true;
//     QWidget::update();
// }

// void PreviewCard::unselect()
// {
//     this->isSelected = false;
//     QWidget::update();
// }

// void PreviewCard::paintEvent(QPaintEvent *event)
// {
//     if (this->progress != 1.0)
//     {
//         QPainter painter(this);
//         painter.setRenderHint(QPainter::Antialiasing);
//         QPainterPath backgroundPath;
//         backgroundPath.addRoundedRect(this->rect(), 3, 3);
//         // painter.fillPath(backgroundPath, QColor(186, 186, 186));
//         painter.setClipPath(backgroundPath);
//         const int filledHeight = this->progress * this->height();
//         const QRect fillRect(0, this->height() - filledHeight, this->width(), filledHeight);
//         painter.fillRect(fillRect, QColor(245, 204, 204, 0.8 * 256 - 1));
//         painter.setClipping(false);
//     }
//     else
//     {
//         if (this->isHovering)
//         {
//             QPainter painter(this);
//             painter.setRenderHint(QPainter::Antialiasing);
//             QPainterPath backgroundPath;
//             backgroundPath.addRoundedRect(this->rect(), 3, 3);
//             painter.fillPath(backgroundPath, QColor(196, 213, 228));
//         }
//         if (this->isSelected)
//         {
//             QPainter painter(this);
//             painter.setRenderHint(QPainter::Antialiasing);
//             QPainterPath backgroundPath;
//             backgroundPath.addRoundedRect(this->rect(), 3, 3);
//             painter.fillPath(backgroundPath, QColor(186, 186, 186));
//         }
//     }
//     QWidget::paintEvent(event);
// }

// void PreviewCard::enterEvent(QEnterEvent *event)
// {
//     this->isHovering = true;
//     this->update();
//     QWidget::enterEvent(event);
// }

// void PreviewCard::leaveEvent(QEvent *event)
// {
//     this->isHovering = false;
//     this->update();
//     QWidget::leaveEvent(event);
// }

// void PreviewCard::mouseReleaseEvent(QMouseEvent *event)
// {
//     if (event->button() == Qt::RightButton)
//     {
//         emit this->rightMouseClicked();
//     }
//     QWidget::mouseReleaseEvent(event);
// }

// void PreviewCard::mouseDoubleClickEvent(QMouseEvent *event)
// {
//     if (event->button() & Qt::LeftButton)
//     {
//         emit this->doubleClicked(this->fileNameLabel->text());
//     }
//     QWidget::mouseDoubleClickEvent(event);
// }
