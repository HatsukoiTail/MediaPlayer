#include "MediaPreviewWidget.h"

#include "FileOutputDialog.h"
#include "MediaInfoCard.h"
#include "Tool.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>

MediaPreviewWidget::MediaPreviewWidget(QWidget *parent)
    : QWidget{parent}
{
    this->layout = new QVBoxLayout(this);
    this->control_bar = new MediaPreviewControlBar(this);
    this->preview_area = new MediaPreviewArea(this);

    this->layout->addWidget(this->control_bar);
    this->layout->addWidget(this->preview_area);

    this->layout->setSpacing(0);

    this->media_finder = new MediaFinder(this);
    this->media_data_loader = new MediaDataLoader(this);
    this->crypto_manager = new CryptoTaskManager(this);

    this->bind();
}

MediaPreviewWidget::~MediaPreviewWidget()
{
    MediaPreviewWidget::close();
}

void MediaPreviewWidget::open(const QString &path)
{
    assert(QDir(path).exists() && !this->is_opened);
    connect(this->media_finder, &MediaFinder::found, this, &MediaPreviewWidget::on_media_found, Qt::QueuedConnection);
    this->media_finder->find(path);
    this->root = path;
    this->is_opened = true;
}

void MediaPreviewWidget::close()
{
    if (!this->tasks.empty())
    {
        auto ret = QMessageBox::question(nullptr, "正在退出",
                                         "后台任务尚未完成，是否继续退出？",
                                         QMessageBox::Ok | QMessageBox::Cancel);
        if (ret != QMessageBox::Ok)
            return;
    }
    disconnect(this->media_finder, &MediaFinder::found, this, &MediaPreviewWidget::on_media_found);
    this->media_finder->stop();
    this->media_data_loader->close();
    this->preview_area->clear();
    this->root.clear();
    this->is_opened = false;
    this->media_list.clear();
}

void MediaPreviewWidget::addItem(const QString &path)
{
    if (!path.startsWith(this->root))
        return;
    this->on_media_found(path);
}

void MediaPreviewWidget::sort(SortingCriteria criteria, SortingOrder order)
{
    std::function<bool(const MediaInfoSummary&, const MediaInfoSummary&)> method;
    switch (criteria) {
    case SortingCriteria::FileName:
        if (order == SortingOrder::Ascending)
        {
            method = [](const MediaInfoSummary& a, const MediaInfoSummary& b){
                QCollator collator(QLocale::Chinese);
                return collator.compare(a.path, b.path) < 0;
            };
        }
        else
        {
            method = [](const MediaInfoSummary& a, const MediaInfoSummary& b){
                QCollator collator(QLocale::Chinese);
                return collator.compare(a.path, b.path) > 0;
            };
        }
        break;
    case SortingCriteria::FileSize:
        if (order == SortingOrder::Ascending)
        {
            method = [](const MediaInfoSummary& a, const MediaInfoSummary& b){
                return a.size < b.size;
            };
        }
        else
        {
            method = [](const MediaInfoSummary& a, const MediaInfoSummary& b){
                return a.size > b.size;
            };
        }
        break;
    default:
        return;
    }
    this->preview_area->sort([method, this](QWidget* a, QWidget* b){
        auto info1 = this->media_list.find(a);
        auto info2 = this->media_list.find(b);
        return method(info1, info2);
    });
    this->media_list.sort(std::move(method));
}

void MediaPreviewWidget::filter(const std::unordered_set<MetaType>& types)
{
    this->media_list.execute([&types](QWidget* widget, MediaInfoSummary& info){
        auto it = std::find(types.begin(), types.end(), info.type);
        if (it == types.end())
            widget->hide();
        else
            widget->show();
    });
    this->media_list.filter(types);
    this->preview_area->flush();
}

bool MediaPreviewWidget::isOpen() const
{
    return this->is_opened;
}



QString MediaPreviewWidget::nextMediaPath(const QString &path) const
{
    return this->media_list.next(path);
}

std::vector<QString> MediaPreviewWidget::mediaList(MetaType type) const
{
    if (type == MetaType::Unknown)
        return this->media_list.list();
    else
        return this->media_list.list(type);
}

QString MediaPreviewWidget::lastMediaPath(const QString &path) const
{
    return this->media_list.last(path);
}

