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

struct FormattedColor {
    char text[24];
};

FormattedColor Format(ColorRGBA color) {
    FormattedColor formatted{};
    snprintf(formatted.text, sizeof(formatted.text), "(%u,%u,%u,%u)", color.r, color.g, color.b, color.a);
    return formatted;
}

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
    static constexpr uint64_t kMaxPrintedMismatches = 5;

    const char* name_;
    uint64_t digest_ = 0xCBF29CE484222325ull;
    uint64_t pixels_ = 0;
    uint64_t mismatches_ = 0;
};

class InvariantChecker {
public:
    explicit InvariantChecker(const char* name) : name_(name) {}

    void Expect(bool condition, const char* invariant, ColorRGBA dest, ColorRGBA input, ColorRGBA output) {
        checks_++;
        if (condition) {
            return;
        }
        if (++failures_ <= kMaxPrintedFailures) {
            printf("  %s violates \"%s\": dest=%s input=%s output=%s\n",
                   name_, invariant, Format(dest).text, Format(input).text, Format(output).text);
        }
    }

    bool Report() const {
        printf("%-28s checks=%-10" PRIu64 " failures=%-10" PRIu64 "\n", name_, checks_, failures_);
        return failures_ == 0;
    }
private:
    static constexpr uint64_t kMaxPrintedFailures = 5;

    const char* name_;
    uint64_t checks_ = 0;
    uint64_t failures_ = 0;
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

bool VerifyUnpremultiplyLine() {
    Checker checker("UnpremultiplyLine");
    SplitMix64 rng(6);
    ColorRGBA dest[kLineWidth];

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        FillDestLine(dest, static_cast<uint8_t>(alpha));
        RunLine(checker, dest, dest, kLineWidth,
                [](ColorRGBA* line, size_t w) { alphablend::internal::UnpremultiplyLine_Generic(line, w); },
                [](ColorRGBA* line, size_t w) { alphablend::UnpremultiplyLine(line, w); });
    }

    for (uint64_t pixels = 0; pixels < kFuzzPixels; ) {
        size_t width = rng.Next() % kMaxFuzzWidth;
        size_t misalignment = rng.Next() % kMaxMisalignment;
        for (size_t i = 0; i < width; i++) {
            dest[i] = rng.NextColor();
        }
        RunLine(checker, dest, dest, width, misalignment,
                [](ColorRGBA* line, size_t w) { alphablend::internal::UnpremultiplyLine_Generic(line, w); },
                [](ColorRGBA* line, size_t w) { alphablend::UnpremultiplyLine(line, w); });
        pixels += width;
    }

    return checker.Report();
}

constexpr uint8_t kSampledValues[] = {0, 1, 127, 128, 254, 255};

bool IsValidPremultiplied(ColorRGBA color) {
    return color.r <= color.a && color.g <= color.a && color.b <= color.a;
}

bool VerifyFillLineWithAlphasInvariants() {
    InvariantChecker checker("FillLineWithAlphas");
    const uint8_t alphas[] = {0, 255};
    ColorRGBA dest[2];

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        for (uint8_t value : kSampledValues) {
            ColorRGBA color(value, static_cast<uint8_t>(255 - value), static_cast<uint8_t>(value / 2),
                            static_cast<uint8_t>(alpha));
            alphablend::FillLineWithAlphas(dest, alphas, color, 2);
            checker.Expect(dest[0].a == 0, "zero coverage is transparent", ColorRGBA(), color, dest[0]);
            checker.Expect(dest[1].a == color.a, "full coverage keeps color alpha", ColorRGBA(), color, dest[1]);
        }
    }

    return checker.Report();
}

// Checks a blend of `src` over every valid premultiplied destination pixel.
// `src_rgb_out` is what an opaque src contributes to the output.
template <typename BlendFunc>
void CheckBlendInvariants(InvariantChecker& checker, ColorRGBA src, ColorRGBA src_rgb_out, BlendFunc blend_func) {
    ColorRGBA dest[kLineWidth];
    ColorRGBA output[kLineWidth];

    for (uint32_t dest_alpha = 0; dest_alpha < 256; dest_alpha++) {
        FillPremultipliedDestLine(dest, static_cast<uint8_t>(dest_alpha));
        memcpy(output, dest, sizeof(dest));
        blend_func(output, src, kLineWidth);

        for (size_t i = 0; i < kLineWidth; i++) {
            if (src.a == 0) {
                checker.Expect(output[i].u32 == dest[i].u32, "transparent src keeps dest", dest[i], src, output[i]);
            } else if (src.a == 255) {
                checker.Expect(output[i].u32 == ColorRGBA(src_rgb_out, 255).u32, "opaque src replaces dest",
                               dest[i], src, output[i]);
            }
            if (dest[i].a == 255) {
                checker.Expect(output[i].a == 255, "opaque dest stays opaque", dest[i], src, output[i]);
            }
            checker.Expect(IsValidPremultiplied(output[i]), "output stays premultiplied", dest[i], src, output[i]);
        }
    }
}

