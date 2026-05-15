using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using AvaloniaMedia.FFmpeg.Demux;
using AvaloniaMedia.FFmpeg.Model;
using FFmpeg.AutoGen;

namespace AvaloniaMedia.FFmpeg.Transcode;

public class TranscoderOption
{
    public string? InputPath { get; set; }
    public Stream? InputStream { get; set; }
    public string? OutputPath { get; set; }
    public Stream? OutputStream { get; set; }
    public string? OutputFormat { get; set; }

    public int ThreadCount { get; set; }

    public TranscoderVideoConfig? Video { get; set; }
    public TranscoderAudioConfig? Audio { get; set; }

    public string? VideoFilter { get; set; }
    public string? AudioFilter { get; set; }

    public Dictionary<string, string> Metadata { get; set; } = [];
    public string? CoverImagePath { get; set; }
}

public class TranscoderVideoConfig
{
    public string? Codec { get; set; }
    public int? Width { get; set; }
    public int? Height { get; set; }
    public int? BitRate { get; set; }
    public AVPixelFormat? PixelFormat { get; set; }
    public AVRational? FrameRate { get; set; }
    public int? GopSize { get; set; }
    public int? MaxBFrame { get; set; }

    public bool Enabled => Codec != null || Width != null || Height != null
        || BitRate != null || PixelFormat != null || FrameRate != null
        || GopSize != null || MaxBFrame != null;
}

public class TranscoderAudioConfig
{
    public string? Codec { get; set; }
    public int? SampleRate { get; set; }
    public AVSampleFormat? SampleFormat { get; set; }
    public int? BitRate { get; set; }

    public bool Enabled => Codec != null || SampleRate != null
        || SampleFormat != null || BitRate != null;
}

public unsafe class Transcoder : IDisposable
{
    private readonly TranscoderOption _option;
    private FormatContext? _inputCtx;
    private AVFormatContext* _outputCtx;
    private readonly List<TranscodeStream> _streams = new();
    private GCHandle? _writeIOHandle;
    private bool _ownsOutputIO;
    private bool _headerWritten;
    private CancellationTokenSource? _cancelSource;

    public event Action<double>? ProgressChanged;
    public event Action? Completed;
    public event Action<string>? Error;

    public Transcoder(TranscoderOption option)
    {
        _option = option;
    }