void MediaPreviewWidget::bind()
{
    connect(this->media_finder, &MediaFinder::started, this->control_bar, &MediaPreviewControlBar::showLoadIcon, Qt::QueuedConnection);
    connect(this->media_finder, &MediaFinder::stopped, this->control_bar, &MediaPreviewControlBar::hideLoadIcon, Qt::QueuedConnection);
    connect(this->control_bar, &MediaPreviewControlBar::requestSort, this, &MediaPreviewWidget::sort);
    connect(this->control_bar, &MediaPreviewControlBar::requestFilter, this, &MediaPreviewWidget::filter);
    connect(this->media_data_loader, &MediaDataLoader::loaded, this, &MediaPreviewWidget::on_receive_data, Qt::QueuedConnection);
    connect(this->preview_area, &MediaPreviewArea::itemHiden, this, &MediaPreviewWidget::on_widget_hiden);
    connect(this->crypto_manager, &CryptoTaskManager::progressChanged, this, &MediaPreviewWidget::on_progress_change, Qt::QueuedConnection);
    connect(this->crypto_manager, &CryptoTaskManager::taskFinished, this, &MediaPreviewWidget::on_crypto_finish, Qt::QueuedConnection);
}

void MediaPreviewWidget::on_media_found(const QString &path)
{
    MediaInfoCard* item = new MediaInfoCard();
    auto load = [this, path](QWidget* widget){
        if (!QFile::exists(path))
        {
            this->media_list.remove(widget);
            this->preview_area->removeItem(widget);
            return;
        }
        this->media_data_loader->load(widget, path);
    };
    auto unload = [](QWidget* widget){
        auto item = qobject_cast<MediaInfoCard*>(widget);
        item->setImage(QImage());
        item->setMediaName("");
        item->setLeftText("");
        item->setRightText("");
    };
    connect(item, &MediaInfoCard::requestPopupMenu, this, &MediaPreviewWidget::show_media_menu);
    this->media_list.insert(item, MediaInfoSummary{ .path = path });
    this->control_bar->setFileCount(this->media_list.visibleSize());
    this->preview_area->addItem(item, std::move(load), std::move(unload));
}

void MediaPreviewWidget::on_receive_data(QWidget *widget, std::shared_ptr<MediaData> data)
{
    auto& info = this->media_list.find(widget);
    info.path = data->filePath;
    info.state = data->isEncrypted ? MediaState::Encrypted : MediaState::Unencrypted;
    info.type = data->metaType;
    info.size = data->fileSize;
    info.duration = data->duration;

    auto card = qobject_cast<MediaInfoCard*>(widget);
    if (!this->preview_area->isVisible(widget))
        return;

    card->setImage(data->image);
    card->setMediaName(data->filePath);
    card->setRightText(formatSize(data->fileSize));

    if (info.type == MetaType::Video)
        card->setLeftText(formatTime(data->duration));

    if (info.type == MetaType::Video)
        card->setMediaType("视频");
    else if (info.type == MetaType::Audio)
        card->setMediaType("音频");
    else if (info.type == MetaType::Image)
        card->setMediaType("图片");
}

void MediaPreviewWidget::on_widget_hiden(QWidget *widget)
{
    const auto& info = this->media_list.find(widget);
    this->media_data_loader->cancel(info.path);
}

void MediaPreviewWidget::on_progress_change(const QString &path, size_t cur, size_t total)
{
    auto it = this->tasks.find(path);
    if (it == this->tasks.end())
        return;
    auto widget = this->media_list.find(path);
    if (!widget)
        return;
    auto card = qobject_cast<MediaInfoCard*>(widget);
    card->setProgress(static_cast<double>(cur) * 100.0 / total);
}

void MediaPreviewWidget::on_crypto_finish(const QString &path, CryptoResult res)
{
    auto it = this->tasks.find(path);
    if (it == this->tasks.end())
        return;

    auto widget = this->media_list.find(path);
    if (!widget)
        return;
    auto card = qobject_cast<MediaInfoCard*>(widget);
    card->hideProgress();

    auto& info_it = this->media_list.find(widget);
    if (info_it.state == MediaState::Encrypting)
        info_it.state = MediaState::Encrypted;
    else if (info_it.state == MediaState::Decrypting)
        info_it.state = MediaState::Unencrypted;

    emit this->mediaMoved(it->first, it->second);
    this->tasks.erase(it);
}

