


using System;
using System.Runtime.InteropServices;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Demux;


public static unsafe class InterruptCallback
{
    private static readonly AVIOInterruptCB_callback interruptCallbackDeleagte = Bridge;

    public static AVIOInterruptCB Create(Func<bool> callback)
    {
        ArgumentNullException.ThrowIfNull(callback);
        var handle = GCHandle.Alloc(callback, GCHandleType.Normal);
        var cb = new AVIOInterruptCB
        {
            callback = interruptCallbackDeleagte,
            opaque = GCHandle.ToIntPtr(handle).ToPointer()
        };
        return cb;
    }

    public static void Release(ref AVIOInterruptCB cb)
    {
        if (cb.opaque != null)
        {
            IntPtr ptr = new(cb.opaque);
            GCHandle handle = GCHandle.FromIntPtr(ptr);
            if (handle.IsAllocated)
            {
                handle.Free();
            }
            cb.opaque = null;
            cb = default;
        }
    }

    private static int Bridge(void* opaque)
    {
        if (opaque == null)
            return 0;
        
        IntPtr ptr = new(opaque);
        var handle = GCHandle.FromIntPtr(ptr);
        if (handle.IsAllocated && handle.Target is Func<bool> instance)
        {
            return instance.Invoke() ? 1 : 0;
        }

        return 0;
    }
}