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
#include "base/floating_helper.hpp"
#include "base/unicode_helper.hpp"
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
    if (iso6392_language_code_ != iso6392_language_code) {
        // Font provider resolves font families (e.g. sans-serif) per language, reset loaded fonts
        ResetFontFaces();
    }
    iso6392_language_code_ = iso6392_language_code;
}

bool TextRendererCoreText::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    if (!font_family_.empty() && font_family_ != font_family) {
        ResetFontFaces();
    }

    font_family_ = font_family;
    return true;
}

void TextRendererCoreText::ResetFontFaces() {
    font_slots_.clear();
    system_fallback_slots_.clear();
    main_slot_index_.reset();
    main_load_error_ = FontProviderError::kFontNotFound;
    missing_codepoints_.clear();
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
                                    float stroke_width, int char_width, int char_height, float aspect_ratio,
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

    // Skip space characters
    if (unicode::IsSpaceCharacter(ucs4)) {
        return TextRenderStatus::kOK;
    }

    if (auto err = EnsureMainSlot()) {
        log_->e("TextRendererCoreText: Cannot find valid font");
        return FontProviderErrorToStatus(*err);
    }

    FontSlot& main_slot = font_slots_[*main_slot_index_];
    CTFontRef ctfont = EnsureSizedCTFont(main_slot, char_height);
    if (!ctfont) {
        log_->e("TextRendererCoreText: Create sized CTFont failed");
        return TextRenderStatus::kOtherError;
    }

    std::u16string utf16;
    size_t codeunit_count = utf::UTF16AppendCodePoint(utf16, ucs4);
    CGGlyph glyphs[2] = {0};
    bool has_glyph = CTFontGetGlyphsForCharacters(ctfont,
                                                  reinterpret_cast<UniChar*>(utf16.data()),
                                                  glyphs,
                                                  static_cast<CFIndex>(codeunit_count));

    if (!has_glyph) {
        if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
            return TextRenderStatus::kCodePointNotFound;
        }

        if (missing_codepoints_.find(ucs4) != missing_codepoints_.end()) {
            return TextRenderStatus::kCodePointNotFound;
        }

        for (size_t i = *main_slot_index_ + 1; i < font_slots_.size(); i++) {
            FontSlot& slot = font_slots_[i];
            if (slot.state == SlotState::kUnavailable) {
                continue;
            }
            if (slot.state == SlotState::kUnloaded) {
                LoadSlot(i);
                if (slot.state != SlotState::kLoaded) {
                    continue;
                }
            }

            if (!CTFontGetGlyphsForCharacters(slot.ctfont.get(),
                                              reinterpret_cast<UniChar*>(utf16.data()),
                                              glyphs,
                                              static_cast<CFIndex>(codeunit_count))) {
                continue;
            }

            ctfont = EnsureSizedCTFont(slot, char_height);
            if (!ctfont) {
                log_->e("TextRendererCoreText: Create sized fallback CTFont failed");
                return TextRenderStatus::kOtherError;
            }

            has_glyph = CTFontGetGlyphsForCharacters(ctfont,
                                                     reinterpret_cast<UniChar*>(utf16.data()),
                                                     glyphs,
                                                     static_cast<CFIndex>(codeunit_count));
            if (has_glyph) {
                break;
            }
        }

        if (!has_glyph && fallback_policy == TextRenderFallbackPolicy::kAutoFallback) {
            if (CTFontRef sys_ctfont = FindOrLoadSystemFallbackSlot(ucs4,
                                                                    main_slot.family_name,
                                                                    reinterpret_cast<const UniChar*>(utf16.data()),
                                                                    static_cast<CFIndex>(codeunit_count),
                                                                    char_height,
                                                                    glyphs)) {
                ctfont = sys_ctfont;
                has_glyph = true;
            }
        }

        if (!has_glyph) {
            // A font-family-only miss may still be served by system fallback later, so don't cache it,
            // unless it is a PUA codepoint which system fallback never handles
            if (fallback_policy == TextRenderFallbackPolicy::kAutoFallback || unicode::IsPrivateUseArea(ucs4)) {
                missing_codepoints_.insert(ucs4);
                log_->e("TextRendererCoreText: Cannot find available fallback font for U+%04X", ucs4);
            }
            return TextRenderStatus::kCodePointNotFound;
        }
    }

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

    // Check if it is MSZ (Middle Size)
    bool is_requesting_halfwidth = floating::AlmostEquals(aspect_ratio, 0.5f, 0.05f);
    bool needless_horizontal_scaling = is_requesting_halfwidth && unicode::IsHalfwidthCharacter(ucs4);

    // Scale character correctly if char_width is different from char_height
    // Do not do horizontal scaling if the character is halfwidth already
    if (char_width != char_height && !needless_horizontal_scaling) {
        CGFloat horizontal_scale = static_cast<CGFloat>(char_width) / static_cast<CGFloat>(char_height);
        CGContextTranslateCTM(ctx.get(), origin.x, origin.y);
        CGContextScaleCTM(ctx.get(), horizontal_scale, 1.0f);
        CGContextTranslateCTM(ctx.get(), -origin.x, -origin.y);
    }

    // Draw stroke border if required
    if (style & CharStyle::kCharStyleStroke && stroke_width > 0.0f) {
        CGAffineTransform path_matrix = CGAffineTransformMakeTranslation(origin.x, origin.y);
        ScopedCFRef<CGPathRef> path(CTFontCreatePathForGlyph(ctfont, glyphs[0], &path_matrix));
        // Bitmap/color glyphs (e.g. Apple Color Emoji) have no outline path
        if (path) {
            CGContextAddPath(ctx.get(), path.get());

            ScopedCFRef<CGColorRef> cg_stroke_color = RGBAToCGColor(stroke_color);
            CGContextSetStrokeColorWithColor(ctx.get(), cg_stroke_color.get());
            CGContextSetLineWidth(ctx.get(), stroke_width * 2);
            CGContextSetLineCap(ctx.get(), kCGLineCapRound);
            CGContextSetLineJoin(ctx.get(), kCGLineJoinRound);
            CGContextStrokePath(ctx.get());
        }
    }

    // Draw character (fill)
    ScopedCFRef<CGColorRef> cg_fill_color = RGBAToCGColor(color);
    CGContextSetFillColorWithColor(ctx.get(), cg_fill_color.get());
    CTFontDrawGlyphs(ctfont, &glyphs[0], &origin, 1, ctx.get());

    CGContextRestoreGState(ctx.get());

    return TextRenderStatus::kOK;
}