void MediaPreviewWidget::show_media_menu(QWidget *widget)
{
    auto& info = this->media_list.find(widget);
    if (!QFile::exists(info.path))
    {
        this->preview_area->removeItem(widget);
        this->media_list.remove(widget);
        return;
    }

    QMenu* menu = new QMenu(this);

    menu->addAction("打开", this, [this, widget](){
        const auto& info = this->media_list.find(widget);
        if (info.state == MediaState::Encrypting || info.state == MediaState::Decrypting)
        {
            auto message = QMessageBox::information(nullptr, "无法播放", "文件正在被使用！");
            return;
        }
        emit this->requestPlay(info.path, info.type);
    });

    if (info.state == MediaState::Encrypting || info.state == MediaState::Decrypting)
    {
        menu->addAction("取消", this, [this, file_path = info.path](){
            this->crypto_manager->cancel(file_path);
        });
    }
    else if (info.state == MediaState::Encrypted)
        menu->addAction("解密", this, [this, path = info.path, widget](){
            auto& it = this->media_list.find(widget);
            if (it.path.isEmpty())
                return;
            const auto output_path = this->open_file_dialog(it.path);
            if (output_path.isEmpty())
                return;
            it.state = MediaState::Decrypting;
            this->tasks.emplace(path, output_path);
            this->crypto_manager->decrypt(path, output_path);
        });
    else
        menu->addAction("加密", this, [this, widget](){
            auto& it = this->media_list.find(widget);
            if (it.path.isEmpty())
                return;
            const auto output_path = this->open_file_dialog(it.path);
            if (output_path.isEmpty())
                return;
            it.state = MediaState::Encrypting;
            this->tasks.emplace(it.path, output_path);
            this->crypto_manager->encrypt(it.path, output_path);
        });

    menu->addAction("重命名", this, [this, widget]{ this->on_rename(widget); });

    menu->addAction("打开文件位置", this, [this, path = info.path](){
        assert(QFileInfo::exists(path) && QFileInfo(path).isFile());
        QString nativePath = QDir::toNativeSeparators(path);
        QString command = "explorer";
        QStringList args;
        args << "/select," << nativePath;
        QProcess::startDetached(command, args);
    });

    menu->addAction("删除", this, [this, file_path = info.path, widget](){
        QFile::remove(file_path);
        this->preview_area->removeItem(widget);
        this->media_list.remove(widget);
    });

    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto pos = QCursor::pos();
    menu->popup(pos);
}

QString MediaPreviewWidget::open_file_dialog(const QString &path)
{
    return QFileDialog::getSaveFileName(this, tr("设置文件路径"), path);
}

void MediaPreviewWidget::on_rename(QWidget *widget)
{
    auto& info = this->media_list.find(widget);

    QInputDialog dialog(this);
    dialog.setWindowTitle("重命名");
    dialog.setLabelText("新名称");
    dialog.setTextValue(formatFileName(info.path));

    QPixmap emptyPixmap(1, 1);
    emptyPixmap.fill(Qt::transparent);
    dialog.setWindowIcon(QIcon(emptyPixmap));

    dialog.setStyleSheet(R"(
            /* 对话框主体 */
            QInputDialog {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f8f9fa);
            }

            /* 标题栏 */
            QInputDialog::title {
                subcontrol-origin: margin;
                subcontrol-position: top center;
                padding: 15px 0px;
                background-color: transparent;
                color: #2c3e50;
                font-size: 18px;
                font-weight: bold;
            }

            /* 标签文字 */
            QLabel {
                color: #34495e;
                font-size: 14px;
                font-weight: 600;
                padding: 8px 0px 8px 20px; /* 上右下左 */
                margin-left: 0px;
                background: transparent;
            }

            /* 输入框 */
            QLineEdit {
                border: 2px solid #dce1e6;
                border-radius: 8px;
                padding: 5px 8px;
                font-size: 14px;
                color: #2c3e50;
                background: white;
                margin: 0px 12px 12px 12px;
                selection-background-color: #3498db;
                min-width: 220px;
            }

            QLineEdit:focus {
                border-color: #3498db;
                background-color: #f8fbfe;
            }

            QLineEdit:hover {
                border-color: #bdc3c7;
            }

            /* 按钮通用样式 */
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3498db, stop:1 #2980b9);
                color: white;
                border: none;
                padding: 5px;
                border-radius: 6px;
                font-size: 14px;
                font-weight: 600;
                min-width: 45px;
            }

            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2980b9, stop:1 #2471a3);
            }

            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2471a3, stop:1 #1b4f72);
            }

            /* 取消按钮特殊样式 */
            QPushButton[text="Cancel"] {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #95a5a6, stop:1 #7f8c8d);
                margin: 0 12px 0 10px;
            }

            QPushButton[text="Cancel"]:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #7f8c8d, stop:1 #707b7c);
            }

            QPushButton[text="Cancel"]:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #707b7c, stop:1 #616a6b);
            }

            /* 确定按钮特殊样式 */
            QPushButton[text="OK"] {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #27ae60, stop:1 #219a52);
                margin: 0 0 0 12px;
            }

            QPushButton[text="OK"]:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #219a52, stop:1 #1e8449);
            }
        )");

    if (dialog.exec() != QDialog::Accepted)
        return;
    QString new_name = dialog.textValue();
    if (new_name.isEmpty())
        return;
    QString new_path = combinePath(formatPath(info.path), new_name);
    if (QFile::exists(new_path))
        return;
    QFile::rename(info.path, new_path);
    auto card = qobject_cast<MediaInfoCard*>(widget);
    card->setMediaName(new_path);
    info.path = new_path;
}
