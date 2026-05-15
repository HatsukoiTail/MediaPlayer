

using System;
using System.Collections.Generic;
using System.Threading;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Filter;

public sealed unsafe class VideoFilter : IDisposable
{
    public sealed class Options
    {
        public int FilterThreadCount { get; set; } = 4;
        public required HashSet<AVPixelFormat> PixelFormatAllowList { get; set; }
        public required int Width { get; set; }
        public required int Height { get; set; }
        public required AVPixelFormat PixelFormat { get; set; }
        public string SwsAlgorithm { get; set; } = "bicubic";
        public required AVRational TimeBase { get; set; }
        public required AVRational AspectRatio { get; set; }
        public required AVColorSpace ColorSpace { get; set; }
        public required AVColorRange ColorRange { get; set; }
        public required AVRational FrameRate { get; set; }
        public AVBufferRef* HardwareContext { get; set; } = null;
        public bool UseVulkan { get; set; } = false;
        public HashSet<AVColorSpace> ColorSpaceSupportList { get; set; } = [];
        public bool AutoRotate { get; set; } = false;
        public AVFrameSideData* SideData { get; set; }
        public string UserFilter { get; set; } = string.Empty;
    }


    public AVFilterGraph* FilterGraphPointer { get; private set; } = null;
    public AVFilterContext* InputContextPointer { get; private set; } = null;
    public AVFilterContext* OutputContextPointer { get; private set; } = null;

    public int Width { get; private set; }
    public int Height { get; private set; }
    public AVPixelFormat PixelFormat { get; private set; }

    public bool IsValid => FilterGraphPointer != null;

    public void Release()
    {
        if (FilterGraphPointer != null)
        {
            var ptr = FilterGraphPointer;
            ffmpeg.avfilter_graph_free(&ptr);
            FilterGraphPointer = null;
            InputContextPointer = null;
            OutputContextPointer = null;
        }
    }

    public void Build(Options options)
    {
        Release();

        if (options.PixelFormatAllowList.Count == 0)
            throw new ArgumentException("Pixel format allow list cannot be empty.");

        AVFilterGraph* filterGraph = null;
        AVFilterContext* filterSrc = null;
        AVFilterContext* filterOut = null;
        AVBufferSrcParameters* param = null;

        try
        {
            filterGraph = ffmpeg.avfilter_graph_alloc();
            if (filterGraph == null)
                throw new Exception("Failed to allocate filter graph.");

            filterGraph->nb_threads = options.FilterThreadCount;

            // 构建sws滤镜
            string swsOptions = "flags=lanczos";
            filterGraph->scale_sws_opts = ffmpeg.av_strdup(swsOptions);

            param = ffmpeg.av_buffersrc_parameters_alloc();
            if (param == null)
            {
                throw new Exception("Failed to allocate buffer source parameters.");
            }

            param->format = (int)options.PixelFormat;
            param->width = options.Width;
            param->height = options.Height;
            param->time_base = options.TimeBase;
            param->sample_aspect_ratio = options.AspectRatio;
            param->color_space = options.ColorSpace;
            param->color_range = options.ColorRange;
            param->frame_rate = options.FrameRate;
            param->hw_frames_ctx = options.HardwareContext;

            filterSrc = ffmpeg.avfilter_graph_alloc_filter(filterGraph, ffmpeg.avfilter_get_by_name("buffer"), "out");
            if (filterSrc == null)
                throw new FFmpegException("Failed to allocate buffer source filter.");

            int result = ffmpeg.av_buffersrc_parameters_set(filterSrc, param);
            if (result < 0)
                throw new FFmpegException("Failed to set buffer source parameters.");

            result = ffmpeg.avfilter_init_dict(filterSrc, null);
            if (result < 0)
                throw new FFmpegException("Failed to init buffer source filter.");

            filterOut = ffmpeg.avfilter_graph_alloc_filter(filterGraph, ffmpeg.avfilter_get_by_name("buffersink"), "in");
            if (filterOut == null)
                throw new FFmpegException("Failed to allocate buffer sink filter.");

            // 将允许的像素格式转换为C风格数组
            AVPixelFormat* pixelFormats = stackalloc AVPixelFormat[options.PixelFormatAllowList.Count];
            {
                int index = 0;
                foreach (var format in options.PixelFormatAllowList)
                    pixelFormats[index++] = format;
            }
            result = ffmpeg.av_opt_set_array(filterOut, "pixel_formats", ffmpeg.AV_OPT_SEARCH_CHILDREN, 0,
                                            (uint)options.PixelFormatAllowList.Count, AVOptionType.AV_OPT_TYPE_PIXEL_FMT, pixelFormats);
            if (result < 0)
                throw new FFmpegException("Failed to set allowed pixel formats.");

            if (!options.UseVulkan && options.ColorSpaceSupportList.Count > 0)
            {
                var colorSpaces = stackalloc AVColorSpace[options.ColorSpaceSupportList.Count];
                int index = 0;
                foreach (var colorSpace in options.ColorSpaceSupportList)
                    colorSpaces[index++] = colorSpace;
                result = ffmpeg.av_opt_set_array(filterOut, "colorspaces", ffmpeg.AV_OPT_SEARCH_CHILDREN, 0,
                                                (uint)options.ColorSpaceSupportList.Count, AVOptionType.AV_OPT_TYPE_INT, colorSpaces);
            }
            if (result < 0)
                throw new FFmpegException("Failed to set allowed color spaces.");

            result = ffmpeg.avfilter_init_dict(filterOut, null);
            if (result < 0)
                throw new FFmpegException("Failed to init buffer sink filter.");

            AVFilterContext* lastFilter = filterOut;
            if (options.AutoRotate)
            {
                lastFilter = BuildRotateFilter(filterGraph, options.SideData, filterOut);
            }

            FilterHelper.BuildFilterGraph(filterGraph, options.UserFilter, filterSrc, lastFilter);

            FilterGraphPointer = filterGraph;
            InputContextPointer = filterSrc;
            OutputContextPointer = filterOut;

            Width = options.Width;
            Height = options.Height;
            PixelFormat = options.PixelFormat;

            filterGraph = null;
            filterSrc = null;
            filterOut = null;
        }
        catch(FFmpegException ex)
        {
            Console.WriteLine(ex.Message);
            throw;
        }
        finally
        {
            if (param != null)
            {
                ffmpeg.av_free(&param);
            }
            if (filterGraph != null)
            {
                ffmpeg.avfilter_graph_free(&filterGraph);
            }
        }
    }

