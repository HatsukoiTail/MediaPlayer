


using System;
using AvaloniaMedia.FFmpeg.Demux;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.Models;

public unsafe class CodecContext : IDisposable
{
    public AVCodecContext* CodecContextPointer { get; private set; }

    public void Open(AVStream* stream, IntPtr hwPtr)
    {
        AVCodec* codec = ffmpeg.avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec == null)
        {
            throw new InvalidOperationException("Codec not found.");
        }
        var ctx = ffmpeg.avcodec_alloc_context3(codec);
        if (ctx == null)
        {
            throw new InvalidOperationException("Failed to allocate codec context.");
        }
        int result = ffmpeg.avcodec_parameters_to_context(ctx, stream->codecpar);
        if (result < 0)
        {
            throw new InvalidOperationException("Failed to copy codec parameters to context.");
        }
        OpenHardwareDevice(codec, ctx, hwPtr);
        result = ffmpeg.avcodec_open2(ctx, codec, null);
        if (result < 0)
        {
            throw new InvalidOperationException("Failed to open codec.");
        }
        CodecContextPointer = ctx;
    }

    private static bool OpenHardwareDevice(AVCodec* codec, AVCodecContext* codecContext, IntPtr hwPtr)
    {
        AVHWDeviceType hardwareDeviceType = AVHWDeviceType.AV_HWDEVICE_TYPE_NONE;
        const int AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX = 1;

        // 定义一个变量用于存储“备选”方案（如果找不到 D3D11VA，就用第一个找到的其他硬件类型）
        AVHWDeviceType fallbackDeviceType = AVHWDeviceType.AV_HWDEVICE_TYPE_NONE;

        // 1. 遍历编解码器支持的所有硬件配置
        for (int i = 0; ; i++)
        {
            AVCodecHWConfig* config = ffmpeg.avcodec_get_hw_config(codec, i);

            // 2. 如果返回 null，说明遍历结束
            if (config == null)
            {
                break;
            }

            // 3. 检查是否支持 hw_device_ctx 方法
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0)
            {
                // 4. 核心修改：优先寻找 D3D11VA
                if (config->device_type == AVHWDeviceType.AV_HWDEVICE_TYPE_D3D11VA)
                {
                    hardwareDeviceType = config->device_type;
                    // 找到了最优解，直接跳出循环
                    break;
                }

                // 5. 如果当前不是 D3D11VA，但还没找到备选方案，则记录下来作为备选
                if (fallbackDeviceType == AVHWDeviceType.AV_HWDEVICE_TYPE_NONE)
                {
                    fallbackDeviceType = config->device_type;
                }
            }
        }

        // 6. 如果没找到 D3D11VA，但有其他硬件方案（如 DXVA2, CUDA 等），则使用备选方案
        if (hardwareDeviceType == AVHWDeviceType.AV_HWDEVICE_TYPE_NONE && fallbackDeviceType != AVHWDeviceType.AV_HWDEVICE_TYPE_NONE)
        {
            hardwareDeviceType = fallbackDeviceType;
        }

        // 7. 最终检查：如果依然没有硬件设备，则报错
        if (hardwareDeviceType == AVHWDeviceType.AV_HWDEVICE_TYPE_NONE)
        {
            Console.WriteLine("No hardware device found for codec {0}.", codec->id);
            return false;
        }

        // 替换掉原来的 av_hwdevice_ctx_create，改为手动构建
        AVBufferRef* hwDeviceContextBuffer = ffmpeg.av_hwdevice_ctx_alloc(
            AVHWDeviceType.AV_HWDEVICE_TYPE_D3D11VA);
        if (hwDeviceContextBuffer == null)
        {
            Console.WriteLine("Failed to alloc hardware device context.");
            return false;
        }

        // 取出内部的D3D11VA设备上下文
        var hwDeviceCtx = (AVHWDeviceContext*)hwDeviceContextBuffer->data;
        var d3d11vaDeviceCtx = (AVD3D11VADeviceContext*)hwDeviceCtx->hwctx;

        // 写入之前
        Console.WriteLine($"写入前 device ptr: {(IntPtr)d3d11vaDeviceCtx->device}");

        // 把ANGLE的D3D11Device指针注入进去
        // 注意：FFmpeg会对这个指针AddRef，所以我们传原始指针即可
        d3d11vaDeviceCtx->device = (ID3D11Device*)hwPtr;

        // 写入之后
        Console.WriteLine($"写入后 device ptr: {(IntPtr)d3d11vaDeviceCtx->device}");
        Console.WriteLine($"externalD3D11DevicePtr: {hwPtr}");

        // 初始化设备上下文
        int result = ffmpeg.av_hwdevice_ctx_init(hwDeviceContextBuffer);
        if (result < 0)
        {
            Console.WriteLine($"Failed to init hardware device context: {result}");
            ffmpeg.av_buffer_unref(&hwDeviceContextBuffer);
            return false;
        }

        codecContext->hw_device_ctx = ffmpeg.av_buffer_ref(hwDeviceContextBuffer);
        ffmpeg.av_buffer_unref(&hwDeviceContextBuffer);

        Console.WriteLine("Hardware device context created with external D3D11 device.");
        return true;
    }

    public void Dispose()
    {
        var ptr = CodecContextPointer;
        ffmpeg.avcodec_free_context(&ptr);
        CodecContextPointer = null;
    }
}

public static class ImageLoader
{
    public static unsafe Frame Load(string path, IntPtr hwPtr)
    {
        var formatContext = new FormatContext();
        formatContext.OpenAsReader(path);
        var codecContext = new CodecContext();
        codecContext.Open(formatContext.GetStream(0), hwPtr);

        var packet = Packet.CreatePacket();
        var frame = Frame.CreateFrame();

        int count = 0;

        while (true)
        {
            int result = ffmpeg.av_read_frame(formatContext.FormatContextPointer, packet.PacketPointer);
            if (packet.StreamIndex != 0)
            {
                packet.Unref();
                continue;
            }
            result = ffmpeg.avcodec_send_packet(codecContext.CodecContextPointer, packet.PacketPointer);
            result = ffmpeg.avcodec_receive_frame(codecContext.CodecContextPointer, frame.FramePointer);
            if (result >= 0 && count > 20)
            {
                Console.WriteLine($"Load a frame: {frame.Width}x{frame.Height}@{(AVPixelFormat)(frame.Format)}");
                return frame;
            }
            ++count;
            frame.Unref();
        }
    }
}