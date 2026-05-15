
using System;
using System.Threading;

using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Model;

public sealed unsafe class Packet : IDisposable
{
    public AVPacket* PacketPointer { get; private set; } = null;

    public bool IsNull => PacketPointer == null;
    public int StreamIndex => PacketPointer->stream_index;
    public int Size => PacketPointer->size;
    public int Serial { get; set; } = -1;
    public double Duration { get; set; } = double.NaN;

    public Packet() {}

    public Packet(AVPacket* packet) => PacketPointer = packet;

    public static Packet CreatePacket()
    {
        var packet = ffmpeg.av_packet_alloc();
        if (packet == null)
            throw new FFmpegException("Failed to allocate packet");

        return new Packet(packet);
    }

    public static Packet MovePacket(Packet other)
    {
        var packetPtr = ffmpeg.av_packet_alloc();
        if (packetPtr == null)
        {
            throw new FFmpegException("Failed to allocate packet");
        }
        var packet = new Packet(packetPtr);
        ffmpeg.av_packet_move_ref(packetPtr, other.PacketPointer);
        return packet;
    }

    /// <summary>
    /// 减少Packet中堆数据的引用计数，该函数调用并不会释放AVPacket结构体本身
    /// </summary>
    public void Unref() => ffmpeg.av_packet_unref(PacketPointer);

    public void Release()
    {
        if (PacketPointer != null)
        {
            var ptr = PacketPointer;
            ffmpeg.av_packet_free(&ptr);
            PacketPointer = null;
        }
    }

    private bool disposed = false;

    ~Packet() => Dispose();

    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, true) == true)
            return;

        Release();

        GC.SuppressFinalize(this);
    }
}