    private static AVFilterContext* BuildRotateFilter(AVFilterGraph* filterGraph, AVFrameSideData* sideData, AVFilterContext* lastCtx)
    {
        double theta = 0;
        int* displayMatrix = (int*)sideData->data;
        if (displayMatrix != null)
        {  
            int_array9* matrix = (int_array9*)sideData->data;
            theta = -Math.Round(ffmpeg.av_display_rotation_get(in *matrix));
        }
        theta -= 360 * Math.Floor(theta / 360.0 + 0.9 / 360.0);
        if (Math.Abs(theta - 90.0) < 1.0)
        {
            var args = displayMatrix[3] > 0 ? "cclock_flip" : "clock";
            return InsertFilter(filterGraph, "transpose", args, lastCtx);
        }
        else if (Math.Abs(theta - 180.0) < 1.0)
        {
            var lastFilter = lastCtx;
            if (displayMatrix[0] < 0)
                lastFilter = InsertFilter(filterGraph, "hflip", string.Empty, lastFilter);
            if (displayMatrix[4] < 0)
                lastFilter = InsertFilter(filterGraph, "vflip", string.Empty, lastFilter);
            return lastFilter;
        }
        else if (Math.Abs(theta - 270.0) < 1.0)
        {
            var args = displayMatrix[3] > 0 ? "clock_flip" : "cclock";
            return InsertFilter(filterGraph, "transpose", args, lastCtx);
        }
        else if (Math.Abs(theta) > 1.0)
        {
            var args = $"{theta}*PI/180";
            return InsertFilter(filterGraph, "rotate", args, lastCtx);
        }
        else
        {
            if (displayMatrix != null && displayMatrix[4] < 0)
            {
                return InsertFilter(filterGraph, "vflip", string.Empty, lastCtx);
            }
        }
        return lastCtx;
    }

    private static AVFilterContext* InsertFilter(AVFilterGraph* graph, string filterName, string args, AVFilterContext* lastCtx)
    {
        AVFilterContext* filterCtx = null;
        int result = ffmpeg.avfilter_graph_create_filter(&filterCtx, ffmpeg.avfilter_get_by_name(filterName), filterName, args, null, graph);
        if (result < 0)
            throw new Exception("Failed to create filter context.");

        result = ffmpeg.avfilter_link(lastCtx, 0, filterCtx, 0);
        if (result < 0)
            throw new Exception("Failed to link filters.");

        return filterCtx;
    }

 
    private bool disposed = false;
    public void Dispose()
    {
        if (disposed == true)
            return;

        if (FilterGraphPointer != null)
        {
            var ptr = FilterGraphPointer;
            ffmpeg.avfilter_graph_free(&ptr);
            FilterGraphPointer = null;
        }

        GC.SuppressFinalize(this);

        disposed = true;
    }
    ~VideoFilter() => Dispose();
}