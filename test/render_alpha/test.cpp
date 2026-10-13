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

// Renders synthetic captions through the public Renderer API and checks that output images hold
// straight (non-premultiplied) alpha, with and without merged region images.

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>
#include "aribcaption/aribcaption.hpp"

using namespace aribcaption;

namespace {

constexpr int kPlaneWidth = 960;
constexpr int kPlaneHeight = 540;
constexpr int kCharSize = 36;
constexpr int kHorizontalSpacing = 4;
constexpr int kVerticalSpacing = 24;
constexpr int kSectionWidth = kCharSize + kHorizontalSpacing;
constexpr int kSectionHeight = kCharSize + kVerticalSpacing;
constexpr int kRegionX = 60;
constexpr int kRegionY = 30;
constexpr int kRegionGap = 10;

constexpr uint32_t kBlackSquare = 0x25A0;  // A glyph with a solid interior
constexpr uint32_t kCodepoints[] = {kBlackSquare, 0x3042, 0x5B57, kBlackSquare};
constexpr int kCharsPerRegion = static_cast<int>(std::size(kCodepoints));
constexpr uint32_t kDRCSCode = 1;

constexpr ColorRGBA kWhite(255, 255, 255, 255);
constexpr ColorRGBA kBlack(0, 0, 0, 255);
constexpr ColorRGBA kTranslucentBlue(0, 0, 255, 128);
constexpr ColorRGBA kTransparent(0, 0, 0, 0);

enum class RegionKind {
    kTranslucentBox,
    kStrokeAndUnderline,
    kOpaqueBox,
    kNoBox,
    kDRCS,
};

struct RegionSpec {
    RegionKind kind;
    const char* name;
    ColorRGBA back_color;
    CharStyle style;
};

constexpr RegionSpec kRegions[] = {
    {RegionKind::kTranslucentBox, "text over translucent box", kTranslucentBlue, CharStyle::kCharStyleDefault},
    {RegionKind::kStrokeAndUnderline, "stroked, underlined text over translucent box", kTranslucentBlue,
     static_cast<CharStyle>(CharStyle::kCharStyleStroke | CharStyle::kCharStyleUnderline)},
    {RegionKind::kOpaqueBox, "text over opaque box", kBlack, CharStyle::kCharStyleDefault},
    {RegionKind::kNoBox, "text without box", kTransparent, CharStyle::kCharStyleDefault},
    {RegionKind::kDRCS, "stroked DRCS over translucent box", kTranslucentBlue, CharStyle::kCharStyleStroke},
};

int RegionTop(size_t region_index) {
    return kRegionY + static_cast<int>(region_index) * (kSectionHeight + kRegionGap);
}

// A 1-bit DRCS with a solid square in the middle
DRCS MakeSquareDRCS() {
    DRCS drcs;
    drcs.width = kCharSize;
    drcs.height = kCharSize;
    drcs.depth = 2;
    drcs.depth_bits = 1;
    drcs.pixels.resize(kCharSize * kCharSize / 8);

    for (int y = 6; y < kCharSize - 6; y++) {
        for (int x = 6; x < kCharSize - 6; x++) {
            int index = y * kCharSize + x;
            drcs.pixels[index / 8] |= static_cast<uint8_t>(0x80 >> (index % 8));
        }
    }

    return drcs;
}

Caption MakeCaption() {
    Caption caption;
    caption.type = CaptionType::kCaption;
    caption.iso6392_language_code = ThreeCC("jpn");
    caption.plane_width = kPlaneWidth;
    caption.plane_height = kPlaneHeight;
    caption.pts = 0;
    caption.wait_duration = DURATION_INDEFINITE;
    caption.drcs_map[kDRCSCode] = MakeSquareDRCS();

    for (size_t r = 0; r < std::size(kRegions); r++) {
        const RegionSpec& spec = kRegions[r];

        CaptionRegion region;
        region.x = kRegionX;
        region.y = RegionTop(r);
        region.width = kCharsPerRegion * kSectionWidth;
        region.height = kSectionHeight;

        for (int i = 0; i < kCharsPerRegion; i++) {
            CaptionChar ch;
            ch.type = spec.kind == RegionKind::kDRCS ? CaptionCharType::kDRCS : CaptionCharType::kText;
            ch.codepoint = kCodepoints[i];
            ch.drcs_code = kDRCSCode;
            ch.x = region.x + i * kSectionWidth;
            ch.y = region.y;
            ch.char_width = kCharSize;
            ch.char_height = kCharSize;
            ch.char_horizontal_spacing = kHorizontalSpacing;
            ch.char_vertical_spacing = kVerticalSpacing;
            ch.char_horizontal_scale = 1.0f;
            ch.char_vertical_scale = 1.0f;
            ch.text_color = kWhite;
            ch.back_color = spec.back_color;
            ch.stroke_color = kBlack;
            ch.style = spec.style;
            region.chars.push_back(ch);
        }

        caption.regions.push_back(std::move(region));
    }

    return caption;
}

// All output images composed into one plane-sized frame
struct Frame {
    std::vector<ColorRGBA> pixels = std::vector<ColorRGBA>(kPlaneWidth * kPlaneHeight);

