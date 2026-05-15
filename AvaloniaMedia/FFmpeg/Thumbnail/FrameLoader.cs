using System.Threading;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using AvaloniaMedia.FFmpeg.Decode;
using AvaloniaMedia.FFmpeg.Demux;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.Helper;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Thumbnail;

public static class FrameLoader
{
    private const int ThumbMaxWidth = 320;
    private const int ThumbMaxHeight = 180;
    private const int SWS_BILINEAR = 2;

    public readonly static AVPixelFormat[] AllowedFormat = [
        AVPixelFormat.AV_PIX_FMT_RGBA,
    ];

    public record Option(int Width = 0, int Height = 0, double Position = double.NaN);

    private static unsafe WriteableBitmap? LoadFrame(string filePath, Option option, CancellationToken token = default)
    {
        using var formatCtx = new FormatContext();
        using var codecCtx = new CodecContext();

        formatCtx.OpenAsReader(filePath);

        int index = ffmpeg.av_find_best_stream(formatCtx.FormatContextPointer, AVMediaType.AVMEDIA_TYPE_VIDEO, -1, -1, null, 0);
        if (index < 0) return null;

        codecCtx.Open(formatCtx.GetStream(index));

        if (option.Position > 0)
        {
            long target = (long)(option.Position * ffmpeg.AV_TIME_BASE);
            ffmpeg.av_seek_frame(formatCtx.FormatContextPointer, index, target, ffmpeg.AVSEEK_FLAG_BACKWARD);
        }

        using var packet = Packet.CreatePacket();
        using var frame = Frame.CreateFrame();

        while (true)
        {
            token.ThrowIfCancellationRequested();
            int result = ffmpeg.av_read_frame(formatCtx.FormatContextPointer, packet.PacketPointer);
            if (result == ffmpeg.AVERROR_EOF)
            {
                return null;
            }
            else if (result < 0)
            {
                throw new FFmpegException(result);
            }

            if (packet.StreamIndex != index)
            {
                packet.Unref();
                continue;
            }

            result = ffmpeg.avcodec_send_packet(codecCtx.CodecContextPointer, packet.PacketPointer);
            packet.Unref();

            if (result < 0 && result != ffmpeg.AVERROR(ffmpeg.EAGAIN))
            {
                throw new FFmpegException(result);
            }

            result = ffmpeg.avcodec_receive_frame(codecCtx.CodecContextPointer, frame.FramePointer);
            if (result < 0 && result != ffmpeg.AVERROR_EOF && result != ffmpeg.AVERROR(ffmpeg.EAGAIN))
            {
                throw new FFmpegException(result);
            }
            if (result >= 0)
            {
                break;
            }
        }

        if (((option.Width <= 0 && option.Height <= 0) ||
             (option.Width == frame.Width && option.Height == frame.Height) ||
             (option.Width == frame.Width && option.Height <= 0) ||
             (option.Height == frame.Height && option.Width <= 0))
            &&
            (AVPixelFormat)frame.Format == AVPixelFormat.AV_PIX_FMT_RGBA)
        {
            // 无需转换
            return MediaHelper.LoadImage(frame.DataPointer(0),
                                         frame.Width,
                                         frame.Height,
                                         frame.LineSize(0));
        }

        using var targetFrame = Frame.CreateFrame();

        if (option.Width > 0 && option.Height > 0)
        {
            targetFrame.FramePointer->width = option.Width;
            targetFrame.FramePointer->height = option.Height;
        }
        else if (option.Width > 0 && option.Height <= 0)
        {
            var targetHeight = (double)frame.Height / frame.Width * option.Width;
            targetFrame.FramePointer->width = option.Width;
            targetFrame.FramePointer->height = (int)targetHeight;
        }
        else if (option.Height > 0 && option.Width <= 0)
        {
            var targetWidth = (double)frame.Width / frame.Height * option.Height;
            targetFrame.FramePointer->width = (int)targetWidth;
            targetFrame.FramePointer->height = option.Height;
        }
        else
        {
            targetFrame.FramePointer->width = frame.Width;
            targetFrame.FramePointer->height = frame.Height;
        }
        targetFrame.FramePointer->format = (int)AVPixelFormat.AV_PIX_FMT_RGBA;

        var swsCtx = ffmpeg.sws_getContext(
                    frame.Width, frame.Height, (AVPixelFormat)frame.Format,
                    targetFrame.Width, targetFrame.Height, (AVPixelFormat)targetFrame.Format,
                    SWS_BILINEAR, null, null, null);

        if (swsCtx == null)
        {
            throw new FFmpegException("Fail to create sws context.");
        }

        int swsResult = ffmpeg.sws_scale_frame(swsCtx, targetFrame.FramePointer, frame.FramePointer);
        ffmpeg.sws_freeContext(swsCtx);

        if (swsResult < 0)
        {
            throw new FFmpegException(swsResult, "Fail to scale frame");
        }

        var bitmap = MediaHelper.LoadImage(targetFrame.DataPointer(0),
                                           targetFrame.Width,
                                           targetFrame.Height,
                                           targetFrame.LineSize(0));

        return bitmap;
    }

    public static Task<WriteableBitmap?> LoadFrameAsync(string filePath, Option option, CancellationToken token = default)
    {
        return Task.Run(() => LoadFrame(filePath, option, token), token);
    }
}
