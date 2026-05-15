#include "Decoder.h"

#include "Factory.h"
#include "Print.h"
#include "Util.h"

#include <cassert>

Decoder::Decoder(std::shared_ptr<Queue<AVPacketPointer>> packets)
    : packets{packets}, frames{std::make_shared<Queue<AVFramePointer>>(frame_queue_size)}
{}

Decoder::~Decoder()
{
    this->close();
    print(Ansi::BgGreen, "Decoder delete!");
}

// 不可重入函数
bool Decoder::open(AVStream *stream)
{
    assert(this->decode_state.load() == State::Closed && "Cannot open again.");
    assert(stream);
    this->codec_ctx = AVCodecContextPointer(open_codec_context(stream));
    if (!this->codec_ctx)
    {
        return false;
    }
    this->decode_state.store(State::Stopped);
    return true;
}

void Decoder::close()
{
    this->stop();
    this->packets.reset();
    this->frames->set_eof(true);
    this->codec_ctx.reset();
    this->decode_state.store(State::Closed);
}

void Decoder::run()
{
    assert(this->decode_state.load() != State::Closed && "Decoder has not been opened.");
    if (this->decode_state.load() == State::Stopped)
    {
        this->decode_thread = std::thread(&Decoder::decode_loop, this);
        return;
    }
    if (this->decode_state.load() == State::Paused)
    {
        this->decode_state.store(State::Running);
        return;
    }
}

void Decoder::pause()
{
    this->decode_state.store(State::Paused);
}

void Decoder::stop()
{
    this->decode_state.store(State::Stopped);

    if (this->decode_thread.joinable())
        this->decode_thread.join();
}

void Decoder::seek(int64_t timestamp, std::function<void (int64_t)> callback)
{
    auto last_state = this->decode_state.load();
    assert(last_state != State::Closed);
    if (last_state == State::Seeking)
        return;

    this->seek_timestamp = timestamp;
    this->seek_callback = std::move(callback);
    this->decode_state.store(State::Seeking);
    this->is_need_flush.store(true);
    this->last_state = last_state;
    this->frames->set_eof(false);
    if (last_state == State::Stopped)
    {
        this->decode_thread = std::thread(&Decoder::decode_loop, this);
    }
}

std::shared_ptr<Queue<AVFramePointer> > Decoder::data()
{
    return this->frames;
}

Decoder::State Decoder::state() const
{
    return this->decode_state.load();
}

AVFramePointer Decoder::process(AVFramePointer frame)
{
    return frame;
}

