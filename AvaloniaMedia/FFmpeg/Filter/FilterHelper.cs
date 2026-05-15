

using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Filter;


public static class FilterHelper
{
    /// <summary>
    /// 
    /// </summary>
    /// <param name="graph"></param>
    /// <param name="args"></param>
    /// <param name="sourceContext"></param>
    /// <param name="sinkContext"></param>
    /// <returns></returns>
    /// <exception cref="FFmpegException"></exception>
    public static unsafe void BuildFilterGraph(AVFilterGraph* graph, string args, AVFilterContext* sourceContext, AVFilterContext* sinkContext)
    {
        int result = 0;
        var filterCount = graph->nb_filters;

        if (!string.IsNullOrEmpty(args))
        {
            var inputs = ffmpeg.avfilter_inout_alloc();
            var outputs = ffmpeg.avfilter_inout_alloc();

            if (inputs == null || outputs == null)
            {
                throw new FFmpegException("Failed to allocate filter input/output structs.");
            }

            outputs->name = ffmpeg.av_strdup("in");
            outputs->filter_ctx = sourceContext;
            outputs->pad_idx = 0;
            outputs->next = null;

            inputs->name = ffmpeg.av_strdup("out");
            inputs->filter_ctx = sinkContext;
            inputs->pad_idx = 0;
            inputs->next = null;

            result = ffmpeg.avfilter_graph_parse_ptr(graph, args, &inputs, &outputs, null);

            ffmpeg.avfilter_inout_free(&outputs);
            ffmpeg.avfilter_inout_free(&inputs);

            if (result < 0)
            {
                throw new FFmpegException(result, "Failed to parse filter graph.");
            }
        }
        else
        {
            result = ffmpeg.avfilter_link(sourceContext, 0, sinkContext, 0);
            if (result < 0)
                throw new FFmpegException(result, "Failed to link filter graph.");
        }

        for (int i = 0; i < graph->nb_filters - filterCount; i++)
        {
            var temp = graph->filters[i];
            graph->filters[i] = graph->filters[filterCount + i];
            graph->filters[filterCount + i] = temp;
        }

        result = ffmpeg.avfilter_graph_config(graph, null);
        if (result < 0)
        {
            throw new FFmpegException(result, "Fail to configure filter graph.");
        }
    }
}