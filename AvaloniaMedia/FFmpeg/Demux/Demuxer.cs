using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using AvaloniaMedia.FFmpeg.Model;
using AvaloniaMedia.FFmpeg.Queue;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Demux;


public class Demuxer : IDisposable
{
    public FormatContext FormatContext { get; private set; } = new();

    private Thread? demuxThread;
    private CancellationTokenSource? threadCancelTokenSource; // 用于取消线程
    private readonly AutoResetEvent demuxResetEvent = new(false); // 用于通知线程结束
    public event Action? DemuxEof;

    public PacketQueue VideoPacketQueue { get; } = new();
    public PacketQueue AudioPacketQueue { get; } = new();
    public PacketQueue SubtitlePacketQueue { get; } = new();

    public bool HasVideo => VideoStreamIndex >= 0;
    public bool HasAudio => AudioStreamIndex >= 0;
    public bool HasSubtitle => SubtitleStreamIndex >= 0;

    public int VideoStreamIndex { get; private set; } = -1;
    public int AudioStreamIndex { get; private set; } = -1;
    public int SubtitleStreamIndex { get; private set; } = -1;

    #region Demuxer State
    public bool IsOpen { get; private set; } = false;       // 是否打开解复用器
    public bool IsRunning => demuxThread != null && demuxThread.IsAlive;    // 解复用线程是否正在运行
    public bool IsEof { get; private set; } = false;   // 是否读取到文件末尾
    public int Serial => VideoPacketQueue.Serial > 0 ? VideoPacketQueue.Serial : AudioPacketQueue.Serial;
    #endregion

    #region Seek Field
    private TaskCompletionSource<bool>? seekTaskCompletionSource;   // 用于等待seek操作完成
    private volatile bool hasSeekRequest = false;
    private double seekPosition = 0;
    public event Action<double>? SeekDone;
    #endregion

    public Demuxer()
    {
        VideoPacketQueue.Awakened += OnQueueAwakened;
        AudioPacketQueue.Awakened += OnQueueAwakened;
        SubtitlePacketQueue.Awakened += OnQueueAwakened;
    }

    private void OnQueueAwakened(PacketQueue queue)
    {
        demuxResetEvent.Set();
    }

    public unsafe AVStream* GetStream(MediaStreamType type)
    {
        return type switch
        {
            MediaStreamType.Video => FormatContext.GetStream(VideoStreamIndex),
            MediaStreamType.Audio => FormatContext.GetStream(AudioStreamIndex),
            MediaStreamType.Subtitle => FormatContext.GetStream(SubtitleStreamIndex),
            _ => throw new ArgumentException("Invalid stream type", nameof(type)),
        };
    }

    public unsafe AVStream* GetStream(int index) => FormatContext.GetStream(index);

    #region Open & Close
    public Task OpenAsync(string filename) => Task.Run(() => Open(filename));

    public Task OpenAsync(Stream stream) => Task.Run(() => Open(stream));

    public void Open(string filename)
    {
        Debug.Assert(IsOpen == false);

        FormatContext.OpenAsReader(filename);

        FindStreamInfo();

        InitQueue();

        IsOpen = true;
    }

    public void Open(Stream stream)
    {
        Debug.Assert(IsOpen == false);

        FormatContext.OpenAsReader(stream);

        FindStreamInfo();

        InitQueue();

        IsOpen = true;
    }

    public void Close()
    {
        if (IsOpen == false)
            return;

        Stop();
        FormatContext.Close();

        VideoPacketQueue.Clear();
        AudioPacketQueue.Clear();
        SubtitlePacketQueue.Clear();

        hasSeekRequest = false;
        seekPosition = 0;
        seekTaskCompletionSource = null;

        VideoStreamIndex = -1;
        AudioStreamIndex = -1;
        SubtitleStreamIndex = -1;

        DemuxEof = null; // 注销注册的事件

        IsOpen = false;
    }

    public Task CloseAsync() => Task.Run(Close);

    #endregion

    public void Stop()
    {
        // 停止队列
        VideoPacketQueue.Abort();
        AudioPacketQueue.Abort();
        SubtitlePacketQueue.Abort();

        // 停止线程
        threadCancelTokenSource?.Cancel();
        demuxResetEvent.Set();
        demuxThread?.Join();
        demuxThread = null;
        threadCancelTokenSource = null;

        Console.WriteLine("Demuxer: Stop");
    }

