#ifndef FILTERCONFIGURE_H
#define FILTERCONFIGURE_H

#include <memory>

extern "C"
{
#include <libavfilter/avfilter.h>
}

inline int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph, AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
    int result = 0;
    int filter_count = graph->nb_filters;
 
    if (filtergraph)
    {
        auto inputs = avfilter_inout_alloc();
        auto outputs = avfilter_inout_alloc();
        if (!inputs || !outputs)
        {
            return AVERROR(ENOMEM);
        }
 
        outputs->name = av_strdup("in");
        outputs->filter_ctx = source_ctx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;
 
        inputs->name = av_strdup("out");
        inputs->filter_ctx = sink_ctx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;
 
        result = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, nullptr);
 
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
 
        if (result < 0)
        {
            return result;
        }
    }
    else
    {
        result = avfilter_link(source_ctx, 0, sink_ctx, 0);
        if (result < 0)
            return result;
    }
 
    for (int i = 0; i < graph->nb_filters - filter_count; i++)
        FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + filter_count]);
 
    result = avfilter_graph_config(graph, nullptr);
 
    return result;
}

#endif // FILTERCONFIGURE_H