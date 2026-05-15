#include "Helper.h"

struct DecodeResult
{
    FFmpegResult result;
};

std::tuple<FFmpegResult, Frame> decode_frame(PacketQueue &packet_queue, AVCodecContext *codec_ctx, std::condition_variable& cv)
{
    int result = 0;

    int serial = -1;

    Frame frame(av_frame_alloc());
    Packet packet(av_packet_alloc());
    const auto codec_type = codec_ctx->codec_type;

    // next_pts表示当前音频帧播放完毕后的下一帧开始时的理论pts
    // 该值用于修复解码器未给出有效pts的情况
    int64_t next_pts = AV_NOPTS_VALUE;
    AVRational next_pts_time_base = AVRational{1, 1000};

    bool packet_pending = false;

    while (true)
    {
        // 只有序列号相等时，才会从解码器中取出帧
        // 序列号不相等，意味着发生了跳转，PacketQueue的序列号更新了
        if (serial == packet_queue.serial())
        {
            // 从解码器取出一帧
            if (packet_queue.is_aborted())
                break;

            if (codec_type == AVMEDIA_TYPE_VIDEO)
            {
                result = avcodec_receive_frame(codec_ctx, frame.ptr());
                if (result >= 0)
                {
                    frame.ptr()->pts = frame.ptr()->best_effort_timestamp;
                }
            }
            else if (codec_type == AVMEDIA_TYPE_AUDIO)
            {
                result = avcodec_receive_frame(codec_ctx, frame.ptr());
                if (result >= 0)
                {
                    // 将pts标准化为基于采样率时间基的pts
                    AVRational time_base = (AVRational){1, frame.sample_rate()};

                    // 如果pts有效，则使用该pts
                    // 如果pts无效，则使用上一帧播放完成后的下一帧开始时的理论pts(即next_pts)推断当前帧的pts
                    if (frame.pts() != AV_NOPTS_VALUE)
                        frame.ptr()->pts = av_rescale_q(frame.pts(), codec_ctx->time_base, time_base);
                    else if (next_pts != AV_NOPTS_VALUE)
                        frame.ptr()->pts = av_rescale_q(next_pts, next_pts_time_base, time_base);
                    else
                    {
                        // do nothing
                    }

                    // 推断下一帧开始时的理论pts
                    if (frame.ptr()->pts != AV_NOPTS_VALUE)
                    {
                        next_pts = frame.pts() + frame.samples();
                        next_pts_time_base = time_base;
                    }
                }
            }

            result = avcodec_receive_frame(codec_ctx, frame.ptr());
            if (result >= 0)
            {
                // 成功取出一帧
                return {FFmpegResult::Success, std::move(frame)};
            }
            if (result == AVERROR_EOF)
            {
                // 解码器已经解码完毕
                avcodec_flush_buffers(codec_ctx);
                return {FFmpegResult::Eof, Frame()};
            }
            else if (result < 0 && result != AVERROR(EAGAIN))
            {
                // 解码器出错
                return {FFmpegResult::Error, Frame()};
            }
        }

        while (true)
        {
            if (packet_queue.count() == 0)
            {
                // 包队列为空，先唤醒解复用线程(该线程在队列满时或者解码结束时会休眠，因此需要唤醒让其继续解复用)
                cv.notify_one();
            }
            if (packet_pending)
            {
                packet_pending = false;
            }
            else
            {
                auto packet = packet_queue.pop();
                if (packet.is_null())
                {
                    // 这里实际上对应了packet_queue.is_aborted() == true
                    break;
                }
                if (packet.serial() != serial)
                {
                    avcodec_flush_buffers(codec_ctx);
                    next_pts = AV_NOPTS_VALUE;
                }
                if (packet_queue.serial() == serial)
                    break;
                av_packet_unref(packet.ptr());
            }
        }
        if (codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            int got_frame = 0;
            AVSubtitle sub;
            int result = avcodec_decode_subtitle2(codec_ctx, &sub, &got_frame, packet.ptr());
            if (result < 0)
            {
                continue;
            }
            else
            {
                if (got_frame && !packet.ptr()->data)
                {
                    packet_pending = true;
                }
                int ret = got_frame ? 0 : (packet.ptr()->data ? AVERROR(EAGAIN) : AVERROR_EOF);
                av_packet_unref(packet.ptr());
            }
        }
        else
        {
            if (packet.ptr()->buf && !packet.ptr()->opaque_ref)
            {
                // 记录包的文件位置信息，后续用于按字节跳转
            }

            if (avcodec_send_packet(codec_ctx, packet.ptr()) == AVERROR(EAGAIN))
            {
                packet_pending = true;
            }
            else
            {
                av_packet_unref(packet.ptr());
            }
        }
    }
}
