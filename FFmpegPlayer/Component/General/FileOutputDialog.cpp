#include "FileOutputDialog.h"

#include <QFileDialog>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>

FileOutputDialog::FileOutputDialog(QWidget* parent)
    : DialogBase{parent}
{
    this->layout = new QVBoxLayout(this);
    this->content_layout = new QVBoxLayout();
    this->path_layout = new QHBoxLayout();
    this->name_layout = new QHBoxLayout();
    this->hint_layout = new QHBoxLayout();
    this->button_layout = new QHBoxLayout();
    this->title = new DialogTitle(this);
    this->path_label = new QLabel(this);
    this->path_edit = new QLineEdit(this);
    this->path_button = new QPushButton(this);
    this->name_label = new QLabel(this);
    this->name_edit = new QLineEdit(this);
    this->hint_label = new QLabel(this);
    this->confirm_button = new QPushButton(this);
    this->cancel_button = new QPushButton(this);

    this->layout->addWidget(this->title, 0);
    this->layout->addLayout(this->content_layout, 1);

    this->content_layout->addStretch(2);
    this->content_layout->addLayout(this->path_layout);
    this->content_layout->addStretch(2);
    this->content_layout->addLayout(this->name_layout);
    this->content_layout->addStretch(2);
    this->content_layout->addLayout(this->hint_layout);
    this->content_layout->addStretch(1);
    this->content_layout->addLayout(this->button_layout);

    this->path_layout->addWidget(this->path_label);
    this->path_layout->addWidget(this->path_edit);
    this->path_layout->addWidget(this->path_button);
    this->name_layout->addWidget(this->name_label);
    this->name_layout->addWidget(this->name_edit);
    this->hint_layout->addWidget(this->hint_label);
    this->button_layout->addWidget(this->cancel_button);
    this->button_layout->addWidget(this->confirm_button);

    this->layout->setAlignment(Qt::AlignTop);
    this->layout->setContentsMargins(0, 0, 0, 0);
    this->content_layout->setContentsMargins(14, 14, 14, 14);
    this->hint_layout->setAlignment(Qt::AlignCenter);

    this->title->setTitleName("设置输出文件路径");

    this->path_label->setText("文件夹： ");
    this->path_edit->setPlaceholderText("选择文件夹...");
    this->path_button->setText("⋯");
    this->name_label->setText("文件名： ");
    this->name_edit->setPlaceholderText("输入文件名...");

    this->confirm_button->setText("确认");
    this->confirm_button->setFixedSize(48, 35);
    this->cancel_button->setText("取消");
    this->cancel_button->setFixedSize(48, 35);

    this->setTitleHeight(this->title->height());

    this->stylise();
    this->bindEvent();
}

QString FileOutputDialog::getFullPath() const
{
    return this->path;
}

void FileOutputDialog::setDefaultName(const QString &name)
{
    this->name_edit->setText(name);
}

void FileOutputDialog::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(254, 254, 255, 250));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(this->contentsRect(), this->cornerRadius(), this->cornerRadius());
}

void FileOutputDialog::stylise()
{
    const QString edit_style = "QLineEdit {"
                               "    background-color: #FFFFFF;"      // 背景颜色
                               "    border: 2px solid #CCCCCC;"     // 边框
                               "    border-radius: 5px;"            // 圆角
                               "    padding: 5px 10px;"             // 内边距：上下 左右
                               "    font-size: 14px;"               // 字体大小
                               "    color: #333333;"                // 文字颜色
                               "}"
                               "QLineEdit:focus {"                  // 获得焦点时的样式
                               "    border: 2px solid #2196F3;"
                               "    background-color: #F5FBFF;"
                               "}";
    this->path_edit->setStyleSheet(edit_style);
    this->name_edit->setStyleSheet(edit_style);

    this->path_button->setFixedSize(36, 30);

    QLabel* label = new QLabel(this);
    this->name_layout->addWidget(label);
    label->setFixedSize(this->path_button->size());
}

void FileOutputDialog::bindEvent()
{
    connect(this->title, &DialogTitle::closeButtonClicked, this, [this](){
        this->reject();
    });
    connect(this->path_button, &QPushButton::clicked, this, [this](){
        const QString select_dir = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), "../../", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (select_dir.isEmpty())
            return;
        this->path_edit->setText(select_dir);
    });
    connect(this->confirm_button, &QPushButton::clicked, this, [this](){
        const auto dir_path = this->path_edit->text();
        const auto name = this->name_edit->text();
        if (dir_path.isEmpty())
        {
            this->hint_label->setText("❌ 请选择文件夹");
            return;
        }
        if (name.isEmpty())
        {
            this->hint_label->setText("❌ 请输入文件名");
            return;
        }
        QDir dir(dir_path);
        if (!dir.exists())
        {
            this->hint_label->setText("❌ 文件夹不存在");
            return;
        }
        static const QRegularExpression invalidChars(R"([<>:"|?*\\/])");
        if (name.contains(invalidChars))
        {
            this->hint_label->setText("❌ 文件名存在非法字符");
            return;
        }
        const auto path = dir.absoluteFilePath(name);
        if (QFileInfo::exists(path))
        {
            this->hint_label->setText("❌ 文件已存在");
            return;
        }
        if (path.length() > 260)
        {
            this->hint_label->setText("❌ 文件路径过长");
            return;
        }
        this->path = path;
        this->accept();
    });
    connect(this->cancel_button, &QPushButton::clicked, this, [this](){
        this->reject();
    });
}
