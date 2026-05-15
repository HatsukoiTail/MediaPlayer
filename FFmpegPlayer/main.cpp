
#include "Window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // av_log_set_level(AV_LOG_ERROR);
    QApplication a(argc, argv);
    Window w;
    w.show();
    return a.exec();
}