    public void Open(TranscoderOption option, CancellationToken token)
    {
        // --- Open input ---
        _inputCtx = new FormatContext();
        if (_option.InputPath != null)
            _inputCtx.OpenAsReader(_option.InputPath);
        else if (_option.InputStream != null)
            _inputCtx.OpenAsReader(_option.InputStream);
        else
            throw new InvalidOperationException("No input specified.");

        var ifmt = _inputCtx.FormatContextPointer;

        // --- Create output context ---
        AVFormatContext* ofmt = null;
        string? outputName = null;
        if (_option.OutputPath != null)
            outputName = _option.OutputPath;
        else if (_option.OutputStream != null)
            outputName = null;

        ffmpeg.avformat_alloc_output_context2(&ofmt, null, _option.OutputFormat, outputName);

        if (ofmt == null)
            throw new FFmpegException("Failed to create output format context.");

        _outputCtx = ofmt;

        // --- Map streams ---
        int videoEncodeIdx = -1, audioEncodeIdx = -1;

        for (int i = 0; i < ifmt->nb_streams; i++)
        {
            var inStream = ifmt->streams[i];
            var codecType = inStream->codecpar->codec_type;

            if (codecType == AVMediaType.AVMEDIA_TYPE_VIDEO
                && videoEncodeIdx < 0
                && (_option.Video?.Enabled ?? false))
            {
                videoEncodeIdx = i;
            }
            else if (codecType == AVMediaType.AVMEDIA_TYPE_AUDIO
                && audioEncodeIdx < 0
                && (_option.Audio?.Enabled ?? false))
            {
                audioEncodeIdx = i;
            }
        }

        for (int i = 0; i < ifmt->nb_streams; i++)
        {
            var inStream = ifmt->streams[i];
            var codecType = inStream->codecpar->codec_type;
            bool reEncode = (i == videoEncodeIdx && _option.Video?.Enabled == true)
                         || (i == audioEncodeIdx && _option.Audio?.Enabled == true);

            if (codecType == AVMediaType.AVMEDIA_TYPE_VIDEO
                || codecType == AVMediaType.AVMEDIA_TYPE_AUDIO)
            {
                if (reEncode)
                {
                    AddReEncodeStream(i, codecType);
                }
                else
                {
                    AddRemuxStream(i);
                }
            }
            else
            {
                AddRemuxStream(i);
            }
        }

        // --- Attach cover image ---
        if (_option.CoverImagePath != null)
        {
            var fileData = File.ReadAllBytes(_option.CoverImagePath!);
            var buffer = (byte*)ffmpeg.av_malloc((ulong)fileData.Length);
            fixed (byte* src = fileData)
                Buffer.MemoryCopy(src, buffer, fileData.Length, fileData.Length);
            SetCoverImage(_outputCtx, buffer, fileData.Length);
            ffmpeg.av_free(buffer);
        }

        // --- Metadata ---
        SetMetaData(ofmt->metadata, _option.Metadata);

        // --- Open output ---
        if (_option.OutputStream != null)
        {
            OpenOutputStream(_option.OutputStream);
        }
        else if (_option.OutputPath != null)
        {
            if ((_outputCtx->oformat->flags & ffmpeg.AVFMT_NOFILE) == 0)
            {
                int ret = ffmpeg.avio_open(&_outputCtx->pb, _option.OutputPath, ffmpeg.AVIO_FLAG_WRITE);
                if (ret < 0)
                    throw new FFmpegException(ret, "Failed to open output file.");
            }
        }

        // --- Write header ---
        int hdrRet = ffmpeg.avformat_write_header(_outputCtx, null);
        if (hdrRet < 0)
            throw new FFmpegException(hdrRet, "Failed to write header.");
        _headerWritten = true;
    }

    private static void SetMetaData(AVDictionary* target, Dictionary<string, string> data)
    {
        foreach (var entry in data)
        {
            int result = ffmpeg.av_dict_set(&target, entry.Key, entry.Value, 0);
            if (result < 0)
            {
                throw new FFmpegException(result, $"Fail to set metadata: {entry.Key} : {entry.Value}");
            }
        }
    }

    // imageData为打包格式的原始数据（如JPG）
    private static void SetCoverImage(AVFormatContext* formatCtx, byte* imageData, int size)
    {
        AVStream* coverStream = null;
        AVCodecContext* codecCtx = null;
        AVFrame* frame = null;
        AVPacket* packet = null;
        try
        {
            coverStream = ffmpeg.avformat_new_stream(formatCtx, null);
            if (coverStream == null)
            {
                throw new FFmpegException("Fail to create cover stream");
            }
            const AVCodecID CodecID = AVCodecID.AV_CODEC_ID_PNG;
            AVCodec* codec = ffmpeg.avcodec_find_encoder(CodecID);
            if (codec == null)
            {
                throw new FFmpegException($"Cannot find {CodecID}");
            }
            codecCtx = ffmpeg.avcodec_alloc_context3(codec);
            if (codecCtx == null)
            {
                throw new FFmpegException("Fail to alloc codec context");
            }
            codecCtx->width = 1;
            codecCtx->height = 1;
            codecCtx->pix_fmt = AVPixelFormat.AV_PIX_FMT_YUVJ420P;
            codecCtx->time_base = new AVRational { num = 1, den = 25 };

            int result = ffmpeg.avcodec_open2(codecCtx, codec, null);
            if (result < 0)
            {
                throw new FFmpegException(result, "Fail to open codec.");
            }

            frame = ffmpeg.av_frame_alloc();
            frame->width = codecCtx->width;
            frame->height = codecCtx->height;
            frame->format = (int)codecCtx->pix_fmt;

            packet = ffmpeg.av_packet_alloc();
            packet->data = imageData;
            packet->size = size;
            packet->stream_index = coverStream->index;

            result = ffmpeg.avcodec_parameters_from_context(coverStream->codecpar, codecCtx);
            if (result < 0)
            {
                throw new FFmpegException(result, "Fail to bind parameter from context");
            }

            result = ffmpeg.av_write_frame(formatCtx, packet);
            if (result < 0)
            {
                throw new FFmpegException(result, "Fail to write cover frame");
            }
        }
        finally
        {
            if (coverStream != null) { /* 似乎并不需要是否AVStream */ }
            if (codecCtx != null) ffmpeg.avcodec_free_context(&codecCtx);
            if (packet != null) ffmpeg.av_packet_free(&packet);
            if (frame != null) ffmpeg.av_frame_free(&frame);
        }
    }

