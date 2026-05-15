

using System;
using System.Runtime.InteropServices;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Filter;


public unsafe class AudioFilter : IDisposable
{
    public AVFilterGraph* FilterGraph { get; private set; }
    public AVFilterContext* InputContext { get; private set; }
    public AVFilterContext* OutputContext { get; private set; }

    public int SampleRate { get; private set; }
    public AVSampleFormat SampleFormat { get; private set; }
    public AVChannelLayout* ChannelLayout { get; private set; }
    public int ChannelCount => ChannelLayout->nb_channels;

    public bool IsValid => FilterGraph != null;

    public AudioFilter()
    {
        ChannelLayout = (AVChannelLayout*)ffmpeg.av_malloc((ulong)sizeof(AVChannelLayout));
    }

    public class Options
    {
        public int ThreadCount { get; set; } = 0;
        public string SwrOpts { get; set; } = string.Empty;
        public required AVChannelLayout* ChannelLayout { get; set; }
        public required int SampleRate { get; set; }
        public required AVSampleFormat SampleFormat { get; set; }
        public string FilterArgs { get; set; } = string.Empty;
    }

    public void Build(Options options)
    {
        Release();

        var filterGraph = ffmpeg.avfilter_graph_alloc();
        if (filterGraph == null)
        {
            throw new FFmpegException("Failed to allocate filter graph");
        }

        try
        {
            filterGraph->nb_threads = options.ThreadCount;
            if (!string.IsNullOrEmpty(options.SwrOpts))
            {
                ffmpeg.av_opt_set(filterGraph, "aresample_swr_opts", options.SwrOpts, 0);
            }

            var channelLayoutDescription = GetChannelLayoutDescription(options.ChannelLayout);
            var sampleFormatName = ffmpeg.av_get_sample_fmt_name(options.SampleFormat);
            var srcFilterArgs = string.Format("sample_rate={0}:sample_fmt={1}:time_base={2}/{3}:channel_layout={4}",
                options.SampleRate, sampleFormatName, 1, options.SampleRate, channelLayoutDescription);

            AVFilterContext* srcContext = null;
            int result = ffmpeg.avfilter_graph_create_filter(&srcContext, ffmpeg.avfilter_get_by_name("abuffer"), "in", srcFilterArgs, null, filterGraph);
            if (result < 0)
            {
                throw new FFmpegException("Failed to create input filter context");
            }

            AVFilterContext* sinkContext = ffmpeg.avfilter_graph_alloc_filter(filterGraph, ffmpeg.avfilter_get_by_name("abuffersink"), "out");
            if (sinkContext == null)
            {
                throw new FFmpegException("Failed to create output filter context");
            }

            result = ffmpeg.av_opt_set(sinkContext, "sample_formats", "s16", ffmpeg.AV_OPT_SEARCH_CHILDREN);
            if (result < 0)
            {
                throw new FFmpegException("Failed to set output sample format");
            }

            result = ffmpeg.av_opt_set_array(sinkContext, "channel_layouts", ffmpeg.AV_OPT_SEARCH_CHILDREN, 0, 1, AVOptionType.AV_OPT_TYPE_CHLAYOUT, options.ChannelLayout);
            if (result < 0)
            {
                throw new FFmpegException("Failed to set output channel layout");
            }

            var sampleRate = options.SampleRate;
            result = ffmpeg.av_opt_set_array(sinkContext, "samplerates", ffmpeg.AV_OPT_SEARCH_CHILDREN, 0, 1, AVOptionType.AV_OPT_TYPE_INT, &sampleRate);
            if (result < 0)
            {
                throw new FFmpegException("Failed to set output sample rate");
            }

            result = ffmpeg.avfilter_init_dict(sinkContext, null);
            if (result < 0)
            {
                throw new FFmpegException("Failed to initialize output filter context");
            }

            FilterHelper.BuildFilterGraph(filterGraph, options.FilterArgs, srcContext, sinkContext);

            FilterGraph = filterGraph;
            InputContext = srcContext;
            OutputContext = sinkContext;

            SampleRate = options.SampleRate;
            SampleFormat = options.SampleFormat;
            ffmpeg.av_channel_layout_copy(ChannelLayout, options.ChannelLayout);

            filterGraph = null; // ownership transferred to this instance
        }
        catch
        {
            throw;
        }
        finally
        {
            if (filterGraph == null)
            {
                ffmpeg.avfilter_graph_free(&filterGraph);
            }
        }
    }

    private string GetChannelLayoutDescription(AVChannelLayout* channelLayout)
    {
        byte* channelLayoutDescription = stackalloc byte[64];
        int result = ffmpeg.av_channel_layout_describe(channelLayout, channelLayoutDescription, 64);
        if (result < 0)
        {
            throw new FFmpegException("Failed to describe channel layout");
        }
        return Marshal.PtrToStringAnsi((IntPtr)channelLayoutDescription)!;
    }

    public void Release()
    {
        if (FilterGraph != null)
        {
            var ptr = FilterGraph;
            ffmpeg.avfilter_graph_free(&ptr);
            FilterGraph = null;
            InputContext = null;
            OutputContext = null;
        }
        ffmpeg.av_channel_layout_uninit(ChannelLayout);
    }

    public void Dispose()
    {
        Release();
        ffmpeg.av_free(ChannelLayout);
        ChannelLayout = null;
        GC.SuppressFinalize(this);
    }
}