bool VerifyBlendColorToLineInvariants() {
    InvariantChecker checker("BlendColorToLine");

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        for (uint8_t value : kSampledValues) {
            ColorRGBA src(value, static_cast<uint8_t>(255 - value), static_cast<uint8_t>(value / 2),
                          static_cast<uint8_t>(alpha));
            CheckBlendInvariants(checker, src, src, [](ColorRGBA* line, ColorRGBA color, size_t width) {
                alphablend::BlendColorToLine(line, color, width);
            });
        }
    }

    return checker.Report();
}

bool VerifyBlendLineInvariants() {
    InvariantChecker checker("BlendLine");
    ColorRGBA src_line[kLineWidth];

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        for (uint8_t value : kSampledValues) {
            ColorRGBA src(value, static_cast<uint8_t>(255 - value), static_cast<uint8_t>(value / 2),
                          static_cast<uint8_t>(alpha));
            CheckBlendInvariants(checker, src, src, [&](ColorRGBA* line, ColorRGBA color, size_t width) {
                for (size_t i = 0; i < width; i++) {
                    src_line[i] = color;
                }
                alphablend::BlendLine(line, src_line, width);
            });
        }
    }

    return checker.Report();
}

bool VerifyBlendLinePremultipliedSrcInvariants() {
    InvariantChecker checker("BlendLine_PremultipliedSrc");
    ColorRGBA src_line[kLineWidth];

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        for (uint8_t value : kSampledValues) {
            auto premultiplied = static_cast<uint8_t>(value * alpha / 255);
            ColorRGBA src(premultiplied, static_cast<uint8_t>(alpha - premultiplied),
                          static_cast<uint8_t>(premultiplied / 2), static_cast<uint8_t>(alpha));
            CheckBlendInvariants(checker, src, src, [&](ColorRGBA* line, ColorRGBA color, size_t width) {
                for (size_t i = 0; i < width; i++) {
                    src_line[i] = color;
                }
                alphablend::BlendLine_PremultipliedSrc(line, src_line, width);
            });
        }
    }

    return checker.Report();
}

bool VerifyUnpremultiplyLineInvariants() {
    InvariantChecker checker("UnpremultiplyLine");
    ColorRGBA line[kLineWidth];

    for (uint32_t alpha = 0; alpha < 256; alpha++) {
        for (uint32_t value = 0; value <= alpha; value++) {
            line[value] = ColorRGBA(static_cast<uint8_t>(value), static_cast<uint8_t>(alpha - value),
                                    static_cast<uint8_t>(value / 2), static_cast<uint8_t>(alpha));
        }
        size_t width = alpha + 1;
        ColorRGBA premultiplied[kLineWidth];
        memcpy(premultiplied, line, width * sizeof(ColorRGBA));
        alphablend::UnpremultiplyLine(line, width);

        for (size_t i = 0; i < width; i++) {
            ColorRGBA input = premultiplied[i];
            ColorRGBA output = line[i];
            checker.Expect(output.a == input.a, "alpha is kept", ColorRGBA(), input, output);
            if (alpha == 0) {
                checker.Expect(output.u32 == 0, "transparent stays zero", ColorRGBA(), input, output);
            }

            const uint8_t input_channels[] = {input.r, input.g, input.b};
            const uint8_t output_channels[] = {output.r, output.g, output.b};
            for (size_t c = 0; c < 3; c++) {
                uint32_t exact = alpha ? (input_channels[c] * 255 + alpha / 2) / alpha : 0;
                uint32_t error = output_channels[c] > exact ? output_channels[c] - exact : exact - output_channels[c];
                checker.Expect(error <= 1, "within 1 of exact", ColorRGBA(), input, output);
            }
        }
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
    passed &= VerifyUnpremultiplyLine();

    printf("Checking invariants of %s kernels\n", kDispatchedPath);
    passed &= VerifyFillLineWithAlphasInvariants();
    passed &= VerifyBlendColorToLineInvariants();
    passed &= VerifyBlendLineInvariants();
    passed &= VerifyBlendLinePremultipliedSrcInvariants();
    passed &= VerifyUnpremultiplyLineInvariants();

    printf("%s\n", passed ? "PASSED" : "FAILED");
    return passed ? 0 : 1;
}