    private void AddReEncodeStream(int streamIdx, AVMediaType codecType)
    {
        var inStream = _inputCtx!.FormatContextPointer->streams[streamIdx];
        var decCtx = CreateDecoder(inStream);

        var outStream = ffmpeg.avformat_new_stream(_outputCtx, null);
        if (outStream == null)
            throw new FFmpegException("Failed to create output stream.");

        var encCtx = CreateEncoder(codecType, decCtx, outStream);
        var filterCtx = CreateFilterGraph(decCtx, encCtx, codecType);

        _streams.Add(new TranscodeStream
        {
            StreamIndex = streamIdx,
            OutStream = outStream,
            DecoderContext = decCtx,
            EncoderContext = encCtx,
            FilterContext = filterCtx,
            DecodeFrame = ffmpeg.av_frame_alloc(),
            FilteredFrame = ffmpeg.av_frame_alloc(),
            EncodePacket = ffmpeg.av_packet_alloc()
        });
    }

    private void AddRemuxStream(int streamIdx)
    {
        var inStream = _inputCtx!.FormatContextPointer->streams[streamIdx];
        var outStream = ffmpeg.avformat_new_stream(_outputCtx, null);
        if (outStream == null)
            throw new FFmpegException("Failed to create output stream.");

        int ret = ffmpeg.avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        if (ret < 0)
            throw new FFmpegException(ret, "Failed to copy codec parameters for remux.");
        outStream->time_base = inStream->time_base;

        _streams.Add(new TranscodeStream
        {
            StreamIndex = streamIdx,
            OutStream = outStream,
            IsRemux = true,
        });
    }

    private AVCodecContext* CreateDecoder(AVStream* inStream)
    {
        AVCodec* dec = ffmpeg.avcodec_find_decoder(inStream->codecpar->codec_id);
        if (dec == null)
            throw new FFmpegException($"Failed to find decoder for {inStream->codecpar->codec_id}.");

        AVCodecContext* ctx = ffmpeg.avcodec_alloc_context3(dec);
        if (ctx == null)
            throw new FFmpegException("Failed to allocate decoder context.");

        int ret = ffmpeg.avcodec_parameters_to_context(ctx, inStream->codecpar);
        if (ret < 0)
            throw new FFmpegException(ret, "Failed to copy parameters to decoder.");

        ctx->pkt_timebase = inStream->time_base;
        ret = ffmpeg.avcodec_open2(ctx, dec, null);
        if (ret < 0)
            throw new FFmpegException(ret, "Failed to open decoder.");

        return ctx;
    }