    public Task StopAsync() => Task.Run(Stop);

    public void Start()
    {
        Debug.Assert(IsOpen = true);
        Debug.Assert(IsRunning == false);
        StartQueue();
        threadCancelTokenSource ??= new CancellationTokenSource();
        demuxThread = new Thread(() => Demux(threadCancelTokenSource.Token));
        demuxThread.Start();
    }

    public void Seek(double position)
    {
        Debug.Assert(IsRunning == true);

        if (Interlocked.Exchange(ref hasSeekRequest, true) == true)
            return;

        seekPosition = position;

        demuxResetEvent.Set();
    }

    public async Task SeekAsync(double position)
    {
        Debug.Assert(IsRunning == true);

        if (Interlocked.Exchange(ref hasSeekRequest, true) == true)
            return;

        seekTaskCompletionSource ??= new TaskCompletionSource<bool>();
        seekPosition = position;

        demuxResetEvent.Set();

        await seekTaskCompletionSource.Task;

        seekTaskCompletionSource = null;
    }

    public void AwakeDemuxThread()
    {
        demuxResetEvent.Set();
    }

    private unsafe void Demux(CancellationToken token)
    {
        int result = 0;
        using var packet = Packet.CreatePacket();

        bool needGetCover = true;

        while (token.IsCancellationRequested == false)
        {
            if (hasSeekRequest)
            {
                DoSeek(seekPosition);

                IsEof = false;
                needGetCover = true;
                seekTaskCompletionSource?.TrySetResult(true);

                SeekDone?.Invoke(seekPosition);

                hasSeekRequest = false;
            }

            // 获取封面
            if (needGetCover)
            {
                var cover = GetCoverPacket();
                if (cover != null)
                {
                    VideoPacketQueue.Enqueue(cover);
                    VideoPacketQueue.Enqueue(Packet.CreatePacket());
                }
                needGetCover = false;
            }

            const int MAX_QUEUE_SIZE = 15 * 1024 * 1024; // 15MB
            if (GetTotalPacketQueueSize() >= MAX_QUEUE_SIZE || HasEnoughPackets() == true)
            {
                demuxResetEvent.WaitOne();
                continue;
            }

            // 读取Packet并入队
            result = ffmpeg.av_read_frame(FormatContext.FormatContextPointer, packet.PacketPointer);
            if (result < 0)
            {
                if ((result == ffmpeg.AVERROR_EOF || ffmpeg.avio_feof(FormatContext.FormatContextPointer->pb) != 0) && !IsEof)
                {
                    if (VideoStreamIndex >= 0)
                        VideoPacketQueue.Enqueue(Packet.CreatePacket());
                    if (AudioStreamIndex >= 0)
                        AudioPacketQueue.Enqueue(Packet.CreatePacket());
                    if (SubtitleStreamIndex >= 0)
                        SubtitlePacketQueue.Enqueue(Packet.CreatePacket());
                    IsEof = true;
                    DemuxEof?.Invoke();
                }
                if (FormatContext.FormatContextPointer->pb != null && FormatContext.FormatContextPointer->pb->error != 0)
                {
                    throw new FFmpegException(FormatContext.FormatContextPointer->pb->error, "Error occurred while reading frame");
                }
                // 当解码完成时，会在此处阻塞等待
                demuxResetEvent.WaitOne();
                continue;
            }
            else
            {
                IsEof = false;
            }

            if (packet.StreamIndex == VideoStreamIndex)
                EnqueuePacket(GetStream(VideoStreamIndex), VideoPacketQueue, packet);
            else if (packet.StreamIndex == AudioStreamIndex)
                EnqueuePacket(GetStream(AudioStreamIndex), AudioPacketQueue, packet);
            else if (packet.StreamIndex == SubtitleStreamIndex)
                EnqueuePacket(GetStream(SubtitleStreamIndex), SubtitlePacketQueue, packet);
            else
                packet.Unref();
        }
    }

