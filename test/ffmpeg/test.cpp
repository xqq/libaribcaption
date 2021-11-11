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

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <memory>
#include "aribcaption/aribcaption.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "png_writer.hpp"

using namespace aribcaption;

constexpr int frame_area_width = 1920;
constexpr int frame_area_height = 1080;
constexpr int margin_left = 0;
constexpr int margin_top = 0;
constexpr int margin_right = 0;
constexpr int margin_bottom = 0;

class CaptionDecodeRendererFFmpeg {
public:
    explicit CaptionDecodeRendererFFmpeg()
        : aribcc_decoder_(aribcc_context_), aribcc_renderer_(aribcc_context_) {}

    ~CaptionDecodeRendererFFmpeg() {
        if (format_context_) {
            avformat_close_input(&format_context_);
        }
    }
public:
    bool Open(const char* input_filename) {
        InitCaptionDecoderRenderer();

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

            if (codec_params->codec_type == AVMEDIA_TYPE_SUBTITLE && codec_params->codec_id == AV_CODEC_ID_ARIB_CAPTION) {
                // if (!(stream->disposition & AV_DISPOSITION_URGENT)) {
                arib_caption_index_ = stream->index;
                break;
                // }
            }
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
        AVPacket packet{};

        while ((ret = av_read_frame(format_context_, &packet) == 0)) {
            if (packet.stream_index == arib_caption_index_) {
                AVStream* stream = format_context_->streams[arib_caption_index_];
                av_packet_rescale_ts(&packet, stream->time_base, AVRational{1, 1000});
                // packet.pts = superimpose_pts_++;
                DecodeRenderAndSave(&packet);
            }
            av_packet_unref(&packet);
        }
    }
private:
    void InitCaptionDecoderRenderer() {
        aribcc_context_.SetLogcatCallback([](LogLevel level, const char* message) {
            if (level == LogLevel::kError) {
                fprintf(stderr, "%s\n", message);
            } else {
                printf("%s\n", message);
            }
        });

        aribcc_decoder_.Initialize(B24Type::kCaption);

        aribcc_renderer_.Initialize();
        // aribcc_renderer_.SetForceStrokeText(true);
        aribcc_renderer_.SetFrameSize(frame_area_width, frame_area_height);
        aribcc_renderer_.SetMargins(margin_top, margin_bottom, margin_left, margin_right);
        // aribcc_renderer_.SetLanguageSpecificFontFamily(ThreeCC("jpn"), {"Rounded M+ 1m for ARIB"});
    }

    bool DecodeRenderAndSave(AVPacket* packet) {
        std::unique_ptr<Caption> caption;

        auto status = aribcc_decoder_.Decode(packet->data, packet->size, packet->pts, [&](auto cap) -> void {
            caption = std::move(cap);
        });

        if (status == Decoder::kDecodeStatusGotCaption) {
            printf("%s\n", caption->text.c_str());
            if (caption->iso6392_language_code == 0) {
                caption->iso6392_language_code = ThreeCC("jpn");
            }
            aribcc_renderer_.AppendCaption(*caption);
        } else if (status == Decoder::kDecodeStatusError) {
            fprintf(stderr, "Decoder::Decode() returned error\n");
            return false;
        }

        if (!caption) {
            return true;
        }

        std::vector<Image> images;

        auto render_status = aribcc_renderer_.Render(packet->pts, [&](int64_t pts, int64_t duration, auto& imgs) {
            printf("Render: pts = %" PRId64 ", duration = %" PRId64 "\n", pts, duration);
            images = imgs;
        });

        if (render_status == Renderer::kRenderStatusError) {
            fprintf(stderr, "Renderer::Render() returned error\n");
            return false;
        }

        Bitmap screen_bmp(frame_area_width, frame_area_height, PixelFormat::kRGBA8888);
        Canvas screen_canvas(screen_bmp);

        for (Image& img : images) {
            int dst_x = img.dst_x;
            int dst_y = img.dst_y;
            Bitmap bmp = Bitmap::FromImage(std::move(img));
            Rect region_rect(dst_x,
                             dst_y,
                             dst_x + bmp.width(),
                             dst_y + bmp.height());
            screen_canvas.DrawBitmap(bmp, region_rect);
        }

        std::string filename("test_ffmpeg_output_");
        filename.append(std::to_string(packet->pts));
        filename.append(".png");
        png_writer_write_bitmap(filename.c_str(), screen_bmp);
        return true;
    }
private:
    AVFormatContext* format_context_ = nullptr;
    int arib_caption_index_ = -1;
    // int64_t superimpose_pts_ = 0;
    Context aribcc_context_;
    Decoder aribcc_decoder_;
    Renderer aribcc_renderer_;
};

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s input \n\n", argv[0]);
        return -1;
    }

    CaptionDecodeRendererFFmpeg decode_renderer;

    if (!decode_renderer.Open(argv[1])) {
        fprintf(stderr, "Open() failed\n");
        return -1;
    }

    decode_renderer.RunLoop();
    return 0;
}
