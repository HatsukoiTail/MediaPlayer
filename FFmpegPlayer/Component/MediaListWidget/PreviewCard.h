// #ifndef PREVIEWCARD_H
// #define PREVIEWCARD_H

// #include <QVBoxLayout>
// #include <QLabel>
// #include <QTextEdit>
// #include <QWidget>

// class PreviewCard : public QWidget
// {
//     Q_OBJECT
// public:
//     explicit PreviewCard(QWidget *parent = nullptr);

// public slots:
//     void select();
//     void unselect();

// public:
//     void setFileName(const QString& fileName);
//     void setFileSize(qint64 size);
//     void setLastModified(qint64 lastModified);
//     void setDuration(qint64 msec);
//     void setThumbnail(const QImage& image);

// signals:
//     void doubleClicked(const QString& path);
//     void rightMouseClicked();

// private:
//     void paintEvent(QPaintEvent* event) override;
//     void enterEvent(QEnterEvent* event) override;
//     void leaveEvent(QEvent* event) override;
//     void mouseReleaseEvent(QMouseEvent* event) override;
//     void mouseDoubleClickEvent(QMouseEvent* event) override;

// private:
//     QVBoxLayout* layout;


//     QLabel* thumbnail;
//     QLabel* fileNameLabel;
//     QLabel* fileSizeLabel;
//     QLabel* lastModifiedLabel;
//     QLabel* durationLabel;
//     bool isHovering = false;
//     bool isSelected = false;
//     qreal progress = 1.0;
// };

// #endif // PREVIEWCARD_H