    private AVCodecContext* CreateEncoder(AVMediaType type, AVCodecContext* decCtx, AVStream* outStream)
    {
        AVCodecID codecId;
        if (type == AVMediaType.AVMEDIA_TYPE_VIDEO)
        {
            string? name = _option.Video?.Codec;
            codecId = !string.IsNullOrEmpty(name)
                ? ffmpeg.avcodec_find_encoder_by_name(name)->id
                : decCtx->codec_id;
        }
        else
        {
            string? name = _option.Audio?.Codec;
            codecId = !string.IsNullOrEmpty(name)
                ? ffmpeg.avcodec_find_encoder_by_name(name)->id
                : decCtx->codec_id;
        }

        AVCodec* enc = ffmpeg.avcodec_find_encoder(codecId);
        if (enc == null)
            throw new FFmpegException($"Encoder not found for {codecId}.");

        AVCodecContext* ctx = ffmpeg.avcodec_alloc_context3(enc);
        if (ctx == null)
            throw new FFmpegException("Failed to allocate encoder context.");

        if (type == AVMediaType.AVMEDIA_TYPE_VIDEO)
        {
            var vOpt = _option.Video!;
            ctx->width = vOpt.Width ?? decCtx->width;
            ctx->height = vOpt.Height ?? decCtx->height;
            ctx->pix_fmt = vOpt.PixelFormat ?? decCtx->pix_fmt;
            ctx->sample_aspect_ratio = decCtx->sample_aspect_ratio;
            ctx->framerate = vOpt.FrameRate ?? decCtx->framerate;
            ctx->time_base = ffmpeg.av_inv_q(ctx->framerate);
            ctx->bit_rate = vOpt.BitRate ?? 0;
            if (vOpt.GopSize.HasValue)
                ctx->gop_size = vOpt.GopSize.Value;
            if (vOpt.MaxBFrame.HasValue)
                ctx->max_b_frames = vOpt.MaxBFrame.Value;
        }
        else
        {
            var aOpt = _option.Audio!;
            ctx->sample_rate = aOpt.SampleRate ?? decCtx->sample_rate;
            ctx->sample_fmt = aOpt.SampleFormat ?? decCtx->sample_fmt;
            ffmpeg.av_channel_layout_copy(&ctx->ch_layout, &decCtx->ch_layout);
            ctx->time_base = new AVRational { num = 1, den = ctx->sample_rate };
            ctx->bit_rate = aOpt.BitRate ?? 0;
        }

        ctx->thread_count = _option.ThreadCount;

        if ((_outputCtx->oformat->flags & ffmpeg.AVFMT_GLOBALHEADER) != 0)
            ctx->flags |= ffmpeg.AV_CODEC_FLAG_GLOBAL_HEADER;

        int ret = ffmpeg.avcodec_open2(ctx, enc, null);
        if (ret < 0)
            throw new FFmpegException(ret,
                $"Failed to open encoder {Marshal.PtrToStringAnsi((IntPtr)enc->name)}.");

        ret = ffmpeg.avcodec_parameters_from_context(outStream->codecpar, ctx);
        if (ret < 0)
            throw new FFmpegException(ret, "Failed to copy encoder parameters to stream.");

        outStream->time_base = ctx->time_base;

        return ctx;
    }