    [[nodiscard]]
    ColorRGBA At(int x, int y) const {
        return pixels[y * kPlaneWidth + x];
    }
};

bool ComposeFrame(const std::vector<Image>& images, Frame& frame) {
    for (const Image& image : images) {
        if (image.dst_x < 0 || image.dst_y < 0 ||
            image.dst_x + image.width > kPlaneWidth || image.dst_y + image.height > kPlaneHeight) {
            fprintf(stderr, "Image at (%d, %d) %dx%d is out of the frame\n",
                    image.dst_x, image.dst_y, image.width, image.height);
            return false;
        }
        for (int y = 0; y < image.height; y++) {
            memcpy(&frame.pixels[(image.dst_y + y) * kPlaneWidth + image.dst_x],
                   image.bitmap.data() + y * image.stride,
                   image.width * sizeof(ColorRGBA));
        }
    }
    return true;
}

class Checker {
public:
    void Expect(bool condition, const char* expectation, int x, int y, ColorRGBA actual) {
        checks_++;
        if (condition) {
            return;
        }
        if (++failures_ <= kMaxPrintedFailures) {
            printf("    expected %s at (%d, %d), got (%u,%u,%u,%u)\n",
                   expectation, x, y, actual.r, actual.g, actual.b, actual.a);
        }
    }

    [[nodiscard]]
    uint64_t checks() const { return checks_; }

    [[nodiscard]]
    uint64_t failures() const { return failures_; }
private:
    static constexpr uint64_t kMaxPrintedFailures = 8;

