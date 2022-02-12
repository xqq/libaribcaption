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

#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <utility>
#include "base/cfstr_helper.hpp"
#include "base/utf_helper.hpp"
#include "renderer/font_provider_coretext.hpp"
#include "renderer/text_renderer_coretext.hpp"

namespace aribcaption {

TextRendererCoreText::TextRendererCoreText(Context& context, FontProvider& font_provider)
    : log_(GetContextLogger(context)), font_provider_(font_provider) {}

TextRendererCoreText::~TextRendererCoreText() = default;

bool TextRendererCoreText::Initialize() {
    return true;
}

void TextRendererCoreText::SetLanguage(uint32_t iso6392_language_code) {
    (void)iso6392_language_code;
    // No-OP
}

bool TextRendererCoreText::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    if (!font_family_.empty() && font_family_ != font_family) {
        // Reset CoreText fonts
        main_ctfont_pixel_height_ = 0;
        fallback_ctfont_pixel_height_ = 0;
        main_face_index_ = 0;
        main_ctfont_sized_.reset();
        main_ctfont_.reset();
        fallback_ctfont_sized_.reset();
        fallback_ctfont_.reset();
    }

    font_family_ = font_family;
    return true;
}

struct TextRenderContextPrivateCoreText : public TextRenderContext::ContextPrivate {
public:
    TextRenderContextPrivateCoreText() = default;
    ~TextRenderContextPrivateCoreText() override = default;
public:
    ScopedCFRef<CGContextRef> cg_context;
};

auto TextRendererCoreText::BeginDraw(Bitmap& target_bmp) -> TextRenderContext {
    ScopedCFRef<CGContextRef> ctx = CreateBitmapTargetCGContext(target_bmp);

    CGContextSetShouldAntialias(ctx.get(), true);
    CGContextSetShouldSmoothFonts(ctx.get(), true);

    CGContextSetAllowsFontSubpixelPositioning(ctx.get(), true);
    CGContextSetShouldSubpixelPositionFonts(ctx.get(), true);

    CGContextSetAllowsFontSubpixelQuantization(ctx.get(), true);
    CGContextSetShouldSubpixelQuantizeFonts(ctx.get(), true);

    auto priv = std::make_unique<TextRenderContextPrivateCoreText>();
    priv->cg_context = std::move(ctx);

    return TextRenderContext{target_bmp, std::move(priv)};
}

void TextRendererCoreText::EndDraw(TextRenderContext& context) {
    auto priv = static_cast<TextRenderContextPrivateCoreText*>(context.GetPrivate());
    // Release CGContext
    priv->cg_context.reset();
}

