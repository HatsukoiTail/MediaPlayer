#ifndef FILEOUTPUTDIALOG_H
#define FILEOUTPUTDIALOG_H

// #include "Window

#include "DialogBase.h"
#include "DialogTitle.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

class FileOutputDialog : public DialogBase
{
    Q_OBJECT
public:
    FileOutputDialog(QWidget* parent = nullptr);
    QString getFullPath() const;
    void setDefaultName(const QString& name);

private:
    void paintEvent(QPaintEvent* event) override;
    void stylise();
    void bindEvent();

private:
    QString path;

private:
    QVBoxLayout* layout; // 主布局
    QVBoxLayout* content_layout; // 内容布局
    QHBoxLayout* path_layout; // 文件夹行布局
    QHBoxLayout* name_layout; // 文件名行布局
    QHBoxLayout* button_layout; // 底部按钮布局

private:
    DialogTitle* title;
    QLabel* path_label;
    QLineEdit* path_edit;
    QPushButton* path_button;
    QLabel* name_label;
    QLineEdit* name_edit;
    QHBoxLayout* hint_layout;
    QLabel* hint_label;
    QPushButton* confirm_button;
    QPushButton* cancel_button;
};

#endif // FILEOUTPUTDIALOG_H
