#include "VideoRender.h"

#include "Print.h"

VideoRender::VideoRender(std::shared_ptr<Queue<AVFramePointer>> frames)
    : frames{frames}
{}

VideoRender::~VideoRender()
{
    this->stop();
    print(Ansi::BgGreen, "VideoRender delete!");
}

void VideoRender::set_callback(std::function<void (AVFramePointer)> callback)
{
    assert(this->render_state.load() == State::Stopped);
    this->callback = std::move(callback);
}

void VideoRender::set_clock(std::function<int64_t ()> sync_clock)
{
    assert(this->render_state.load() == State::Stopped);
    this->external_clock = sync_clock;
}

void VideoRender::run()
{
    if (this->render_state.load() == State::Stopped)
    {
        if (this->external_clock)
        {
            this->thread = std::thread(&VideoRender::external_sync, this);
        }
        else
        {
            this->thread = std::thread(&VideoRender::self_sync, this);
        }
    }
    else if (this->render_state.load() == State::Paused)
    {
        this->render_state.store(State::Running);
    }
}

void VideoRender::stop()
{
    this->render_state.store(State::Stopped);
    if (this->thread.joinable())
        this->thread.join();
}

void VideoRender::pause()
{
    if (this->render_state.load() == State::Stopped)
        return;
    this->render_state.store(State::Paused);
}

void VideoRender::set_speed(double speed)
{
    this->play_speed = speed;
}

VideoRender::State VideoRender::state() const
{
    return this->render_state.load();
}

int64_t VideoRender::clock() const
{
    return this->video_time.load();
}

void VideoRender::self_sync()
{
    this->render_state.store(State::Running);
    AVFramePointer current_frame;
    AVFramePointer next_frame;
    bool has_eof = false;
    constexpr double default_interval = 40.0;
    while (true)
    {
        if (this->render_state.load() == State::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (this->render_state.load() == State::Stopped)
        {
            break;
        }

        if (!current_frame)
        {
            auto frame_opt = this->frames->pop();
            if (!frame_opt.has_value())
            {
                if (this->frames->eof())
                {
                    has_eof = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            current_frame = std::move(frame_opt.value());
        }

        if (!next_frame)
        {
            auto frame_opt = this->frames->pop();
            if (frame_opt.has_value())
                next_frame = std::move(frame_opt.value());
            else if (this->frames->eof())
                has_eof = true;
        }

        const double last_frame_time = current_frame->pts;

        if (this->callback)
            this->callback(std::move(current_frame));

        this->video_time.store(last_frame_time);

        if (!next_frame)
        {
            if (has_eof)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        const double next_frame_time = next_frame->pts;
        double delta_time = (next_frame_time - last_frame_time) / this->play_speed;

        if (delta_time <= 0 || delta_time > 1000)
            delta_time = default_interval / this->play_speed;

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delta_time)));

        current_frame = std::move(next_frame);
    }

    this->render_state.store(State::Stopped);
}

void VideoRender::external_sync()
{
    assert(this->external_clock);
    this->render_state.store(State::Running);
    while (true)
    {
        if (this->render_state.load() == State::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (this->render_state.load() == State::Stopped)
        {
            break;
        }
        auto frame_opt = this->frames->pop();
        if (!frame_opt.has_value())
        {
            if (this->frames->eof())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        auto frame = std::move(frame_opt.value());

        const double video_time = frame->pts;
        const double audio_time = this->external_clock();
        const double delta_time = (video_time - audio_time) / this->play_speed.load();

        // print(Ansi::Blue, "video time = {}, audio time = {}, delta = {}", video_time, audio_time, delta_time);

        // if (delta_time < -50)
        //     continue;
        if (delta_time > 30)
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(std::min(delta_time, 50.0))));
        if (this->callback)
            this->callback(std::move(frame));
        this->video_time.store(video_time);
    }
    this->render_state.store(State::Stopped);
}