auto TextRendererCoreText::DrawChar(TextRenderContext& render_ctx, int target_x, int target_y,
                                    uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                                    float stroke_width, int char_width, int char_height,
                                    std::optional<UnderlineInfo> underline_info,
                                    TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus {
    if (!render_ctx.GetPrivate()) {
        log_->e("TextRendererCoreText: Invalid TextRenderContext, BeginDraw() failed or not called");
        return TextRenderStatus::kOtherError;
    }

    assert(char_height > 0);
    if (stroke_width < 0.0f) {
        stroke_width = 0.0f;
    }

    // Handle space characters
    if (ucs4 == 0x0009 || ucs4 == 0x0020 || ucs4 == 0x00A0 || ucs4 == 0x1680 ||
        ucs4 == 0x3000 || ucs4 == 0x202F || ucs4 == 0x205F || (ucs4 >= 0x2000 && ucs4 <= 0x200A)) {
        return TextRenderStatus::kOK;
    }

    // Load main CTFont if not loaded
    if (!main_ctfont_) {
        auto result = LoadCTFont();
        if (result.is_err()) {
            log_->e("TextRendererCoreText: Cannot find valid font");
            return FontProviderErrorToStatus(result.error());
        }
        std::pair<ScopedCFRef<CTFontRef>, size_t>& pair = result.value();
        main_ctfont_ = std::move(pair.first);
        main_face_index_ = pair.second;
    }

    // Build size-specified main CTFont
    if (!main_ctfont_sized_ || char_height != main_ctfont_pixel_height_) {
        main_ctfont_pixel_height_ = char_height;
        main_ctfont_sized_ = CreateSizedCTFont(main_ctfont_.get(), char_height);
        if (!main_ctfont_sized_) {
            log_->e("TextRendererCoreText: Create sized CTFont failed");
            return TextRenderStatus::kOtherError;
        }
    }

    CTFontRef ctfont = main_ctfont_sized_.get();

    std::u16string utf16;
    size_t codeunit_count = utf::UTF16AppendCodePoint(utf16, ucs4);
    CGGlyph glyphs[2] = {0};
    bool has_glyph = CTFontGetGlyphsForCharacters(ctfont,
                                                  reinterpret_cast<UniChar*>(utf16.data()),
                                                  glyphs,
                                                  static_cast<CFIndex>(codeunit_count));

    // Check whether codepoint exists in main CTFont, otherwise load fallback CTFont
    if (!has_glyph) {
        ScopedCFRef<CFStringRef> cf_main_family_name(CTFontCopyFamilyName(ctfont));
        std::string main_family_name = cfstr::CFStringToStdString(cf_main_family_name.get());
        log_->w("TextRendererCoreText: Main font %s doesn't contain U+%04X", main_family_name.c_str(), ucs4);

        if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
            return TextRenderStatus::kCodePointNotFound;
        }

        if (main_face_index_ + 1 >= font_family_.size()) {
            // Fallback fonts not available
            return TextRenderStatus::kCodePointNotFound;
        }

        bool reset_sized_fallback = false;
        // Missing glyph, check fallback CTFont
        if (!fallback_ctfont_ || !CTFontGetGlyphsForCharacters(fallback_ctfont_.get(),
                                                               reinterpret_cast<UniChar*>(utf16.data()),
                                                               glyphs,
                                                               static_cast<CFIndex>(codeunit_count))) {
            // Fallback CTFont not loaded, or fallback CTFont doesn't contain required codepoint
            // Load next fallback CTFont by specific codepoint
            auto result = LoadCTFont(ucs4, main_face_index_ + 1);
            if (result.is_err()) {
                log_->e("TextRendererCoreText: Cannot find available fallback font for U+%04X", ucs4);
                return FontProviderErrorToStatus(result.error());
            }
            std::pair<ScopedCFRef<CTFontRef>, size_t>& pair = result.value();
            fallback_ctfont_ = std::move(pair.first);
            reset_sized_fallback = true;
        }

        // Build size-specified fallback CTFont
        if (!fallback_ctfont_sized_ || char_height != fallback_ctfont_pixel_height_ || reset_sized_fallback) {
            fallback_ctfont_pixel_height_ = char_height;
            fallback_ctfont_sized_ = CreateSizedCTFont(fallback_ctfont_.get(), char_height);
            if (!fallback_ctfont_sized_) {
                log_->e("TextRendererCoreText: Create sized fallback CTFont failed");
                return TextRenderStatus::kOtherError;
            }
        }

        ctfont = fallback_ctfont_sized_.get();
    }

    // Re-retrieve glyph from sized CTFont
    CTFontGetGlyphsForCharacters(ctfont,
                                 reinterpret_cast<UniChar*>(utf16.data()),
                                 glyphs,
                                 static_cast<CFIndex>(codeunit_count));

    CGFloat ascent = CTFontGetAscent(ctfont);
    CGFloat descent = CTFontGetDescent(ctfont);
    CGFloat em_height = ascent + descent;

    CGFloat em_adjust_y = (static_cast<CGFloat>(char_height) - em_height) / 2.0f;
    CGFloat charbox_bottom = render_ctx.GetBitmap().height() - (target_y + char_height);
    CGFloat baseline_y = std::round(charbox_bottom + descent + em_adjust_y);

    CGFloat underline_pos = CTFontGetUnderlinePosition(ctfont);
    CGFloat underline_thickness = CTFontGetUnderlineThickness(ctfont);

    auto render_ctx_priv = static_cast<TextRenderContextPrivateCoreText*>(render_ctx.GetPrivate());
    const ScopedCFRef<CGContextRef>& ctx = render_ctx_priv->cg_context;

    CGContextSaveGState(ctx.get());

    // Draw Underline if required
    if ((style & kCharStyleUnderline) && underline_info && underline_thickness > 0.0f) {
        CGFloat underline_y = baseline_y + underline_pos;
        ScopedCFRef<CGColorRef> cg_underline_color = RGBAToCGColor(color);
        CGContextSetStrokeColorWithColor(ctx.get(), cg_underline_color.get());
        CGContextSetLineWidth(ctx.get(), underline_thickness);

        CGContextMoveToPoint(ctx.get(), underline_info->start_x, underline_y);
        CGContextAddLineToPoint(ctx.get(), underline_info->start_x + underline_info->width, underline_y);
        CGContextStrokePath(ctx.get());
    }

    CGPoint origin = CGPointMake(target_x, baseline_y);

    // Scale character correctly if char_width is different from char_height
    if (char_width != char_height) {
        CGFloat horizontal_scale = static_cast<CGFloat>(char_width) / static_cast<CGFloat>(char_height);
        CGContextTranslateCTM(ctx.get(), origin.x, origin.y);
        CGContextScaleCTM(ctx.get(), horizontal_scale, 1.0f);
        CGContextTranslateCTM(ctx.get(), -origin.x, -origin.y);
    }

    // Draw stroke border if required
    if (style & CharStyle::kCharStyleStroke && stroke_width > 0.0f) {
        CGAffineTransform path_matrix = CGAffineTransformMakeTranslation(origin.x, origin.y);
        ScopedCFRef<CGPathRef> path(CTFontCreatePathForGlyph(ctfont, glyphs[0], &path_matrix));
        CGContextAddPath(ctx.get(), path.get());

        ScopedCFRef<CGColorRef> cg_stroke_color = RGBAToCGColor(stroke_color);
        CGContextSetStrokeColorWithColor(ctx.get(), cg_stroke_color.get());
        CGContextSetLineWidth(ctx.get(), stroke_width * 2);
        CGContextSetLineCap(ctx.get(), kCGLineCapRound);
        CGContextSetLineJoin(ctx.get(), kCGLineJoinRound);
        CGContextStrokePath(ctx.get());
    }

    // Draw character (fill)
    ScopedCFRef<CGColorRef> cg_fill_color = RGBAToCGColor(color);
    CGContextSetFillColorWithColor(ctx.get(), cg_fill_color.get());
    CTFontDrawGlyphs(ctfont, &glyphs[0], &origin, 1, ctx.get());

    CGContextRestoreGState(ctx.get());

    return TextRenderStatus::kOK;
}