    private TranscodeFilterContext* CreateFilterGraph(
        AVCodecContext* decCtx, AVCodecContext* encCtx, AVMediaType type)
    {
        AVFilterGraph* graph = ffmpeg.avfilter_graph_alloc();
        if (graph == null)
            throw new FFmpegException("Failed to allocate filter graph.");

        AVFilterContext* srcCtx = null;
        AVFilterContext* sinkCtx = null;

        if (type == AVMediaType.AVMEDIA_TYPE_VIDEO)
        {
            var src = ffmpeg.avfilter_get_by_name("buffer");
            var sink = ffmpeg.avfilter_get_by_name("buffersink");
            if (src == null || sink == null)
                throw new FFmpegException("Buffer source/sink not found.");

            var args = $"video_size={decCtx->width}x{decCtx->height}:pix_fmt={decCtx->pix_fmt}:" +
                       $"time_base={decCtx->pkt_timebase.num}/{decCtx->pkt_timebase.den}:" +
                       $"pixel_aspect={decCtx->sample_aspect_ratio.num}/{decCtx->sample_aspect_ratio.den}";

            int ret = ffmpeg.avfilter_graph_create_filter(&srcCtx, src, "in", args, null, graph);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to create video buffer source.");

            sinkCtx = ffmpeg.avfilter_graph_alloc_filter(graph, sink, "out");
            if (sinkCtx == null)
                throw new FFmpegException("Failed to allocate video buffer sink.");

            var pixFmt = (int)encCtx->pix_fmt;
            ret = ffmpeg.av_opt_set_bin(sinkCtx, "pix_fmts", (byte*)&pixFmt, sizeof(int),
                ffmpeg.AV_OPT_SEARCH_CHILDREN);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to set output pixel format.");

            ret = ffmpeg.avfilter_init_dict(sinkCtx, null);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to init video buffer sink.");
        }
        else
        {
            var src = ffmpeg.avfilter_get_by_name("abuffer");
            var sink = ffmpeg.avfilter_get_by_name("abuffersink");
            if (src == null || sink == null)
                throw new FFmpegException("Audio buffer source/sink not found.");

            if (decCtx->ch_layout.order == AVChannelOrder.AV_CHANNEL_ORDER_UNSPEC)
                ffmpeg.av_channel_layout_default(&decCtx->ch_layout, decCtx->ch_layout.nb_channels);

            byte* chBuf = stackalloc byte[256];
            ffmpeg.av_channel_layout_describe(&decCtx->ch_layout, chBuf, 256);
            string chDesc = Marshal.PtrToStringAnsi((IntPtr)chBuf)!;
            var fmtNameStr = ffmpeg.av_get_sample_fmt_name((AVSampleFormat)decCtx->sample_fmt);
            var args = $"time_base={decCtx->pkt_timebase.num}/{decCtx->pkt_timebase.den}:" +
                       $"sample_rate={decCtx->sample_rate}:sample_fmt={fmtNameStr}:" +
                       $"channel_layout={chDesc}";

            int ret = ffmpeg.avfilter_graph_create_filter(&srcCtx, src, "in", args, null, graph);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to create audio buffer source.");

            sinkCtx = ffmpeg.avfilter_graph_alloc_filter(graph, sink, "out");
            if (sinkCtx == null)
                throw new FFmpegException("Failed to allocate audio buffer sink.");

            var sampleFmt = (int)encCtx->sample_fmt;
            ret = ffmpeg.av_opt_set_bin(sinkCtx, "sample_fmts", (byte*)&sampleFmt, sizeof(int),
                ffmpeg.AV_OPT_SEARCH_CHILDREN);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to set output sample format.");

            ffmpeg.av_channel_layout_describe(&encCtx->ch_layout, chBuf, 256);
            string outCh = Marshal.PtrToStringAnsi((IntPtr)chBuf)!;
            ret = ffmpeg.av_opt_set(sinkCtx, "ch_layouts", outCh, ffmpeg.AV_OPT_SEARCH_CHILDREN);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to set output channel layout.");

            var sampleRate = encCtx->sample_rate;
            ret = ffmpeg.av_opt_set_bin(sinkCtx, "sample_rates", (byte*)&sampleRate, sizeof(int),
                ffmpeg.AV_OPT_SEARCH_CHILDREN);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to set output sample rate.");

            if (encCtx->frame_size > 0)
                ffmpeg.av_buffersink_set_frame_size(sinkCtx, (uint)encCtx->frame_size);

            ret = ffmpeg.avfilter_init_dict(sinkCtx, null);
            if (ret < 0)
                throw new FFmpegException(ret, "Failed to init audio buffer sink.");
        }

        // Link with optional user filter
        var outputs = ffmpeg.avfilter_inout_alloc();
        var inputs = ffmpeg.avfilter_inout_alloc();
        if (outputs == null || inputs == null)
            throw new FFmpegException("Failed to allocate filter inout.");

        outputs->name = ffmpeg.av_strdup("in");
        outputs->filter_ctx = srcCtx;
        outputs->pad_idx = 0;
        outputs->next = null;

        inputs->name = ffmpeg.av_strdup("out");
        inputs->filter_ctx = sinkCtx;
        inputs->pad_idx = 0;
        inputs->next = null;

        string? filterSpec = type == AVMediaType.AVMEDIA_TYPE_VIDEO
            ? _option.VideoFilter
            : _option.AudioFilter;

        if (string.IsNullOrEmpty(filterSpec))
            filterSpec = type == AVMediaType.AVMEDIA_TYPE_VIDEO ? "null" : "anull";

        int linkRet = ffmpeg.avfilter_graph_parse_ptr(graph, filterSpec, &inputs, &outputs, null);
        if (linkRet < 0)
            throw new FFmpegException(linkRet, "Failed to parse filter graph.");

        linkRet = ffmpeg.avfilter_graph_config(graph, null);
        if (linkRet < 0)
            throw new FFmpegException(linkRet, "Failed to configure filter graph.");

        ffmpeg.avfilter_inout_free(&inputs);
        ffmpeg.avfilter_inout_free(&outputs);

        var fctx = (TranscodeFilterContext*)ffmpeg.av_mallocz((ulong)sizeof(TranscodeFilterContext));
        fctx->SourceContext = srcCtx;
        fctx->SinkContext = sinkCtx;
        fctx->FilterGraph = graph;
        return fctx;
    }

