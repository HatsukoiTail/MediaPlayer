#ifndef HELPER_H
#define HELPER_H

#include "Frame.h"
#include "PacketQueue.h"

enum class FFmpegResult { Success, Eof, Eagain, Error };

int decode_frame();


void test()
{
    av_dict_get()
}

#endif // HELPER_H