bool TextRendererCoreText::IsDuplicateFont(const std::string& family_name,
                                           const std::string& postscript_name) const {
    auto matches_list = [&](const std::vector<FontSlot>& slots) -> bool {
        for (const auto& existing : slots) {
            if (existing.state != SlotState::kLoaded) {
                continue;
            }
            if (!postscript_name.empty() && !existing.postscript_name.empty()) {
                if (postscript_name == existing.postscript_name) {
                    return true;
                }
            } else if (!family_name.empty() && family_name == existing.family_name) {
                return true;
            }
        }
        return false;
    };

    return matches_list(font_slots_) || matches_list(system_fallback_slots_);
}

auto TextRendererCoreText::LoadSlot(size_t slot_index) -> std::optional<FontProviderError> {
    FontSlot& slot = font_slots_[slot_index];
    if (slot.state != SlotState::kUnloaded) {
        return std::nullopt;
    }

    auto result = font_provider_.GetFontFace(font_family_[slot_index]);
    if (result.is_err()) {
        slot.state = SlotState::kUnavailable;
        return result.error();
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kCoreText) {
        log_->e("TextRendererCoreText: Font provider must be FontProviderCoreText");
        slot.state = SlotState::kUnavailable;
        return FontProviderError::kOtherError;
    }

    auto priv = static_cast<FontfaceInfoPrivateCoreText*>(info.provider_priv.get());
    if (!priv || !priv->ct_font) {
        slot.state = SlotState::kUnavailable;
        return FontProviderError::kFontNotFound;
    }

    std::string family_name = info.family_name;
    if (family_name.empty()) {
        ScopedCFRef<CFStringRef> cf_family(CTFontCopyFamilyName(priv->ct_font.get()));
        if (cf_family) {
            family_name = cfstr::CFStringToStdString(cf_family.get());
        }
    }

    std::string postscript_name = info.postscript_name;
    if (postscript_name.empty()) {
        ScopedCFRef<CFStringRef> cf_ps(CTFontCopyPostScriptName(priv->ct_font.get()));
        if (cf_ps) {
            postscript_name = cfstr::CFStringToStdString(cf_ps.get());
        }
    }

    if (IsDuplicateFont(family_name, postscript_name)) {
        slot.state = SlotState::kUnavailable;
        return std::nullopt;
    }

    slot.ctfont = std::move(priv->ct_font);
    slot.family_name = std::move(family_name);
    slot.postscript_name = std::move(postscript_name);
    slot.state = SlotState::kLoaded;
    return std::nullopt;
}

