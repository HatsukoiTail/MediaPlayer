// #include "MediaData.h"

// #include "Factory.h"
// #include "Print.h"
// #include "Util.h"

// extern "C"
// {
// #include <libavutil/imgutils.h>
// }

// #include <QPixmap>

// #include <cassert>

// enum class MediaData::DecodeResult
// {
//     FrameReady,
//     NeedMoreData,
//     EndOfStream,
//     Error
// };

// MediaData::MediaData(const QString &path)
// {
//     this->open(path);
// }

// MediaData::~MediaData()
// {
//     this->close();
// }

// bool MediaData::open(const QString &path)
// {
//     assert(!this->is_opened);

//     auto result = this->data_stream.open(path.toStdString());
//     if (!result)
//         return false;

//     const auto meta_type = this->data_stream.metaType();
//     if (meta_type != MetaType::Video && meta_type != MetaType::Audio)
//     {
//         this->is_opened = true;
//         return true;
//     }

//     this->format_ctx = AVFormatContextPointer(open_format_context(this, read_callback, seek_callback));
//     if (!this->format_ctx)
//         return false;

//     this->video_index = av_find_best_stream(this->format_ctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
//     this->codec_ctx = AVCodecContextPointer(open_codec_context(this->format_ctx->streams[this->video_index]));
//     if (!this->codec_ctx)
//         return false;

//     this->is_opened = true;
//     return true;
// }

// void MediaData::close()
// {
//     if (!this->is_opened)
//         return;
//     this->data_stream.close();
//     this->is_opened = false;
//     this->sws_ctx.reset();
//     this->codec_ctx.reset();
//     this->format_ctx.reset();
// }

// bool MediaData::isOpen() const
// {
//     return this->is_opened;
// }

// QImage MediaData::thumbnail()
// {
//     assert(this->is_opened && this->format_ctx && this->codec_ctx);
//     AVFramePointer frame;
//     bool looping = true;
//     while (looping)
//     {
//         auto packet = this->read_packet();
//         if (!packet)
//             return {};
//         auto [result, return_frame] = this->read_frame(std::move(packet));
//         switch (result) {
//         case DecodeResult::FrameReady:
//             frame = std::move(return_frame);
//             looping = false;
//             break;
//         case DecodeResult::NeedMoreData:
//             continue;
//             break;
//         case DecodeResult::EndOfStream:
//             return {};
//         default:
//             print(Ansi::Red, "Error occurred when decode {}", this->data_stream.fileName());
//             return {};
//         }
//     }
//     if (!frame)
//         return {};
//     if (frame->format != AV_PIX_FMT_RGBA)
//     {
//         this->sws_ctx = SwsContextPointer(open_sws_context(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
//                                                              frame->width, frame->height, AV_PIX_FMT_RGBA));
//         if (!this->sws_ctx)
//             return {};

//         auto final_frame = this->sws_frame(std::move(frame));
//         if (!final_frame)
//             return {};

//         frame = std::move(final_frame);
//     }

//     QImage image(frame->width, frame->height, QImage::Format_RGBA8888);
//     uint8_t* image_ptr = image.bits();
//     const auto line_size = frame->width * 4;
//     const auto frame_ptr = frame->data[0];
//     const auto stride = frame->linesize[0];
//     for (int y = 0; y < frame->height; ++y)
//     {
//         memcpy(image_ptr, frame_ptr + y * stride, line_size);
//         image_ptr += line_size;
//     }
//     return image;
// }

// MetaData MediaData::metaData()
// {
//     MetaData data;
//     data.isEncrypted = this->data_stream.isEncrypted();
//     data.filePath = QString::fromStdString(this->data_stream.filePath());
//     data.fileName = QString::fromStdString(this->data_stream.fileName());
//     data.fileSize = this->data_stream.fileSize();
//     data.metaType = this->data_stream.metaType();
//     data.lastModified = this->data_stream.lastModified();
//     data.duration = (data.metaType == MetaType::Video || data.metaType == MetaType::Audio) ? this->format_ctx->duration / 1000 : 0;
//     data.image = (data.metaType == MetaType::Video) ? this->thumbnail() : QImage();
//     return data;
// }

