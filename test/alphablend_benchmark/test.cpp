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

// Benchmarks the alphablend kernels (generic and dispatched) line by line, as Canvas uses them,
// and a full Renderer::Render() including the share of UnpremultiplyLine().

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>
#include "aribcaption/aribcaption.hpp"
#include "renderer/alphablend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "sample_data.h"
#include "stopwatch.hpp"

using namespace aribcaption;

namespace {

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
constexpr const char* kDispatchedPath = "SSE2";
#else
constexpr const char* kDispatchedPath = "generic";
#endif

struct Size {
    const char* name;
    int width;
    int height;
    int rounds;
};

constexpr Size kSizes[] = {
    {"3840x2160", 3840, 2160, 30},
    {"1920x320", 1920, 320, 300},
};

// Best time of all rounds in milliseconds. prepare() runs before each round and is not timed.
template <typename PrepareFunc, typename RunFunc>
double BestMilliseconds(int rounds, PrepareFunc prepare, RunFunc run) {
    auto stopwatch = StopWatch::Create();
    int64_t best = INT64_MAX;

    for (int i = 0; i < rounds; i++) {
        prepare();
        stopwatch->Reset();
        stopwatch->Start();
        run();
        stopwatch->Stop();
        best = std::min<int64_t>(best, stopwatch->GetMicroseconds());
    }

    return static_cast<double>(best) / 1000.0;
}

// Deterministic on every platform
class SplitMix64 {
public:
    explicit SplitMix64(uint64_t seed) : state_(seed) {}

    uint32_t Next() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return static_cast<uint32_t>(z ^ (z >> 31));
    }

    ColorRGBA NextPremultipliedColor() {
        ColorRGBA color(Next());
        return ColorRGBA(static_cast<uint8_t>(color.r * color.a / 255),
                         static_cast<uint8_t>(color.g * color.a / 255),
                         static_cast<uint8_t>(color.b * color.a / 255),
                         color.a);
    }
private:
    uint64_t state_;
};

struct Buffers {
    int width = 0;
    int height = 0;
    std::vector<ColorRGBA> dest_source;    // premultiplied
    std::vector<ColorRGBA> dest;
    std::vector<ColorRGBA> straight_src;
    std::vector<ColorRGBA> premultiplied_src;
    std::vector<uint8_t> alphas;
    ColorRGBA color = ColorRGBA(200, 120, 40, 160);

    explicit Buffers(const Size& size) : width(size.width), height(size.height) {
        size_t count = static_cast<size_t>(width) * height;
        SplitMix64 rng(1);
        dest_source.resize(count);
        straight_src.resize(count);
        premultiplied_src.resize(count);
        alphas.resize(count);
        for (size_t i = 0; i < count; i++) {
            dest_source[i] = rng.NextPremultipliedColor();
            straight_src[i] = ColorRGBA(rng.Next());
            premultiplied_src[i] = rng.NextPremultipliedColor();
            alphas[i] = static_cast<uint8_t>(rng.Next());
        }
        dest = dest_source;
    }

    void ResetDest() {
        memcpy(dest.data(), dest_source.data(), dest.size() * sizeof(ColorRGBA));
    }

    ColorRGBA* DestLine(int y) { return dest.data() + static_cast<size_t>(y) * width; }

    template <typename T>
    const T* Line(const std::vector<T>& buffer, int y) { return buffer.data() + static_cast<size_t>(y) * width; }
};

using LineFunc = void (*)(Buffers& buffers, int y);

struct Kernel {
    const char* name;
    LineFunc generic;
    LineFunc dispatched;
};

const Kernel kKernels[] = {
    {"FillLineWithAlphas",
     [](Buffers& b, int y) {
         alphablend::internal::FillLineWithAlphas_Generic(b.DestLine(y), b.Line(b.alphas, y), b.color, b.width);
     },
     [](Buffers& b, int y) { alphablend::FillLineWithAlphas(b.DestLine(y), b.Line(b.alphas, y), b.color, b.width); }},
    {"BlendColorToLine",
     [](Buffers& b, int y) { alphablend::internal::BlendColorToLine_Generic(b.DestLine(y), b.color, b.width); },
     [](Buffers& b, int y) { alphablend::BlendColorToLine(b.DestLine(y), b.color, b.width); }},
    {"BlendLine",
     [](Buffers& b, int y) {
         alphablend::internal::BlendLine_Generic(b.DestLine(y), b.Line(b.straight_src, y), b.width);
     },
     [](Buffers& b, int y) { alphablend::BlendLine(b.DestLine(y), b.Line(b.straight_src, y), b.width); }},
    {"BlendLine_PremultipliedSrc",
     [](Buffers& b, int y) {
         alphablend::internal::BlendLine_PremultipliedSrc_Generic(b.DestLine(y), b.Line(b.premultiplied_src, y),
                                                                  b.width);
     },
     [](Buffers& b, int y) {
         alphablend::BlendLine_PremultipliedSrc(b.DestLine(y), b.Line(b.premultiplied_src, y), b.width);
     }},
    {"UnpremultiplyLine",
     [](Buffers& b, int y) { alphablend::internal::UnpremultiplyLine_Generic(b.DestLine(y), b.width); },
     [](Buffers& b, int y) { alphablend::UnpremultiplyLine(b.DestLine(y), b.width); }},
};

