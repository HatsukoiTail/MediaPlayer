#ifndef UTIL_H
#define UTIL_H

extern "C"
{
#include <portaudio.h>
}

#include <string>

std::string debug(int error);

#endif // UTIL_H