// MetaData MediaData::metaData(const QString &path)
// {
//     MediaData media(path);
//     if (!media.isOpen())
//         return {};
//     return media.metaData();
// }

// AVPacketPointer MediaData::read_packet()
// {
//     AVPacketPointer packet(av_packet_alloc());
//     while (true)
//     {
//         int result = av_read_frame(this->format_ctx.get(), packet.get());
//         if (result == AVERROR_EOF)
//         {
//             break;
//         }
//         if (result < 0)
//         {
//             print(Ansi::Red, "Fail to read frame while demuxer running, {}", debug(result));
//             break;
//         }
//         if (packet->stream_index != this->video_index)
//         {
//             av_packet_unref(packet.get());
//             continue;
//         }
//         return packet;
//     }
//     return nullptr;
// }

// std::pair<MediaData::DecodeResult, AVFramePointer> MediaData::read_frame(AVPacketPointer packet)
// {
//     AVFramePointer frame(av_frame_alloc());
//     while (true)
//     {
//         auto result = avcodec_send_packet(this->codec_ctx.get(), packet.get());
//         if (result == AVERROR_EOF)
//         {
//             print(Ansi::Red, "Decode reach EOF.");
//             return {DecodeResult::EndOfStream, nullptr};
//         }
//         else if (result < 0)
//         {
//             print(Ansi::Red, "Fail to send video packet to decoder, {}", debug(result));
//             return {DecodeResult::Error, nullptr};
//         }
//         while (true)
//         {
//             result = avcodec_receive_frame(this->codec_ctx.get(), frame.get());
//             if (result == AVERROR(EAGAIN))
//             {
//                 return {DecodeResult::NeedMoreData, nullptr};
//             }
//             if (result == AVERROR_EOF)
//             {
//                 return {DecodeResult::EndOfStream, nullptr};
//             }
//             if (result < 0)
//             {
//                 print(Ansi::Red, "Fail to receive video frame from decoder, {}", debug(result));
//                 return {DecodeResult::Error, nullptr};
//             }
//             return {DecodeResult::FrameReady, std::move(frame)};
//         }
//     }
// }

// AVFramePointer MediaData::sws_frame(AVFramePointer frame)
// {
//     AVFramePointer result(av_frame_alloc());
//     result->width = frame->width;
//     result->height = frame->height;
//     result->format = AV_PIX_FMT_RGBA;

//     auto error = av_frame_get_buffer(result.get(), 32);
//     if (error < 0)
//         return nullptr;
//     error = sws_scale_frame(this->sws_ctx.get(), result.get(), frame.get());
//     if (error < 0)
//         return nullptr;

//     return result;
// }

// int MediaData::read_callback(void *opaque, uint8_t *buf, int buf_size)
// {
//     MediaData* media_data = reinterpret_cast<MediaData*>(opaque);
//     const auto read_size = media_data->data_stream.read(buf, buf_size);
//     if (read_size == 0)
//         return AVERROR_EOF;
//     else if (read_size < 0)
//         return AVERROR(EIO);
//     else
//         return static_cast<int>(read_size);
// }

// int64_t MediaData::seek_callback(void *opaque, int64_t offset, int whence)
// {
//     MediaData* media_data = reinterpret_cast<MediaData*>(opaque);
//     auto& media_stream = media_data->data_stream;
//     if (whence == AVSEEK_SIZE)
//         return media_stream.fileSize();

//     std::ios_base::seekdir dir;
//     switch (whence) {
//     case SEEK_SET: dir = std::ios::beg; break;
//     case SEEK_CUR: dir = std::ios::cur; break;
//     case SEEK_END: dir = std::ios::end; break;
//     default: return AVERROR(EINVAL);
//     }

//     if (!media_stream.seek(offset, dir))
//         return AVERROR(EIO);

//     return media_stream.pos();
// }
