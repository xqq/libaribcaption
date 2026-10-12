/*
* Copyright (C) 2026 magicxqq <xqq@xqq.im>. All rights reserved.
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

// Verifies that the dispatched (SIMD) alphablend kernels are bit-identical to the generic ones,
// and prints a digest of every kernel's output for comparison across platforms.

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "renderer/alphablend.hpp"

using namespace aribcaption;

namespace {

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
constexpr const char* kDispatchedPath = "SSE2";
#else
constexpr const char* kDispatchedPath = "generic";
#endif

constexpr size_t kLineWidth = 256;
constexpr size_t kMaxFuzzWidth = 20;
constexpr size_t kMaxMisalignment = 4;
constexpr uint64_t kFuzzPixels = 10'000'000;
constexpr uint8_t kSampledAlphas[] = {0, 1, 2, 64, 127, 128, 129, 200, 254, 255};

// Deterministic on every platform, unlike std::uniform_int_distribution
class SplitMix64 {
public:
    explicit SplitMix64(uint64_t seed) : state_(seed) {}

    uint64_t Next() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    uint8_t NextByte() { return static_cast<uint8_t>(Next()); }

    ColorRGBA NextColor() { return ColorRGBA(static_cast<uint32_t>(Next())); }

    ColorRGBA NextPremultipliedColor() {
        ColorRGBA color = NextColor();
        return ColorRGBA(static_cast<uint8_t>(color.r * color.a / 255),
                         static_cast<uint8_t>(color.g * color.a / 255),
                         static_cast<uint8_t>(color.b * color.a / 255),
                         color.a);
    }
private:
    uint64_t state_;
};

class Checker {
public:
    explicit Checker(const char* name) : name_(name) {}

    void Check(const ColorRGBA* dispatched, const ColorRGBA* generic,
               const ColorRGBA* dest_before, const ColorRGBA* input, size_t width) {
        for (size_t i = 0; i < width; i++) {
            digest_ = (digest_ ^ dispatched[i].u32) * 0x100000001B3ull;
        }
        pixels_ += width;

        if (memcmp(dispatched, generic, width * sizeof(ColorRGBA)) == 0) {
            return;
        }
        for (size_t i = 0; i < width; i++) {
            if (dispatched[i].u32 == generic[i].u32) {
                continue;
            }
            if (++mismatches_ <= kMaxPrintedMismatches) {
                printf("  %s mismatch: dest=%s input=%s generic=%s %s=%s\n",
                       name_, Format(dest_before[i]).text, Format(input[i]).text,
                       Format(generic[i]).text, kDispatchedPath, Format(dispatched[i]).text);
            }
        }
    }

    bool Report() const {
        printf("%-28s pixels=%-10" PRIu64 " mismatches=%-10" PRIu64 " digest=%016" PRIx64 "\n",
               name_, pixels_, mismatches_, digest_);
        return mismatches_ == 0;
    }
private:
    struct FormattedColor {
        char text[24];
    };

    static FormattedColor Format(ColorRGBA color) {
        FormattedColor formatted{};
        snprintf(formatted.text, sizeof(formatted.text), "(%u,%u,%u,%u)", color.r, color.g, color.b, color.a);
        return formatted;
    }

    static constexpr uint64_t kMaxPrintedMismatches = 5;

    const char* name_;
    uint64_t digest_ = 0xCBF29CE484222325ull;
    uint64_t pixels_ = 0;
    uint64_t mismatches_ = 0;
};

// Runs the generic and the dispatched implementation on copies of the same destination line.
// Lines start at a 16-byte boundary plus `misalignment` pixels, to cover the SIMD prologue paths.
template <typename GenericFunc, typename DispatchedFunc>
void RunLine(Checker& checker, const ColorRGBA* dest, const ColorRGBA* input, size_t width,
             size_t misalignment, GenericFunc generic_func, DispatchedFunc dispatched_func) {
    alignas(16) ColorRGBA generic[kLineWidth + kMaxMisalignment];
    alignas(16) ColorRGBA dispatched[kLineWidth + kMaxMisalignment];

    memcpy(generic + misalignment, dest, width * sizeof(ColorRGBA));
    memcpy(dispatched + misalignment, dest, width * sizeof(ColorRGBA));
    generic_func(generic + misalignment, width);
    dispatched_func(dispatched + misalignment, width);

    checker.Check(dispatched + misalignment, generic + misalignment, dest, input, width);
}

template <typename GenericFunc, typename DispatchedFunc>
void RunLine(Checker& checker, const ColorRGBA* dest, const ColorRGBA* input, size_t width,
             GenericFunc generic_func, DispatchedFunc dispatched_func) {
    RunLine(checker, dest, input, width, 0, generic_func, dispatched_func);
}

// Destination line covering every channel value, with a fixed alpha
void FillDestLine(ColorRGBA* dest, uint8_t alpha) {
    for (size_t i = 0; i < kLineWidth; i++) {
        auto value = static_cast<uint8_t>(i);
        dest[i] = ColorRGBA(value, static_cast<uint8_t>(255 - value), static_cast<uint8_t>(value ^ 0x5A), alpha);
    }
}

void FillPremultipliedDestLine(ColorRGBA* dest, uint8_t alpha) {
    for (size_t i = 0; i < kLineWidth; i++) {
        auto value = static_cast<uint8_t>(i * alpha / 255);
        dest[i] = ColorRGBA(value, static_cast<uint8_t>(alpha - value), static_cast<uint8_t>(value / 2), alpha);
    }
}

bool VerifyFillLine() {
    Checker checker("FillLine");
    SplitMix64 rng(1);
    ColorRGBA dest[kLineWidth];
    ColorRGBA input[kLineWidth];

    for (uint64_t pixels = 0; pixels < kFuzzPixels / 10; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        ColorRGBA color = rng.NextColor();
        for (size_t i = 0; i < width; i++) {
            dest[i] = rng.NextColor();
            input[i] = color;
        }
        RunLine(checker, dest, input, width, misalignment,
                [&](ColorRGBA* line, size_t w) { alphablend::internal::FillLine_Generic(line, color, w); },
                [&](ColorRGBA* line, size_t w) { alphablend::FillLine(line, color, w); });
        pixels += width;
    }

    return checker.Report();
}

bool VerifyFillLineWithAlphas() {
    Checker checker("FillLineWithAlphas");
    SplitMix64 rng(2);
    ColorRGBA dest[kLineWidth] = {};
    ColorRGBA input[kLineWidth];
    uint8_t alphas[kLineWidth];

    for (uint32_t color_alpha = 0; color_alpha < 256; color_alpha++) {
        ColorRGBA color(static_cast<uint8_t>(color_alpha), 0x33, static_cast<uint8_t>(255 - color_alpha),
                        static_cast<uint8_t>(color_alpha));
        for (size_t i = 0; i < kLineWidth; i++) {
            alphas[i] = static_cast<uint8_t>(i);
            input[i] = ColorRGBA(color, alphas[i]);
        }
        RunLine(checker, dest, input, kLineWidth,
                [&](ColorRGBA* line, size_t w) {
                    alphablend::internal::FillLineWithAlphas_Generic(line, alphas, color, w);
                },
                [&](ColorRGBA* line, size_t w) { alphablend::FillLineWithAlphas(line, alphas, color, w); });
    }

    for (uint64_t pixels = 0; pixels < kFuzzPixels; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        ColorRGBA color = rng.NextColor();
        for (size_t i = 0; i < width; i++) {
            alphas[i] = rng.NextByte();
            input[i] = ColorRGBA(color, alphas[i]);
        }
        RunLine(checker, dest, input, width, misalignment,
                [&](ColorRGBA* line, size_t w) {
                    alphablend::internal::FillLineWithAlphas_Generic(line, alphas, color, w);
                },
                [&](ColorRGBA* line, size_t w) { alphablend::FillLineWithAlphas(line, alphas, color, w); });
        pixels += width;
    }

    return checker.Report();
}

bool VerifyBlendColorToLine() {
    Checker checker("BlendColorToLine");
    SplitMix64 rng(3);
    ColorRGBA dest[kLineWidth];
    ColorRGBA input[kLineWidth];

    for (uint8_t dest_alpha : kSampledAlphas) {
        FillDestLine(dest, dest_alpha);
        for (uint32_t alpha = 0; alpha < 256; alpha++) {
            for (uint32_t value = 0; value < 256; value++) {
                ColorRGBA color(static_cast<uint8_t>(value), static_cast<uint8_t>(255 - value),
                                static_cast<uint8_t>(value ^ 0xA5), static_cast<uint8_t>(alpha));
                for (size_t i = 0; i < kLineWidth; i++) {
                    input[i] = color;
                }
                RunLine(checker, dest, input, kLineWidth,
                        [&](ColorRGBA* line, size_t w) {
                            alphablend::internal::BlendColorToLine_Generic(line, color, w);
                        },
                        [&](ColorRGBA* line, size_t w) { alphablend::BlendColorToLine(line, color, w); });
            }
        }
    }

    for (uint64_t pixels = 0; pixels < kFuzzPixels; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        ColorRGBA color = rng.NextColor();
        for (size_t i = 0; i < width; i++) {
            dest[i] = rng.NextColor();
            input[i] = color;
        }
        RunLine(checker, dest, input, width, misalignment,
                [&](ColorRGBA* line, size_t w) { alphablend::internal::BlendColorToLine_Generic(line, color, w); },
                [&](ColorRGBA* line, size_t w) { alphablend::BlendColorToLine(line, color, w); });
        pixels += width;
    }

    return checker.Report();
}

bool VerifyBlendLine() {
    Checker checker("BlendLine");
    SplitMix64 rng(4);
    ColorRGBA dest[kLineWidth];
    ColorRGBA src[kLineWidth];

    for (uint8_t dest_alpha : kSampledAlphas) {
        FillDestLine(dest, dest_alpha);
        for (uint32_t alpha = 0; alpha < 256; alpha++) {
            for (uint32_t value = 0; value < 256; value++) {
                ColorRGBA color(static_cast<uint8_t>(value), static_cast<uint8_t>(255 - value),
                                static_cast<uint8_t>(value ^ 0xA5), static_cast<uint8_t>(alpha));
                for (size_t i = 0; i < kLineWidth; i++) {
                    src[i] = color;
                }
                RunLine(checker, dest, src, kLineWidth,
                        [&](ColorRGBA* line, size_t w) { alphablend::internal::BlendLine_Generic(line, src, w); },
                        [&](ColorRGBA* line, size_t w) { alphablend::BlendLine(line, src, w); });
            }
        }
    }

    for (uint64_t pixels = 0; pixels < kFuzzPixels; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        for (size_t i = 0; i < width; i++) {
            dest[i] = rng.NextColor();
            src[i] = rng.NextColor();
        }
        RunLine(checker, dest, src, width, misalignment,
                [&](ColorRGBA* line, size_t w) { alphablend::internal::BlendLine_Generic(line, src, w); },
                [&](ColorRGBA* line, size_t w) { alphablend::BlendLine(line, src, w); });
        pixels += width;
    }

    return checker.Report();
}

bool VerifyBlendLinePremultipliedSrc() {
    Checker checker("BlendLine_PremultipliedSrc");
    SplitMix64 rng(5);
    ColorRGBA dest[kLineWidth];
    ColorRGBA src[kLineWidth];

    for (uint8_t dest_alpha : kSampledAlphas) {
        FillPremultipliedDestLine(dest, dest_alpha);
        for (uint32_t alpha = 0; alpha < 256; alpha++) {
            for (uint32_t value = 0; value <= alpha; value++) {
                ColorRGBA color(static_cast<uint8_t>(value), static_cast<uint8_t>(alpha - value),
                                static_cast<uint8_t>(value / 2), static_cast<uint8_t>(alpha));
                for (size_t i = 0; i < kLineWidth; i++) {
                    src[i] = color;
                }
                RunLine(checker, dest, src, kLineWidth,
                        [&](ColorRGBA* line, size_t w) {
                            alphablend::internal::BlendLine_PremultipliedSrc_Generic(line, src, w);
                        },
                        [&](ColorRGBA* line, size_t w) { alphablend::BlendLine_PremultipliedSrc(line, src, w); });
            }
        }
    }

    for (uint64_t pixels = 0; pixels < kFuzzPixels; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        for (size_t i = 0; i < width; i++) {
            dest[i] = rng.NextPremultipliedColor();
            src[i] = rng.NextPremultipliedColor();
        }
        RunLine(checker, dest, src, width, misalignment,
                [&](ColorRGBA* line, size_t w) {
                    alphablend::internal::BlendLine_PremultipliedSrc_Generic(line, src, w);
                },
                [&](ColorRGBA* line, size_t w) { alphablend::BlendLine_PremultipliedSrc(line, src, w); });
        pixels += width;
    }

    return checker.Report();
}

}  // namespace

int main(int argc, char** argv) {
    printf("Comparing %s kernels against generic kernels\n", kDispatchedPath);

    bool passed = true;
    passed &= VerifyFillLine();
    passed &= VerifyFillLineWithAlphas();
    passed &= VerifyBlendColorToLine();
    passed &= VerifyBlendLine();
    passed &= VerifyBlendLinePremultipliedSrc();

    printf("%s\n", passed ? "PASSED" : "FAILED");
    return passed ? 0 : 1;
}
