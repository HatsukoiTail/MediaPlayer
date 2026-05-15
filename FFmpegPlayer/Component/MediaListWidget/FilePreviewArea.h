#ifndef FILEPREVIEWAREA_H
#define FILEPREVIEWAREA_H

#include "model.h"

#include <QWidget>
#include <QMap>

class FilePreviewArea : public QWidget
{
    Q_OBJECT
public:
    explicit FilePreviewArea(QWidget *parent = nullptr);

signals:

private:
    QMap<QWidget*, MetaData> file_list;
};

#endif // FILEPREVIEWAREA_H
