#ifndef NATIVE_H
#define NATIVE_H

#include <QWidget>

void enableFrameless(QWidget* window);

bool handleNativeEvent(void *message, qintptr *result);

#endif // NATIVE_H
