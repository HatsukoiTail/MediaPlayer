
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include "PixelConverter.h"
#include "Slic.h"

void test_slic(QWidget* parent)
{
    auto layout = new QHBoxLayout(parent);
    auto old_label = new QLabel(parent);
    auto new_label = new QLabel(parent);

    layout->addWidget(old_label);
    layout->addWidget(new_label);

    const QString file_path("test.png");

    QImage old_image(file_path);
    old_image = old_image.convertToFormat(QImage::Format_RGB888);

    qDebug() << "Image format: " << old_image.format() << old_image.width() << old_image.height();
    old_label->setPixmap(QPixmap::fromImage(old_image));

    // LAB图像数据容器
    int width = old_image.width();
    int height = old_image.height();
    std::vector<float> lab_data(width * height * 3);

    // 提取 RGB 并转为 Lab (注意像素对齐)
    for (int y = 0; y < height; ++y)
    {
        // 获取当前行的起始指针
        const uchar* linePtr = old_image.scanLine(y);

        for (int x = 0; x < width; ++x)
        {
            int byteIdx = x * 3;
            auto rgb_r = linePtr[byteIdx];
            auto rgb_g = linePtr[byteIdx + 1];
            auto rgb_b = linePtr[byteIdx + 2];

            auto [l, a, b] = RGBtoLAB(rgb_r, rgb_g, rgb_b);

            // 存入一维数组
            int pixelIdx = (y * width + x);
            lab_data[pixelIdx * 3] = l;
            lab_data[pixelIdx * 3 + 1] = a;
            lab_data[pixelIdx * 3 + 2] = b;
        }
    }


    auto start = std::chrono::high_resolution_clock::now();
    // 运行 SLIC 算法
    auto slic_data = apply_SLIC(lab_data, width, height, 10, 10);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;

    qDebug() << "SLIC Execution Time: " << elapsed.count() << " ms";

    // 将结果写回 QImage
    QImage new_image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y)
    {
        // 获取新图像当前行的可写指针
        uchar* linePtr = new_image.scanLine(y);

        for (int x = 0; x < width; ++x)
        {
            int pixelIdx = (y * width + x);
            auto lab_l = slic_data[pixelIdx * 3];
            auto lab_a = slic_data[pixelIdx * 3 + 1];
            auto lab_b = slic_data[pixelIdx * 3 + 2];

            auto [r, g, b] = LABtoRGB(lab_l, lab_a, lab_b);

            int byteIdx = x * 3;
            linePtr[byteIdx] = r;
            linePtr[byteIdx + 1] = g;
            linePtr[byteIdx + 2] = b;
        }
    }

    new_label->setPixmap(QPixmap::fromImage(new_image));
}

int main(int argc, char *argv[])
{
    // run test with Qt GUI
    QApplication a(argc, argv);
    QWidget w;
    test_slic(&w);
    w.show();
    return a.exec();
}
