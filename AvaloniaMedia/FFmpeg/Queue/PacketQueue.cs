

using System;
using System.Collections.Generic;
using System.Threading;
using AvaloniaMedia.FFmpeg.Model;

namespace AvaloniaMedia.FFmpeg.Queue;

public class PacketQueue : IDisposable
{
    private readonly Queue<Packet> queue = [];
    private readonly object locker = new();

    public int Count => queue.Count;
    public bool IsEmpty => queue.Count == 0;
    public bool IsAborted { get; private set; } = true;

    public int Serial { get; private set; } = -1;
    public int Size { get; private set; }
    public double Duration { get; private set; } = 0;

    public event Action<PacketQueue>? Awakened;

    // 调用该函数后，调用Wait()函数的线程将被唤醒
    public void Awake()
    {
        Awakened?.Invoke(this);
    }

    public void Init()
    {
        lock (locker)
        {
            IsAborted = false;
            Serial = -1;
        }
    }

    public void Clear()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (locker)
        {
            while (queue.Count > 0)
            {
                var packet = queue.Dequeue();
                packet.Dispose();
            }
            Serial = -1;
            Size = 0;
            Duration = 0;
        }
    }

    public void Enqueue(Packet packet)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (locker)
        {
            queue.Enqueue(packet);
            packet.Serial = Serial;
            Size += packet.Size;
            Duration += packet.Duration;
            Monitor.Pulse(locker);
        }
    }

    // 阻塞式获取队列中的第一个元素
    public Packet? Dequeue()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (locker)
        {
            while (queue.Count == 0 && !IsAborted)
            {
                Monitor.Wait(locker);
            }
            if (IsAborted)
            {
                return null;
            }
            var packet = queue.Dequeue();
            Serial = packet.Serial;
            Size -= packet.Size;
            Duration -= packet.Duration;
            return packet;
        }
    }

    public void Start()
    {
        lock (locker)
        {
            IsAborted = false;
            Serial++;
        }
    }

    public void Flush()
    {
        lock (locker)
        {
            queue.Clear();
            Serial++;
            Size = 0;
            Duration = 0;
        }
    }

    public void Abort()
    {
        lock (locker)
        {
            IsAborted = true;
            Monitor.Pulse(locker);
        }
    }


    private bool disposed = false;

    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, true) == true)
            return;
        
        lock (locker)
        {
            IsAborted = true;
            Monitor.PulseAll(locker);
            while (queue.Count > 0)
            {
                var packet = queue.Dequeue();
                packet.Dispose();
            }
            Serial = -1;
            Duration = 0;
            Size = 0;
        }

        GC.SuppressFinalize(this);
    }
}