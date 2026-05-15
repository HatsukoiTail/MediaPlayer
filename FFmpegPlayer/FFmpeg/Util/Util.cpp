#include "Util.h"

extern "C"
{
#include <libavutil/error.h>
}

std::string debug(int error)
{
    char arr[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error, arr, sizeof(arr));
    return std::string(arr);
}