    private static unsafe void EnqueuePacket(AVStream* stream, PacketQueue queue, Packet packet)
    {
        var received = Packet.MovePacket(packet);
        received.Duration = ffmpeg.av_q2d(stream->time_base) * received.PacketPointer->duration;
        queue.Enqueue(received);
    }

    private unsafe Packet? GetCoverPacket()
    {
        if (VideoStreamIndex >= 0 && (FormatContext.GetStream(VideoStreamIndex)->disposition & ffmpeg.AV_DISPOSITION_ATTACHED_PIC) != 0)
        {
            var packet = Packet.CreatePacket();
            int result = ffmpeg.av_packet_ref(packet.PacketPointer, &FormatContext.GetStream(VideoStreamIndex)->attached_pic);
            if (result < 0)
            {
                throw new FFmpegException(result, "Failed to get cover.");
            }
            return packet;
        }
        return null;
    }

    private int GetTotalPacketQueueSize()
    {
        return VideoPacketQueue.Count + AudioPacketQueue.Count + SubtitlePacketQueue.Count;
    }

    private unsafe bool HasEnoughPackets()
    {
        if (VideoStreamIndex >= 0 && !HasEnoughPackets(VideoPacketQueue, FormatContext.GetStream(VideoStreamIndex)))
            return false;
        if (AudioStreamIndex >= 0 && !HasEnoughPackets(AudioPacketQueue, FormatContext.GetStream(AudioStreamIndex)))
            return false;
        if (SubtitleStreamIndex >= 0 && !HasEnoughPackets(SubtitlePacketQueue, FormatContext.GetStream(SubtitleStreamIndex)))
            return false;
        return true;
    }

    private unsafe bool HasEnoughPackets(PacketQueue queue, AVStream* stream)
    {
        const int MAX_PACKETS = 25;

        if (queue.IsAborted)
            return true;
        if ((stream->disposition & ffmpeg.AV_DISPOSITION_ATTACHED_PIC) != 0)
            return true;
        if (queue.Count >= MAX_PACKETS && queue.Duration >= 1.0)
            return true;

        return false;
    }

    private unsafe void DoSeek(double position)
    {
        long pts = (long)(position * ffmpeg.AV_TIME_BASE);
        int result = ffmpeg.avformat_seek_file(FormatContext.FormatContextPointer, -1, long.MinValue, pts, long.MaxValue, 0);
        if (result < 0)
        {
            throw new FFmpegException(result, "Failed to seek");
        }

        if (VideoStreamIndex >= 0)
            VideoPacketQueue.Flush();
        if (AudioStreamIndex >= 0)
            AudioPacketQueue.Flush();
        if (SubtitleStreamIndex >= 0)
            SubtitlePacketQueue.Flush();
    }

    private unsafe void FindStreamInfo()
    {
        Debug.Assert(FormatContext.IsOpen);
        VideoStreamIndex = ffmpeg.av_find_best_stream(FormatContext.FormatContextPointer, AVMediaType.AVMEDIA_TYPE_VIDEO, -1, -1, null, 0);
        AudioStreamIndex = ffmpeg.av_find_best_stream(FormatContext.FormatContextPointer, AVMediaType.AVMEDIA_TYPE_AUDIO, -1, -1, null, 0);
        SubtitleStreamIndex = ffmpeg.av_find_best_stream(FormatContext.FormatContextPointer, AVMediaType.AVMEDIA_TYPE_SUBTITLE, -1, -1, null, 0);
    }

    private void InitQueue()
    {
        if (HasVideo)
        {
            VideoPacketQueue.Init();
        }
        if (HasAudio)
        {
            AudioPacketQueue.Init();
        }
        if (HasSubtitle)
        {
            SubtitlePacketQueue.Init();
        }
    }

    private void StartQueue()
    {
        if (HasAudio)
            AudioPacketQueue.Start();
        if (HasVideo)
            VideoPacketQueue.Start();
        if (HasSubtitle)
            SubtitlePacketQueue.Start();
    }

    private bool disposed = false;
    public void Dispose()
    {
        if (disposed == true)
            return;

        Close();
        FormatContext.Dispose();

        VideoPacketQueue.Dispose();
        AudioPacketQueue.Dispose();
        SubtitlePacketQueue.Dispose();

        GC.SuppressFinalize(this);

        disposed = true;
    }
}