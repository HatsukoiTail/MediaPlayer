#pragma once

#ifndef FFMPEG_FILTERGRAPH_H
#define FFMPEG_FILTERGRAPH_H

#include <memory>

extern "C"
{
#include <libavfilter/avfilter.h>
}

struct AVFilterGraphDeleter
{
    void operator()(AVFilterGraph* graph) noexcept
    {
        avfilter_graph_free(&graph);
    }
};

using AVFilterGraphPtr = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;

#endif // FFMPEG_FILTERGRAPH_H