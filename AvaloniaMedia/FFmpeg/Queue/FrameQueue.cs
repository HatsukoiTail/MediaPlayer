

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using AvaloniaMedia.FFmpeg.Model;

namespace AvaloniaMedia.FFmpeg.Queue;


public class FrameQueue : IDisposable
{
    private readonly Queue<Frame> queue = new();
    private readonly object lockObject = new();

    public int Count => queue.Count;
    public bool IsEmpty => queue.Count == 0;
    public int MaxCount { get; set; } = 15;
    public bool IsAborted { get; private set; } = true;

    public void Start()
    {
        lock (lockObject)
        {
            IsAborted = false;
        }
    }

    public void Clear()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (lockObject)
        {
            while (queue.Count > 0)
            {
                var frame = queue.Dequeue();
                frame.Dispose();
            }
        }
    }

    public void Abort()
    {
        lock (lockObject)
        {
            IsAborted = true;
            Monitor.PulseAll(lockObject);
        }
    }

    public bool Enqueue(Frame frame)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (lockObject)
        {
            while (queue.Count >= MaxCount && !IsAborted)
            {
                Monitor.Wait(lockObject);
            }
            if (IsAborted)
            {
                return false;
            }
            var enqueuedFrame = Frame.MoveFrame(frame);
            queue.Enqueue(enqueuedFrame);
            Monitor.Pulse(lockObject);
            return true;
        }
    }

    public Frame? Dequeue()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (lockObject)
        {
            while (queue.Count == 0 && !IsAborted)
            {
                Monitor.Wait(lockObject);
            }
            if (IsAborted)
            {
                return null;
            }
            var frame = queue.Dequeue();
            Monitor.Pulse(lockObject);
            return frame;
        }
    }

    public Frame? Peek()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (lockObject)
        {
            if (queue.Count == 0)
            {
                return null;
            }
            return queue.Peek();
        }
    }

    public Frame? PeekNext()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        lock (lockObject)
        {
            if (queue.Count < 2)
                return null;

            return queue.ElementAt(1);
        }
    }

    public bool TryDequeue(out Frame? frame)
    {
        lock (lockObject)
        {
            if (queue.Count == 0 || IsAborted)
            {
                frame = null;
                return false;
            }
            frame = queue.Dequeue();
            Monitor.Pulse(lockObject);
            return true;
        }
    }


    private bool disposed = false;

    /// <summary>
    /// 释放资源，非线程安全
    /// </summary>
    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, true) == true)
            return;

        // Abort and Clear
        lock (lockObject)
        {
            IsAborted = true;
            Monitor.PulseAll(lockObject);

            while (queue.Count > 0)
            {
                var frame = queue.Dequeue();
                frame.Dispose();
            }
        }

        GC.SuppressFinalize(this);
    }

}