double BenchmarkLines(Buffers& buffers, int rounds, LineFunc func) {
    return BestMilliseconds(rounds, [&] { buffers.ResetDest(); }, [&] {
        for (int y = 0; y < buffers.height; y++) {
            func(buffers, y);
        }
    });
}

void BenchmarkKernels() {
    printf("%-28s %-10s %12s %12s %8s\n", "kernel", "size", "generic ms", kDispatchedPath, "speedup");
    for (const Size& size : kSizes) {
        Buffers buffers(size);
        for (const Kernel& kernel : kKernels) {
            double generic = BenchmarkLines(buffers, size.rounds, kernel.generic);
            double dispatched = BenchmarkLines(buffers, size.rounds, kernel.dispatched);
            printf("%-28s %-10s %12.3f %12.3f %7.2fx\n", kernel.name, size.name, generic, dispatched,
                   dispatched > 0 ? generic / dispatched : 0.0);
        }
    }
}

// The original benchmark: Canvas::DrawBitmap() of a 4K bitmap, kept for comparison with earlier results
void BenchmarkCanvasDrawBitmap() {
    Bitmap background(3840, 2160, PixelFormat::kRGBA8888);
    Canvas canvas(background);
    canvas.ClearColor(ColorRGBA(255, 0, 0, 180));

    Bitmap foreground(3840, 2160, PixelFormat::kRGBA8888);
    Canvas fg_canvas(foreground);
    fg_canvas.ClearColor(ColorRGBA(0, 255, 0, 128));

    double average = 0;
    constexpr int count = 100;
    auto stopwatch = StopWatch::Create();
    stopwatch->Start();
    for (int i = 0; i < count; i++) {
        canvas.DrawBitmap(foreground, 0, 0);
    }
    stopwatch->Stop();
    average = static_cast<double>(stopwatch->GetMicroseconds()) / 1000.0 / count;

    printf("Canvas::DrawBitmap 3840x2160: %.3f ms average of %d\n", average, count);
}

double UnpremultiplyMilliseconds(const std::vector<Image>& images, int rounds) {
    std::vector<std::vector<uint8_t>> copies;
    for (const Image& image : images) {
        copies.emplace_back(image.bitmap.begin(), image.bitmap.end());
    }

    return BestMilliseconds(rounds, [&] {
        for (size_t i = 0; i < images.size(); i++) {
            std::copy(images[i].bitmap.begin(), images[i].bitmap.end(), copies[i].begin());
        }
    }, [&] {
        for (size_t i = 0; i < images.size(); i++) {
            for (int y = 0; y < images[i].height; y++) {
                auto line = reinterpret_cast<ColorRGBA*>(copies[i].data() + static_cast<size_t>(y) * images[i].stride);
                alphablend::UnpremultiplyLine(line, static_cast<size_t>(images[i].width));
            }
        }
    });
}

void BenchmarkRender() {
    Context context;
    Decoder decoder(context);
    decoder.Initialize();

    DecodeResult decode_result;
    if (decoder.Decode(sample_data_drcs_1, sizeof(sample_data_drcs_1), 0, decode_result) != DecodeStatus::kGotCaption) {
        fprintf(stderr, "Decoder::Decode() failed\n");
        return;
    }
    Caption& caption = *decode_result.caption;
    caption.iso6392_language_code = ThreeCC("jpn");

    constexpr Size kFrames[] = {
        {"1920x1080", 1920, 1080, 30},
        {"3840x2160", 3840, 2160, 30},
    };

    printf("%-10s %7s %10s %10s %16s %7s\n", "frame", "images", "pixels", "Render ms", "Unpremultiply ms", "share");
    for (const Size& frame : kFrames) {
        Renderer renderer(context);
        renderer.Initialize();
        renderer.SetFrameSize(frame.width, frame.height);

        RenderResult render_result;
        renderer.AppendCaption(caption);
        renderer.Render(0, render_result);  // Warm up font loading and caches

        int not_rendered = 0;
        double render = BestMilliseconds(frame.rounds, [&] {
            renderer.Flush();
            renderer.AppendCaption(caption);
        }, [&] {
            not_rendered += renderer.Render(0, render_result) != RenderStatus::kGotImage;
        });
        if (not_rendered) {
            fprintf(stderr, "Renderer::Render() did not render %d times\n", not_rendered);
        }

        size_t pixels = 0;
        for (const Image& image : render_result.images) {
            pixels += static_cast<size_t>(image.width) * image.height;
        }
        double unpremultiply = UnpremultiplyMilliseconds(render_result.images, frame.rounds * 10);

        printf("%-10s %7zu %10zu %10.3f %16.3f %6.2f%%\n", frame.name, render_result.images.size(), pixels,
               render, unpremultiply, render > 0 ? unpremultiply / render * 100.0 : 0.0);
    }
}

}  // namespace

int main(int argc, char** argv) {
    BenchmarkKernels();
    printf("\n");
    BenchmarkCanvasDrawBitmap();
    printf("\n");
    BenchmarkRender();

    return 0;
}
