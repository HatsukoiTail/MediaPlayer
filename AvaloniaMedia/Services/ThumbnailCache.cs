using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using Avalonia.Threading;

namespace AvaloniaMedia.Services;

public class ThumbnailCache : IDisposable
{
    private readonly record struct CacheEntry(
        Bitmap Bitmap,
        int RefCount,
        DateTime LastAccess);

    private readonly Dictionary<string, CacheEntry> _cache = [];
    private readonly Lock _lock = new();
    private readonly int _maxSize;
    private readonly TimeSpan _ttl;
    private readonly Timer _evictTimer;
    private bool _disposed;

    public ThumbnailCache(int maxSize = 200, int ttlSeconds = 30)
    {
        _maxSize = maxSize;
        _ttl = TimeSpan.FromSeconds(ttlSeconds);
        _evictTimer = new Timer(_ => Evict(), null, TimeSpan.FromSeconds(10), TimeSpan.FromSeconds(10));
    }

    public Bitmap? Acquire(string path)
    {
        lock (_lock)
        {
            if (_cache.TryGetValue(path, out var entry))
            {
                _cache[path] = new CacheEntry(entry.Bitmap, entry.RefCount + 1, DateTime.UtcNow);
                return entry.Bitmap;
            }
        }
        return null;
    }

    public void Put(string path, Bitmap bitmap)
    {
        lock (_lock)
        {
            if (_cache.Count >= _maxSize)
                EvictOne();

            // Remove old entry if exists (shouldn't normally happen)
            if (_cache.TryGetValue(path, out var old))
                old.Bitmap.Dispose();

            _cache[path] = new CacheEntry(bitmap, 1, DateTime.UtcNow);
        }
    }

    public void Release(string path)
    {
        lock (_lock)
        {
            if (_cache.TryGetValue(path, out var entry))
                _cache[path] = new CacheEntry(entry.Bitmap, entry.RefCount - 1, DateTime.UtcNow);
        }
    }

    private void EvictOne()
    {
        // Remove the entry with refCount=0 and oldest LastAccess
        string? toRemove = null;
        DateTime oldest = DateTime.MaxValue;
        foreach (var (path, entry) in _cache)
        {
            if (entry.RefCount <= 0 && entry.LastAccess < oldest)
            {
                oldest = entry.LastAccess;
                toRemove = path;
            }
        }

        if (toRemove is not null)
        {
            var entry = _cache[toRemove];
            _cache.Remove(toRemove);
            entry.Bitmap.Dispose();
        }
    }

    private void Evict()
    {
        lock (_lock)
        {
            var now = DateTime.UtcNow;
            foreach (var (path, entry) in _cache.ToList())
            {
                if (entry.RefCount <= 0 && now - entry.LastAccess > _ttl)
                {
                    _cache.Remove(path);
                    entry.Bitmap.Dispose();
                }
            }
        }
    }

    public void Clear()
    {
        lock (_lock)
        {
            foreach (var entry in _cache.Values)
                entry.Bitmap.Dispose();
            _cache.Clear();
        }
    }

    public int Count
    {
        get { lock (_lock) return _cache.Count; }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _evictTimer.Dispose();
        Clear();
    }
}