    private void OpenOutputStream(Stream stream)
    {
        const int ioBufferSize = 512 * 1024;
        byte* buffer = (byte*)ffmpeg.av_malloc(ioBufferSize);
        if (buffer == null)
            throw new FFmpegException("Failed to allocate output IO buffer.");

        var ioCtx = new WriteIOContext(stream);
        var handle = GCHandle.Alloc(ioCtx, GCHandleType.Normal);
        void* opaque = GCHandle.ToIntPtr(handle).ToPointer();

        var writeDelegate = new avio_alloc_context_write_packet(WriteIOContext.FFmpegWrite);
        var seekDelegate = new avio_alloc_context_seek(WriteIOContext.FFmpegSeek);

        AVIOContext* avioCtx = ffmpeg.avio_alloc_context(
            buffer, ioBufferSize, 1, opaque, null, writeDelegate, seekDelegate);

        if (avioCtx == null)
        {
            handle.Free();
            ffmpeg.av_free(buffer);
            throw new FFmpegException("Failed to allocate output AVIO context.");
        }

        _outputCtx->pb = avioCtx;
        _writeIOHandle = handle;
        _ownsOutputIO = true;
    }

    public void Run(CancellationToken cancellationToken = default)
    {
        _cancelSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        var ifmt = _inputCtx!.FormatContextPointer;
        double duration = _inputCtx.Duration;
        using var packet = new Packet();
        var pkt = packet.PacketPointer;

        while (!_cancelSource.Token.IsCancellationRequested)
        {
            int ret = ffmpeg.av_read_frame(ifmt, pkt);
            if (ret < 0)
                break;

            int streamIdx = pkt->stream_index;
            var stream = GetStream(streamIdx);
            if (stream == null)
            {
                ffmpeg.av_packet_unref(pkt);
                continue;
            }

            if (stream.FilterContext != null)
            {
                ProcessReEncode(stream, pkt);
            }
            else
            {
                ProcessRemux(stream, pkt);
            }

            ffmpeg.av_packet_unref(pkt);

            if (duration > 0)
            {
                double pos = (double)(pkt->pts * ffmpeg.av_q2d(ifmt->streams[streamIdx]->time_base));
                ProgressChanged?.Invoke(pos / duration);
            }
        }

        Flush();
        Completed?.Invoke();
    }

    public Task RunAsync(CancellationToken cancellationToken = default)
        => Task.Run(() => Run(cancellationToken), cancellationToken);

    private void ProcessReEncode(TranscodeStream stream, AVPacket* pkt)
    {
        int ret = ffmpeg.avcodec_send_packet(stream.DecoderContext, pkt);
        if (ret < 0) return;

        while (ret >= 0)
        {
            ret = ffmpeg.avcodec_receive_frame(stream.DecoderContext, stream.DecodeFrame);
            if (ret == ffmpeg.AVERROR(ffmpeg.EAGAIN) || ret == ffmpeg.AVERROR_EOF)
                break;
            if (ret < 0)
            {
                Error?.Invoke($"Decode error: {ret}");
                break;
            }

            stream.DecodeFrame->pts = stream.DecodeFrame->best_effort_timestamp;
            FilterEncodeWrite(stream, stream.DecodeFrame);
        }
    }