auto TextRendererCoreText::LoadCTFont(std::optional<uint32_t> codepoint, std::optional<size_t> begin_index)
        -> Result<std::pair<ScopedCFRef<CTFontRef>, size_t>, FontProviderError> {
    if (begin_index && begin_index.value() >= font_family_.size()) {
        return Err(FontProviderError::kFontNotFound);
    }

    // begin_index is optional
    size_t font_index = begin_index.value_or(0);

    const std::string& font_name = font_family_[font_index];
    auto result = font_provider_.GetFontFace(font_name, codepoint);

    while (result.is_err() && font_index + 1 < font_family_.size()) {
        // Find next suitable font
        font_index++;
        result = font_provider_.GetFontFace(font_family_[font_index], codepoint);
    }
    if (result.is_err()) {
        // Not found, return Err Result
        return Err(result.error());
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kCoreText) {
        log_->e("TextRendererCoreText: Font provider must be FontProviderCoreText");
        return Err(FontProviderError::kOtherError);
    }

    auto priv = static_cast<FontfaceInfoPrivateCoreText*>(info.provider_priv.get());

    return Ok(std::make_pair(std::move(priv->ct_font), font_index));
}

auto TextRendererCoreText::RGBAToCGColor(ColorRGBA rgba) -> ScopedCFRef<CGColorRef> {
    CGFloat components[] = {
        static_cast<CGFloat>(rgba.r) / 255.0f,
        static_cast<CGFloat>(rgba.g) / 255.0f,
        static_cast<CGFloat>(rgba.b) / 255.0f,
        static_cast<CGFloat>(rgba.a) / 255.0f,
    };
    ScopedCFRef<CGColorSpaceRef> space(CGColorSpaceCreateDeviceRGB());
    ScopedCFRef<CGColorRef> cgcolor(CGColorCreate(space.get(), components));

    return cgcolor;
}

auto TextRendererCoreText::CreateBitmapTargetCGContext(Bitmap& bitmap) -> ScopedCFRef<CGContextRef> {
    size_t bits_per_component = 8;
    ScopedCFRef<CGColorSpaceRef> space(CGColorSpaceCreateDeviceRGB());
    uint32_t bitmap_info = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;

    ScopedCFRef<CGContextRef> cgctx(CGBitmapContextCreate(bitmap.data(),
                                                          bitmap.width(),
                                                          bitmap.height(),
                                                          bits_per_component,
                                                          bitmap.stride(),
                                                          space.get(),
                                                          bitmap_info));
    return cgctx;
}

auto TextRendererCoreText::CreateSizedCTFont(CTFontRef ctfont, int char_height) -> ScopedCFRef<CTFontRef> {
    ScopedCFRef<CTFontRef> sized_ctfont(CTFontCreateCopyWithAttributes(ctfont,
                                                                       static_cast<CGFloat>(char_height),
                                                                       nullptr,
                                                                       nullptr));
    return sized_ctfont;
}

}  // namespace aribcaption
