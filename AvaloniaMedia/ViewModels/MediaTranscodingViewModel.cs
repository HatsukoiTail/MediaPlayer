using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using AvaloniaMedia.FFmpeg.Transcode;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.DependencyInjection;
using CommunityToolkit.Mvvm.Input;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.ViewModels;

public partial class MediaTranscodingViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial string InputPath { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string OutputPath { get; set; } = string.Empty;

    [ObservableProperty]
    public partial string OutputFormat { get; set; } = "mp4";

    [ObservableProperty]
    public partial string InputInfo { get; set; } = string.Empty;

    // Video

    [ObservableProperty]
    public partial bool ReEncodeVideo { get; set; }


    [ObservableProperty]
    public partial string VideoCodec { get; set; } = "libx264";


    [ObservableProperty]
    public partial int OutputWidth { get; set; } = 1920;


    [ObservableProperty]
    public partial int OutputHeight { get; set; } = 1080;

    [ObservableProperty]
    public partial bool AspectRatioLocked { get; set; } = true;

    private double _originalAspectRatio = 16.0 / 9.0;
    private bool _suppressDimensionSync;


    [ObservableProperty]
    public partial int VideoBitRate { get; set; } = 5000;


    [ObservableProperty]
    public partial double FrameRate { get; set; } = 30;

    [ObservableProperty]
    public partial string PixelFormat { get; set; } = "YUV420P";

    public string[] PixelFormats { get; } =
        ["YUV420P", "YUV422P", "YUV444P", "YUV420P10LE", "YUV420P12LE", "NV12", "P010LE", "RGB24"];

    // Audio

    [ObservableProperty]
    public partial bool ReEncodeAudio { get; set; }


    [ObservableProperty]
    public partial string AudioCodec { get; set; } = "aac";


    [ObservableProperty]
    public partial int SampleRate { get; set; } = 48000;


    [ObservableProperty]
    public partial int AudioBitRate { get; set; } = 192;

    [ObservableProperty]
    public partial string SampleFormat { get; set; } = "S16";

    public string[] SampleFormats { get; } = ["S16", "S32", "FLT", "S16P", "FLTP", "S32P"];

    // Cover

    [ObservableProperty]
    public partial string? CoverImagePath { get; set; }

    // Progress

    [ObservableProperty]
    public partial double Progress { get; set; }


    [ObservableProperty]
    public partial string StatusText { get; set; } = "";


    [ObservableProperty]
    public partial bool IsTranscoding { get; set; }

    // Format list

    public string[] OutputFormats { get; } = ["mp4", "mkv", "mov", "avi", "webm", "flv", "ts"];
    public string[] VideoCodecs { get; } = ["libx264", "libx265", "libvpx-vp9", "libaom-av1", "mpeg4"];
    public string[] AudioCodecs { get; } = ["aac", "libmp3lame", "libopus", "libvorbis", "ac3", "flac"];

    private Transcoder? _transcoder;
    private CancellationTokenSource? _cancelSource;

