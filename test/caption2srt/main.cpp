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
    #include <shellapi.h>
    #include "base/wchar_helper.hpp"
#endif

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <cinttypes>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
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
        // On Windows, convert UTF-8 path to native Unicode (UTF-16 std::wstring) for constructing
        errno = 0;
#ifdef _WIN32
        ofs_.open(std::filesystem::path(wchar::UTF8ToWideString(output_filename)));
#else
        ofs_.open(output_filename);
#endif
        if (!ofs_) {
            const int error = errno;
            fprintf(stderr, "Open SRT output failed: %s, output: %s\n",
                    error ? std::strerror(error) : "I/O error", output_filename);
            return false;
        }

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

    bool RunLoop() {
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

        ofs_.close();
        if (!ofs_) {
            fprintf(stderr, "Write or close SRT output failed\n");
            return false;
        }
        return true;
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

#ifdef _WIN32
std::vector<std::string> ParseWin32CommandLineToUTF8() {
    std::vector<std::string> argv_utf8;

    int argc = 0;
    wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wide_argv) {
        fprintf(stderr, "Parse Win32 command line failed\n");
        return {};
    }

    for (int i = 0; i < argc; i++) {
        std::string argument = wchar::WideStringToUTF8(wide_argv[i]);
        argv_utf8.push_back(std::move(argument));
    }

    LocalFree(wide_argv);
    return argv_utf8;
}
#endif  // _WIN32

int main(int argc, const char* argv[]) {
#ifdef _WIN32
    UTF8CodePage enable_utf8_console;
#endif

    if (argc < 3) {
        printf("Usage: %s [MPEG-TS INPUT] [SRT OUTPUT] \n\n", argv[0]);
        return -1;
    }

    const char* input_filename = argv[1];
    const char* output_filename = argv[2];

#ifdef _WIN32
    std::vector<std::string> argv_utf8 = ParseWin32CommandLineToUTF8();
    if (argv_utf8.size() < 3) {
        return -1;
    }

    const std::string& input_filename_utf8 = argv_utf8[1];
    const std::string& output_filename_utf8 = argv_utf8[2];

    input_filename = input_filename_utf8.c_str();
    output_filename = output_filename_utf8.c_str();
#endif  // _WIN32

    CaptionConverter converter;

    if (!converter.Open(input_filename, output_filename)) {
        fprintf(stderr, "Open input or output failed\n");
        return -1;
    }

    if (!converter.RunLoop()) {
        return -1;
    }
    return 0;
}
