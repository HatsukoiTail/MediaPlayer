


using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.MediaInfo;

public static class SwsHelper
{
    // public static unsafe bool SwsFrame(AVFrame* dst, AVFrame* src)
    // {
    //     SwsContext* sws_ctx = ffmpeg.sws_getContext(src->width, src->height, (AVPixelFormat)src->format,
    //                                                 dst->width, dst->height, (AVPixelFormat)dst->format,
    //                                                 (int)SwsFlags.SWS_BILINEAR, null, null, null);
    //     if (sws_ctx == null)
    //     {
    //         return false;
    //     }
    //     int result = ffmpeg.sws_scale_frame(sws_ctx, dst, src);
    // }
}