void Decoder::decode_loop()
{
    this->decode_state.store(State::Running);
    AVFramePointer last_frame, current_frame, pending_frame;
    bool need_send_packet = true;
    bool flushed = false;
    bool processed = false;
    while (true)
    {
        State state = this->decode_state.load();

        if (state == State::Stopped)
            break;

        if (state == State::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // --- seek 请求 flush ---
        if (this->is_need_flush.exchange(false))
        {
            print(Ansi::Red, "flush codec buffer, {}", this->codec_ctx->width > 0 ? "video" : "audio");
            avcodec_flush_buffers(this->codec_ctx.get());
            pending_frame.reset();
        }

        // 处理待处理的帧
        if (pending_frame && !processed)
        {
            // pending_frame有值，且尚未被处理过
            pending_frame = this->process(std::move(pending_frame));
            processed = true;
        }
        if (pending_frame && !this->frames->push(std::move(pending_frame)))
        {
            // pending_frame已经被处理过
            // 如果pending_frame有值，则push，并且push失败
            // 进入下个循环再次push
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (current_frame)
        {
            pending_frame = std::move(current_frame);
            processed = false;
            continue;
        }

        if (need_send_packet)
        {
            // --- 从包队列取数据 ---
            auto packet_optional = this->packets->pop();
            if (!packet_optional.has_value())
            {
                // EOF：尝试取出剩余帧
                if (this->packets->eof())
                {
                    auto flush_frame = this->flush_remain(flushed);
                    flushed = true;
                    if (flush_frame)
                    {
                        pending_frame = std::move(flush_frame);
                        continue;
                    }
                    else
                    {
                        this->frames->set_eof(true);
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 发送packet
            auto packet = std::move(packet_optional.value());
            int error = avcodec_send_packet(this->codec_ctx.get(), packet.get());
            if (error == AVERROR_EOF)
            {
                auto flush_frame = this->flush_remain(true);
                flushed = true;
                if (flush_frame)
                {
                    pending_frame = std::move(flush_frame);
                    continue;
                }
                else
                {
                    this->frames->set_eof(true);
                    break;
                }
            }
            else if (error < 0)
            {
                print(Ansi::Red, "Fail to send video packet to decoder, {}", debug(error));
                break;
            }
            need_send_packet = false;
        }

        // 接收frame
        if (!current_frame)
            current_frame = AVFramePointer(av_frame_alloc());

        int error = avcodec_receive_frame(this->codec_ctx.get(), current_frame.get());
        if (error == AVERROR(EAGAIN))
        {
            need_send_packet = true;
            current_frame.reset();
            continue;
        }
        if (error == AVERROR_EOF)
        {
            this->frames->set_eof(true);
            break;
        }
        if (error < 0)
        {
            print(Ansi::Red, "Fail to receive video frame from decoder, {}", debug(error));
            break;
        }

        current_frame->time_base = this->codec_ctx->time_base;

        // 成功接收到一帧，last_frame为空，current_frame有值
        const int64_t frame_time = av_rescale_q(current_frame->pts, this->codec_ctx->time_base, AVRational{1, 1000});
        if (state != State::Seeking)
        {
            pending_frame = std::move(current_frame);
            pending_frame->pts = frame_time;
            processed = false;
            continue;
        }
        else
        {
            // print(Ansi::Black, "time = {}, format = {}", frame_time, current_frame->width > 0 ? "video" : "audio");
            // 1. 当前帧小于时间戳
            if (frame_time < this->seek_timestamp)
            {
                last_frame = std::move(current_frame);
                continue;
            }
            // 2. 当前帧等于时间戳
            if (frame_time == this->seek_timestamp)
            {
                this->frames->clear();
                pending_frame = std::move(current_frame);
                processed = false;
                last_frame.reset();
                this->decode_state.store(this->last_state);
                if (this->seek_callback)
                    this->seek_callback(frame_time);
                continue;
            }
            // 3. 当前帧大于时间戳并且上一帧小于时间戳
            else if (last_frame && last_frame->pts < this->seek_timestamp)
            {
                this->frames->clear();
                pending_frame = std::move(last_frame);
                processed = false;
                this->decode_state.store(this->last_state);
                if (this->seek_callback)
                    this->seek_callback(pending_frame->pts);
                continue;
            }
            // 存在跳转后，时间戳大于跳转时间的情况
            else
            {
                this->frames->clear();
                pending_frame = std::move(current_frame);
                processed = false;
                last_frame.reset();
                this->decode_state.store(this->last_state);
                if (this->seek_callback)
                    this->seek_callback(frame_time);
                continue;
            }
        }
    }
    this->decode_state.store(State::Stopped);
}

AVFramePointer Decoder::flush_remain(bool flushed)
{
    if (!flushed)
    {
        int result = avcodec_send_packet(this->codec_ctx.get(), nullptr);
        if (result < 0)
        {
            print(Ansi::Red, "Fail to send flush packet to decoder, {}", debug(result));
            return nullptr;
        }
    }
    AVFramePointer frame = AVFramePointer(av_frame_alloc());
    while (true)
    {
        int result = avcodec_receive_frame(this->codec_ctx.get(), frame.get());
        if (result == AVERROR_EOF)
            return nullptr;
        if (result < 0)
        {
            print(Ansi::Red, "Fail to receive flush frame from decoder, {}", debug(result));
            return nullptr;
        }
    }
    return frame;
}

int Decoder::width() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO);
    return this->codec_ctx->width;
}

int Decoder::height() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO);
    return this->codec_ctx->height;
}

AVPixelFormat Decoder::pixel_format() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO);
    return this->codec_ctx->pix_fmt;
}

int Decoder::sample_rate() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO);
    return this->codec_ctx->sample_rate;
}

int Decoder::channel_count() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO);
    return this->codec_ctx->ch_layout.nb_channels;
}

AVSampleFormat Decoder::sample_format() const
{
    assert(this->codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO);
    return this->codec_ctx->sample_fmt;
}