    private void FilterEncodeWrite(TranscodeStream stream, AVFrame* frame)
    {
        var fctx = stream.FilterContext!;
        int ret = ffmpeg.av_buffersrc_add_frame_flags(fctx->SourceContext, frame, 0);
        if (ret < 0) return;

        while (true)
        {
            ret = ffmpeg.av_buffersink_get_frame(fctx->SinkContext, stream.FilteredFrame);
            if (ret == ffmpeg.AVERROR(ffmpeg.EAGAIN) || ret == ffmpeg.AVERROR_EOF)
                break;
            if (ret < 0) break;

            stream.FilteredFrame->time_base = ffmpeg.av_buffersink_get_time_base(fctx->SinkContext);
            EncodeWrite(stream, stream.FilteredFrame);
            ffmpeg.av_frame_unref(stream.FilteredFrame);
        }
    }

    private void EncodeWrite(TranscodeStream stream, AVFrame* frame)
    {
        if (frame->pts != ffmpeg.AV_NOPTS_VALUE)
        {
            frame->pts = ffmpeg.av_rescale_q(frame->pts, frame->time_base,
                stream.EncoderContext->time_base);
        }

        int ret = ffmpeg.avcodec_send_frame(stream.EncoderContext, frame);
        if (ret < 0) return;

        ffmpeg.av_packet_unref(stream.EncodePacket);

        while (ret >= 0)
        {
            ret = ffmpeg.avcodec_receive_packet(stream.EncoderContext, stream.EncodePacket);
            if (ret == ffmpeg.AVERROR(ffmpeg.EAGAIN) || ret == ffmpeg.AVERROR_EOF)
                break;
            if (ret < 0) break;

            stream.EncodePacket->stream_index = stream.OutStream->index;
            ffmpeg.av_packet_rescale_ts(stream.EncodePacket,
                stream.EncoderContext->time_base, stream.OutStream->time_base);
            ffmpeg.av_interleaved_write_frame(_outputCtx, stream.EncodePacket);
        }
    }

    private void ProcessRemux(TranscodeStream stream, AVPacket* pkt)
    {
        ffmpeg.av_packet_rescale_ts(pkt,
            _inputCtx!.FormatContextPointer->streams[stream.StreamIndex]->time_base,
            stream.OutStream->time_base);
        pkt->stream_index = stream.OutStream->index;
        ffmpeg.av_interleaved_write_frame(_outputCtx, pkt);
    }

    private void Flush()
    {
        if (_cancelSource?.IsCancellationRequested == true) return;

        foreach (var stream in _streams)
        {
            if (stream.FilterContext == null) continue;

            // Flush decoder
            ffmpeg.avcodec_send_packet(stream.DecoderContext, null);
            while (true)
            {
                int ret = ffmpeg.avcodec_receive_frame(stream.DecoderContext, stream.DecodeFrame);
                if (ret == ffmpeg.AVERROR_EOF) break;
                if (ret < 0) break;
                stream.DecodeFrame->pts = stream.DecodeFrame->best_effort_timestamp;
                FilterEncodeWrite(stream, stream.DecodeFrame);
            }

            // Flush filter
            ffmpeg.av_buffersrc_add_frame_flags(
                stream.FilterContext->SourceContext, null, 0);
            while (true)
            {
                int ret = ffmpeg.av_buffersink_get_frame(
                    stream.FilterContext->SinkContext, stream.FilteredFrame);
                if (ret == ffmpeg.AVERROR_EOF || ret == ffmpeg.AVERROR(ffmpeg.EAGAIN)) break;
                if (ret < 0) break;
                stream.FilteredFrame->time_base = ffmpeg.av_buffersink_get_time_base(
                    stream.FilterContext->SinkContext);
                EncodeWrite(stream, stream.FilteredFrame);
                ffmpeg.av_frame_unref(stream.FilteredFrame);
            }

            // Flush encoder
            EncodeWrite(stream, null);
        }

        ffmpeg.av_write_trailer(_outputCtx);
    }

    private TranscodeStream? GetStream(int inputStreamIdx)
    {
        foreach (var s in _streams)
            if (s.StreamIndex == inputStreamIdx) return s;
        return null;
    }