auto TextRendererCoreText::EnsureMainSlot() -> std::optional<FontProviderError> {
    if (font_family_.empty()) {
        return FontProviderError::kFontNotFound;
    }

    if (main_slot_index_) {
        return std::nullopt;
    }

    if (font_slots_.empty()) {
        font_slots_.resize(font_family_.size());
    }

    for (size_t i = 0; i < font_slots_.size(); i++) {
        if (font_slots_[i].state == SlotState::kUnavailable) {
            continue;
        }
        if (auto err = LoadSlot(i)) {
            main_load_error_ = *err;
        }
        if (font_slots_[i].state == SlotState::kLoaded) {
            main_slot_index_ = i;
            return std::nullopt;
        }
    }

    return main_load_error_;
}

auto TextRendererCoreText::FindOrLoadSystemFallbackSlot(uint32_t ucs4,
                                                        std::string_view base_family_name,
                                                        const UniChar* utf16,
                                                        CFIndex codeunit_count,
                                                        int char_height,
                                                        CGGlyph* out_glyphs) -> CTFontRef {
    if (unicode::IsPrivateUseArea(ucs4)) {
        return nullptr;
    }

    for (auto& slot : system_fallback_slots_) {
        if (slot.state != SlotState::kLoaded) {
            continue;
        }
        if (!CTFontGetGlyphsForCharacters(slot.ctfont.get(), utf16, out_glyphs, codeunit_count)) {
            continue;
        }
        CTFontRef sized = EnsureSizedCTFont(slot, char_height);
        if (sized && CTFontGetGlyphsForCharacters(sized, utf16, out_glyphs, codeunit_count)) {
            return sized;
        }
    }

    auto result = font_provider_.GetFallbackFontFace(ucs4, base_family_name);
    if (result.is_err()) {
        return nullptr;
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kCoreText) {
        return nullptr;
    }

    auto priv = static_cast<FontfaceInfoPrivateCoreText*>(info.provider_priv.get());
    if (!priv || !priv->ct_font) {
        return nullptr;
    }

    std::string family_name = info.family_name;
    if (family_name.empty()) {
        ScopedCFRef<CFStringRef> cf_family(CTFontCopyFamilyName(priv->ct_font.get()));
        if (cf_family) {
            family_name = cfstr::CFStringToStdString(cf_family.get());
        }
    }

    std::string postscript_name = info.postscript_name;
    if (postscript_name.empty()) {
        ScopedCFRef<CFStringRef> cf_ps(CTFontCopyPostScriptName(priv->ct_font.get()));
        if (cf_ps) {
            postscript_name = cfstr::CFStringToStdString(cf_ps.get());
        }
    }

    if (IsDuplicateFont(family_name, postscript_name)) {
        return nullptr;
    }

    FontSlot slot;
    slot.ctfont = std::move(priv->ct_font);
    slot.family_name = std::move(family_name);
    slot.postscript_name = std::move(postscript_name);
    slot.state = SlotState::kLoaded;

    CTFontRef sized = EnsureSizedCTFont(slot, char_height);
    if (!sized || !CTFontGetGlyphsForCharacters(sized, utf16, out_glyphs, codeunit_count)) {
        return nullptr;
    }

    static constexpr size_t kMaxSystemFallbackSlots = 16;
    if (system_fallback_slots_.size() >= kMaxSystemFallbackSlots) {
        system_fallback_slots_.erase(system_fallback_slots_.begin());
    }

    system_fallback_slots_.push_back(std::move(slot));
    return system_fallback_slots_.back().ctfont_sized.get();
}

auto TextRendererCoreText::EnsureSizedCTFont(FontSlot& slot, int char_height) -> CTFontRef {
    if (!slot.ctfont_sized || slot.pixel_height != char_height) {
        slot.ctfont_sized = CreateSizedCTFont(slot.ctfont.get(), char_height);
        if (!slot.ctfont_sized) {
            slot.pixel_height = 0;
            return nullptr;
        }
        slot.pixel_height = char_height;
    }
    return slot.ctfont_sized.get();
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
