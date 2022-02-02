/*
 * Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
 *
 * This file is part of libaribcaption.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef _WIN32
    #include <windows.h>
#endif

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <cinttypes>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <deque>
#include <iomanip>
#include <fstream>
#include <sstream>
#include "aribcaption/decoder.hpp"

using namespace aribcaption;

#ifdef _WIN32
class UTF8CodePage {
public:
    UTF8CodePage() : old_codepage_(GetConsoleOutputCP()) {
        SetConsoleOutputCP(CP_UTF8);
    }
    ~UTF8CodePage() {
        SetConsoleOutputCP(old_codepage_);
    }
private:
    UINT old_codepage_;
};
#endif

class CaptionConverter {
public:
    explicit CaptionConverter() : decoder_(context_) {}

    ~CaptionConverter() {
        if (format_context_) {
            avformat_close_input(&format_context_);
        }
        if (ofs_.is_open()) {
            ofs_.close();
        }
    }
public:
    bool Open(const char* input_filename, const char* output_filename) {
        ofs_.open(output_filename);

        InitCaptionDecoder();

        format_context_ = avformat_alloc_context();

        int ret = 0;

        if ((ret = avformat_open_input(&format_context_, input_filename, nullptr, nullptr)) < 0) {
            fprintf(stderr, "avformat_open_input failed\n");
            return false;
        }

        if ((ret = avformat_find_stream_info(format_context_, nullptr)) < 0) {
            fprintf(stderr, "avformat_find_stream_info failed\n");
            return false;
        }

        for (size_t i = 0; i < format_context_->nb_streams; i++) {
            AVStream* stream = format_context_->streams[i];
            AVCodecParameters* codec_params = stream->codecpar;

            if (codec_params->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index_ == -1) {
                video_stream_index_ = stream->index;
            }

            if (codec_params->codec_id == AV_CODEC_ID_ARIB_CAPTION && arib_caption_index_ == -1) {
                arib_caption_index_ = stream->index;
            }
        }

        if (video_stream_index_ == -1) {
            fprintf(stderr, "Video stream not found\n");
            avformat_close_input(&format_context_);
            return false;
        }

        if (arib_caption_index_ == -1) {
            fprintf(stderr, "ARIB caption stream not found\n");
            avformat_close_input(&format_context_);
            return false;
        }

        return true;
    }

    void RunLoop() {
        int ret = 0;
        bool first_video_found = false;
        int64_t first_video_pts = 0;

        AVPacket packet{};

        while ((ret = av_read_frame(format_context_, &packet) == 0)) {
            if (packet.stream_index == video_stream_index_ && !first_video_found) {
                first_video_found = true;
                first_video_pts = packet.pts;
            } else if (packet.stream_index == arib_caption_index_) {
                AVStream* stream = format_context_->streams[arib_caption_index_];
                packet.pts -= first_video_pts;
                av_packet_rescale_ts(&packet, stream->time_base, AVRational{1, 1000});
                ConvertCaptionPacket(&packet);
            }
            av_packet_unref(&packet);
        }

        while (!caption_queue_.empty()) {
            Caption& caption = caption_queue_.front();
            if (caption.text.empty()) {
                caption_queue_.pop_front();
                continue;
            } else if (caption.wait_duration == DURATION_INDEFINITE) {
                caption.wait_duration = 1000;
            }
            DumpToSRT(caption);
            caption_queue_.pop_front();
        }
    }
private:
    void InitCaptionDecoder() {
        context_.SetLogcatCallback([](LogLevel level, const char* message) {
            if (level == LogLevel::kError || level == LogLevel::kWarning) {
                fprintf(stderr, "%s\n", message);
            } else {
                printf("%s\n", message);
            }
        });

        decoder_.Initialize(EncodingScheme::kAuto, CaptionType::kCaption);
    }

    static std::string MillisecondsToTime(int64_t millis) {
        std::ostringstream oss;

        oss << std::setfill('0') << std::setw(2) << millis / 1000 / 60 / 60;
        oss << ':';

        oss << std::setfill('0') << std::setw(2) << (millis / 1000 / 60) % 60;
        oss << ':';

        oss << std::setfill('0') << std::setw(2) << (millis / 1000) % 60;
        oss << ',';

        oss << std::setfill('0') << std::setw(2) << millis % 1000;

        return oss.str();
    }

    void DumpToSRT(const Caption& caption) {
        ofs_ << srt_index_ << std::endl;
        ofs_ << MillisecondsToTime(caption.pts) << " --> ";
        ofs_ << MillisecondsToTime(caption.pts + caption.wait_duration) << std::endl;
        ofs_ << caption.text << std::endl << std::endl;
        srt_index_++;
    }

    bool ConvertCaptionPacket(AVPacket* packet) {
        DecodeResult decode_result;

        auto status = decoder_.Decode(packet->data, packet->size, packet->pts, decode_result);

        if (status == DecodeStatus::kError) {
            fprintf(stderr, "Decoder::Decode() returned error\n");
            return false;
        } else if (status == DecodeStatus::kNoCaption) {
            return true;
        }

        std::unique_ptr<Caption> caption = std::move(decode_result.caption);

        if (caption->wait_duration == DURATION_INDEFINITE) {
            printf("[%.3lfs][INDEFINITE] %s\n",
                   (double)caption->pts / 1000.0f,
                   caption->text.c_str());
        } else {
            printf("[%.3lfs][%.7lfs] %s\n",
                   (double)caption->pts / 1000.0f,
                   (double)caption->wait_duration / 1000.0f,
                   caption->text.c_str());
        }
        fflush(stdout);

        if (!caption_queue_.empty()) {
            Caption& prev = caption_queue_.back();
            if (prev.wait_duration == DURATION_INDEFINITE) {
                prev.wait_duration = caption->pts - prev.pts - 1;
            }
        }
        caption_queue_.push_back(std::move(*caption));

        while (!caption_queue_.empty() && caption_queue_.front().wait_duration != DURATION_INDEFINITE) {
            Caption& cap = caption_queue_.front();
            if (cap.text.empty()) {
                caption_queue_.pop_front();
                continue;
            }

            DumpToSRT(cap);
            caption_queue_.pop_front();
        }

        return true;
    }
   private:
    AVFormatContext* format_context_ = nullptr;
    int video_stream_index_ = -1;
    int arib_caption_index_ = -1;

    Context context_;
    Decoder decoder_;

    std::deque<Caption> caption_queue_;
    std::ofstream ofs_;

    int srt_index_ = 1;
};

int main(int argc, const char* argv[]) {
#ifdef _WIN32
    UTF8CodePage enable_utf8_console;
#endif

    if (argc < 3) {
        printf("Usage: %s [MPEG-TS INPUT] [SRT OUTPUT] \n\n", argv[0]);
        return -1;
    }

    CaptionConverter converter;

    if (!converter.Open(argv[1], argv[2])) {
        fprintf(stderr, "Open input MPEG-TS failed\n");
        return -1;
    }

    converter.RunLoop();
    return 0;
}