    public void Dispose()
    {
        _cancelSource?.Cancel();
        _cancelSource?.Dispose();
        _cancelSource = null;

        foreach (var stream in _streams)
        {
            if (stream.DecoderContext != null)
            {
                var p = stream.DecoderContext;
                ffmpeg.avcodec_free_context(&p);
                stream.DecoderContext = null;
            }
            if (stream.EncoderContext != null)
            {
                var p = stream.EncoderContext;
                ffmpeg.avcodec_free_context(&p);
                stream.EncoderContext = null;
            }
            if (stream.FilterContext != null)
            {
                var fctx = stream.FilterContext;
                ffmpeg.avfilter_graph_free(&fctx->FilterGraph);
                ffmpeg.av_free(fctx);
                stream.FilterContext = null;
            }
            if (stream.DecodeFrame != null)
            {
                var p = stream.DecodeFrame;
                ffmpeg.av_frame_free(&p);
                stream.DecodeFrame = null;
            }
            if (stream.FilteredFrame != null)
            {
                var p = stream.FilteredFrame;
                ffmpeg.av_frame_free(&p);
                stream.FilteredFrame = null;
            }
            if (stream.EncodePacket != null)
            {
                var p = stream.EncodePacket;
                ffmpeg.av_packet_free(&p);
                stream.EncodePacket = null;
            }
        }
        _streams.Clear();

        if (_outputCtx != null)
        {
            if (_ownsOutputIO && _outputCtx->pb != null)
            {
                ffmpeg.avio_context_free(&_outputCtx->pb);
            }
            else if ((_outputCtx->oformat->flags & ffmpeg.AVFMT_NOFILE) == 0
                && _outputCtx->pb != null)
            {
                ffmpeg.avio_closep(&_outputCtx->pb);
            }
            ffmpeg.avformat_free_context(_outputCtx);
            _outputCtx = null;
        }

        if (_writeIOHandle != null)
        {
            if (_writeIOHandle.Value.IsAllocated)
                _writeIOHandle.Value.Free();
            _writeIOHandle = null;
        }

        _inputCtx?.Dispose();
        _inputCtx = null;
    }
}

internal unsafe struct TranscodeFilterContext
{
    public AVFilterContext* SourceContext;
    public AVFilterContext* SinkContext;
    public AVFilterGraph* FilterGraph;
}

internal unsafe class TranscodeStream
{
    public int StreamIndex;
    public AVStream* OutStream;
    public AVCodecContext* DecoderContext;
    public AVCodecContext* EncoderContext;
    public TranscodeFilterContext* FilterContext;
    public AVFrame* DecodeFrame;
    public AVFrame* FilteredFrame;
    public AVPacket* EncodePacket;
    public bool IsRemux;
}

internal sealed unsafe class WriteIOContext(Stream stream)
{
    private readonly Stream _stream = stream;

    private int Write(byte* buffer, int bufferSize)
    {
        _stream.Write(new ReadOnlySpan<byte>(buffer, bufferSize));
        return bufferSize;
    }

    private long Seek(long offset, int whence)
    {
        const int SEEK_SET = 0;
        const int SEEK_CUR = 1;
        const int SEEK_END = 2;
        const int AVSEEK_SIZE = 65536;

        if (whence == AVSEEK_SIZE)
            return _stream.Length;

        SeekOrigin origin = whence switch
        {
            SEEK_SET => SeekOrigin.Begin,
            SEEK_CUR => SeekOrigin.Current,
            SEEK_END => SeekOrigin.End,
            _ => SeekOrigin.Begin
        };
        return _stream.Seek(offset, origin);
    }

    public static int FFmpegWrite(void* opaque, byte* buffer, int bufferSize)
    {
        var handle = GCHandle.FromIntPtr((IntPtr)opaque);
        var ctx = (WriteIOContext)handle.Target!;
        return ctx.Write(buffer, bufferSize);
    }

    public static long FFmpegSeek(void* opaque, long offset, int whence)
    {
        var handle = GCHandle.FromIntPtr((IntPtr)opaque);
        var ctx = (WriteIOContext)handle.Target!;
        return ctx.Seek(offset, whence);
    }
}