    uint64_t checks_ = 0;
    uint64_t failures_ = 0;
};

// White text with coverage c over the translucent blue box composites to alpha a = c + (1 - c) * 128 / 255.
// Straight output must then be r = g = 255 * c / a, and b = 255.
void CheckTranslucentBoxPixel(Checker& checker, int x, int y, ColorRGBA pixel) {
    if (pixel.a < kTranslucentBlue.a) {
        checker.Expect(false, "at least the box alpha", x, y, pixel);
        return;
    }

    auto expected_rgb = [](double alpha) {
        double coverage = (alpha - kTranslucentBlue.a) / (255.0 - kTranslucentBlue.a);
        return 255.0 * 255.0 * coverage / alpha;
    };
    // Accept the colors of alpha +-0.5, widened by the rounding of the premultiplied color (0.5 * 255 / alpha)
    // and by unpremultiplying (1.5, as UnpremultiplyLine() is within 1 of exact rounding)
    double tolerance = 0.5 * 255.0 / pixel.a + 1.5;
    double lower = expected_rgb(std::max<double>(pixel.a - 0.5, kTranslucentBlue.a)) - tolerance;
    double upper = expected_rgb(std::min(pixel.a + 0.5, 255.0)) + tolerance;

    checker.Expect(pixel.r >= lower && pixel.r <= upper && std::abs(pixel.r - pixel.g) <= 1 && pixel.b >= 254,
                   "straight white over the box", x, y, pixel);
}

void CheckRegion(Checker& checker, const Frame& frame, size_t region_index) {
    const RegionSpec& spec = kRegions[region_index];
    int top = RegionTop(region_index);

    for (int i = 0; i < kCharsPerRegion; i++) {
        int section_x = kRegionX + i * kSectionWidth;

        // The top-left corner of a section lies in the spacing, so it only has the box
        ColorRGBA corner = frame.At(section_x + 1, top + 1);
        checker.Expect(corner.u32 == spec.back_color.u32, "the exact box color", section_x + 1, top + 1, corner);

        if (spec.kind == RegionKind::kDRCS || kCodepoints[i] == kBlackSquare) {
            int center_x = section_x + kSectionWidth / 2;
            int center_y = top + kSectionHeight / 2;
            ColorRGBA center = frame.At(center_x, center_y);
            checker.Expect(center.u32 == kWhite.u32, "the exact text color", center_x, center_y, center);
        }
    }

    for (int y = top; y < top + kSectionHeight; y++) {
        for (int x = kRegionX; x < kRegionX + kCharsPerRegion * kSectionWidth; x++) {
            ColorRGBA pixel = frame.At(x, y);
            switch (spec.kind) {
                case RegionKind::kTranslucentBox:
                    CheckTranslucentBoxPixel(checker, x, y, pixel);
                    break;
                case RegionKind::kNoBox:
                    checker.Expect(pixel.a == 0 ? pixel.u32 == 0 : (pixel.r >= 254 && pixel.g >= 254 && pixel.b >= 254),
                                   "transparent, or straight white", x, y, pixel);
                    break;
                case RegionKind::kOpaqueBox:
                    checker.Expect(pixel.a == 255 && pixel.r == pixel.g && pixel.g == pixel.b,
                                   "opaque grey", x, y, pixel);
                    break;
                case RegionKind::kStrokeAndUnderline:
                case RegionKind::kDRCS:
                    checker.Expect(pixel.a >= kTranslucentBlue.a, "at least the box alpha", x, y, pixel);
                    break;
            }
        }
    }
}

struct RenderedFrame {
    Frame frame;
    size_t image_count = 0;
};

bool RenderFrame(Context& context, TextRendererType renderer_type, bool merge, RenderedFrame& out) {
    Renderer renderer(context);
    if (!renderer.Initialize(CaptionType::kCaption, FontProviderType::kAuto, renderer_type)) {
        fprintf(stderr, "Renderer::Initialize() failed\n");
        return false;
    }
    renderer.SetFrameSize(kPlaneWidth, kPlaneHeight);
    renderer.SetMergeRegionImages(merge);
    renderer.AppendCaption(MakeCaption());

    RenderResult result;
    if (renderer.Render(0, result) != RenderStatus::kGotImage) {
        fprintf(stderr, "Renderer::Render() did not return an image\n");
        return false;
    }

    out.image_count = result.images.size();
    return ComposeFrame(result.images, out.frame);
}

bool VerifyRenderer(Context& context, TextRendererType renderer_type, const char* renderer_name) {
    bool passed = true;
    RenderedFrame frames[2];

    for (bool merge : {false, true}) {
        RenderedFrame& rendered = frames[merge];
        printf("%s, merge_region_images=%s\n", renderer_name, merge ? "on" : "off");
        if (!RenderFrame(context, renderer_type, merge, rendered)) {
            passed = false;
            continue;
        }

        size_t expected_images = merge ? 1 : std::size(kRegions);
        if (rendered.image_count != expected_images) {
            printf("    expected %zu images, got %zu\n", expected_images, rendered.image_count);
            passed = false;
        }

        for (size_t r = 0; r < std::size(kRegions); r++) {
            Checker checker;
            CheckRegion(checker, rendered.frame, r);
            printf("  %-48s checks=%-6" PRIu64 " failures=%" PRIu64 "\n",
                   kRegions[r].name, checker.checks(), checker.failures());
            passed &= checker.failures() == 0;
        }
    }

    size_t differences = 0;
    for (size_t i = 0; i < frames[0].frame.pixels.size(); i++) {
        differences += frames[0].frame.pixels[i].u32 != frames[1].frame.pixels[i].u32;
    }
    printf("%s, merged vs. unmerged: %zu differing pixels\n", renderer_name, differences);
    passed &= differences == 0;

    return passed;
}

}  // namespace

int main(int argc, char** argv) {
    Context context;
    bool passed = true;

    passed &= VerifyRenderer(context, TextRendererType::kAuto, "default text renderer");
#if defined(ARIBCC_USE_FREETYPE)
    passed &= VerifyRenderer(context, TextRendererType::kFreetype, "FreeType text renderer");
#endif

    printf("%s\n", passed ? "PASSED" : "FAILED");
    return passed ? 0 : 1;
}