    [RelayCommand]
    public async Task SelectInputFile()
    {
        var storage = Ioc.Default.GetRequiredService<IStorageProvider>();
        var result = await storage.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select input media file",
            AllowMultiple = false
        });
        if (result.Count == 0) return;

        InputPath = result[0].Path.LocalPath;
        OutputPath = Path.Combine(
            Path.GetDirectoryName(InputPath)!,
            Path.GetFileNameWithoutExtension(InputPath) + "_out." + OutputFormat);

        InputInfo = ReadInputInfo(InputPath, out int vw, out int vh);
        if (vw > 0 && vh > 0)
        {
            _originalAspectRatio = (double)vw / vh;
            if (!ReEncodeVideo)
            {
                OutputWidth = vw;
                OutputHeight = vh;
            }
        }
    }

    [RelayCommand]
    public async Task SelectOutputFile()
    {
        var storage = Ioc.Default.GetRequiredService<IStorageProvider>();
        var result = await storage.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Select output location",
            DefaultExtension = OutputFormat,
            SuggestedFileName = Path.GetFileName(OutputPath)
        });
        if (result == null) return;
        OutputPath = result.Path.LocalPath;
    }

    [RelayCommand]
    public async Task SelectCoverImage()
    {
        var storage = Ioc.Default.GetRequiredService<IStorageProvider>();
        var result = await storage.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select cover image",
            AllowMultiple = false,
            FileTypeFilter = [new FilePickerFileType("Images") { Patterns = ["*.jpg", "*.jpeg", "*.png", "*.bmp"] }]
        });
        if (result.Count == 0) return;
        CoverImagePath = result[0].Path.LocalPath;
    }

    [RelayCommand]
    public async Task StartTranscode()
    {
        if (string.IsNullOrEmpty(InputPath) || string.IsNullOrEmpty(OutputPath))
        {
            StatusText = "Please select input and output files.";
            return;
        }
        if (IsTranscoding) return;

        IsTranscoding = true;
        Progress = 0;
        StatusText = "Starting...";
        _cancelSource = new CancellationTokenSource();

        try
        {
            var option = new TranscoderOption
            {
                InputPath = InputPath,
                OutputPath = OutputPath,
                OutputFormat = OutputFormat,
                Video = ReEncodeVideo ? new TranscoderVideoConfig
                {
                    Codec = VideoCodec,
                    Width = OutputWidth,
                    Height = OutputHeight,
                    BitRate = VideoBitRate * 1000,
                    FrameRate = new AVRational { num = (int)FrameRate, den = 1 },
                    PixelFormat = ParsePixelFormat(PixelFormat),
                } : null,
                Audio = ReEncodeAudio ? new TranscoderAudioConfig
                {
                    Codec = AudioCodec,
                    SampleRate = SampleRate,
                    BitRate = AudioBitRate * 1000,
                    SampleFormat = ParseSampleFormat(SampleFormat),
                } : null,
                CoverImagePath = CoverImagePath,
            };

            _transcoder = new Transcoder(option);
            _transcoder.ProgressChanged += p =>
            {
                Progress = p * 100;
                StatusText = $"Transcoding... {p:P0}";
            };
            _transcoder.Error += msg =>
            {
                StatusText = $"Error: {msg}";
            };

            StatusText = "Opening...";
            _transcoder.Open(option, _cancelSource.Token);

            StatusText = "Transcoding...";
            await _transcoder.RunAsync(_cancelSource.Token);

            StatusText = "Completed!";
            Progress = 100;
        }
        catch (OperationCanceledException)
        {
            StatusText = "Cancelled.";
        }
        catch (Exception ex)
        {
            StatusText = $"Error: {ex.Message}";
        }
        finally
        {
            IsTranscoding = false;
            _transcoder?.Dispose();
            _transcoder = null;
            _cancelSource?.Dispose();
            _cancelSource = null;
        }
    }

    [RelayCommand]
    public void CancelTranscode()
    {
        _cancelSource?.Cancel();
        StatusText = "Cancelling...";
    }

    partial void OnOutputFormatChanged(string value)
    {
        if (!string.IsNullOrEmpty(InputPath))
        {
            OutputPath = Path.Combine(
                Path.GetDirectoryName(InputPath)!,
                Path.GetFileNameWithoutExtension(InputPath) + "_out." + value);
        }
    }

    partial void OnOutputWidthChanged(int value)
    {
        if (!AspectRatioLocked || _suppressDimensionSync || value <= 0 || _originalAspectRatio <= 0)
            return;

        _suppressDimensionSync = true;
        OutputHeight = Math.Max(1, (int)Math.Round(value / _originalAspectRatio));
        // Snap to even dimensions (most encoders prefer this)
        if (OutputHeight % 2 != 0) OutputHeight++;
        _suppressDimensionSync = false;
    }

    partial void OnOutputHeightChanged(int value)
    {
        if (!AspectRatioLocked || _suppressDimensionSync || value <= 0 || _originalAspectRatio <= 0)
            return;

        _suppressDimensionSync = true;
        OutputWidth = Math.Max(1, (int)Math.Round(value * _originalAspectRatio));
        if (OutputWidth % 2 != 0) OutputWidth++;
        _suppressDimensionSync = false;
    }

    public static unsafe string ReadInputInfo(string path, out int width, out int height)
    {
        width = 0; height = 0;
        try
        {
            var ctx = new FFmpeg.Demux.FormatContext();
            ctx.OpenAsReader(path);
            var fmt = ctx.FormatContextPointer;
            var info = $"{fmt->nb_streams} streams";
            for (int i = 0; i < fmt->nb_streams; i++)
            {
                var s = fmt->streams[i];
                var codec = ffmpeg.avcodec_find_decoder(s->codecpar->codec_id);
                var codecName = codec != null
                    ? Marshal.PtrToStringAnsi((IntPtr)codec->name)
                    : "unknown";
                var typeStr = s->codecpar->codec_type == AVMediaType.AVMEDIA_TYPE_VIDEO
                    ? $"Video: {s->codecpar->width}x{s->codecpar->height}"
                    : s->codecpar->codec_type == AVMediaType.AVMEDIA_TYPE_AUDIO
                        ? $"Audio: {s->codecpar->sample_rate}Hz"
                        : "Other";
                info += $"\n  #{i}: {typeStr}, {codecName}";

                if (s->codecpar->codec_type == AVMediaType.AVMEDIA_TYPE_VIDEO)
                {
                    width = s->codecpar->width;
                    height = s->codecpar->height;
                }
            }
            ctx.Dispose();
            return info;
        }
        catch
        {
            return "(Could not read)";
        }
    }

    private static AVPixelFormat ParsePixelFormat(string name) => name switch
    {
        "YUV420P" => AVPixelFormat.AV_PIX_FMT_YUV420P,
        "YUV422P" => AVPixelFormat.AV_PIX_FMT_YUV422P,
        "YUV444P" => AVPixelFormat.AV_PIX_FMT_YUV444P,
        "YUV420P10LE" => AVPixelFormat.AV_PIX_FMT_YUV420P10LE,
        "YUV420P12LE" => AVPixelFormat.AV_PIX_FMT_YUV420P12LE,
        "NV12" => AVPixelFormat.AV_PIX_FMT_NV12,
        "P010LE" => AVPixelFormat.AV_PIX_FMT_P010LE,
        "RGB24" => AVPixelFormat.AV_PIX_FMT_RGB24,
        _ => AVPixelFormat.AV_PIX_FMT_YUV420P,
    };

    private static AVSampleFormat ParseSampleFormat(string name) => name switch
    {
        "S16" => AVSampleFormat.AV_SAMPLE_FMT_S16,
        "S32" => AVSampleFormat.AV_SAMPLE_FMT_S32,
        "FLT" => AVSampleFormat.AV_SAMPLE_FMT_FLT,
        "S16P" => AVSampleFormat.AV_SAMPLE_FMT_S16P,
        "FLTP" => AVSampleFormat.AV_SAMPLE_FMT_FLTP,
        "S32P" => AVSampleFormat.AV_SAMPLE_FMT_S32P,
        _ => AVSampleFormat.AV_SAMPLE_FMT_S16,
    };
}
