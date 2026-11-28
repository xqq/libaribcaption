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
#include <TargetConditionals.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string_view>
#include <vector>
#if TARGET_OS_IPHONE
    #include <CoreText/CoreText.h>
#else
    #include <ApplicationServices/ApplicationServices.h>
#endif
#include "base/cfstr_helper.hpp"
#include "base/language_code.hpp"
#include "base/scoped_cfref.hpp"
#include "base/scoped_holder.hpp"
#include "base/unicode_helper.hpp"
#include "base/utf_helper.hpp"
#include "renderer/font_provider_coretext.hpp"

namespace aribcaption {

FontProviderCoreText::FontProviderCoreText(Context& context) : log_(GetContextLogger(context)) {}

FontProviderType FontProviderCoreText::GetType() {
    return FontProviderType::kCoreText;
}

bool FontProviderCoreText::Initialize() {
    return true;
}

void FontProviderCoreText::SetLanguage(uint32_t iso6392_language_code) {
    if (iso6392_language_code_ != iso6392_language_code) {
        cached_base_font_.reset();
        cached_base_family_name_.clear();
        cached_base_language_code_ = 0;
    }
    iso6392_language_code_ = iso6392_language_code;
}

// The returned view refers to either family_name or a string literal
static std::string_view ConvertFamilyName(std::string_view family_name, uint32_t iso6392_language_code) {
    std::string_view font_name = family_name;

    if (family_name == "sans-serif") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "Hiragino Sans";
        } else {
            font_name = "Verdana";
        }
    } else if (family_name == "serif") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "Hiragino Mincho ProN";
        } else {
            font_name = "Times";
        }
    } else if (family_name == "monospace") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "Hiragino Sans";
        } else {
            font_name = "Courier";
        }
    }

    if (font_name == "Hiragino Sans") {
#if TARGET_OS_IPHONE
        font_name = "Hiragino Sans W3";  // iOS
#else
        font_name = "Hiragino Sans W4";  // macOS
#endif
    }

    return font_name;
}

static auto MatchFontDescriptor(CFStringRef font_name, CFStringRef name_attribute) -> ScopedCFRef<CTFontDescriptorRef> {
    ScopedCFRef<CFMutableDictionaryRef> cf_attributes(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                                0,
                                                                                &kCFTypeDictionaryKeyCallBacks,
                                                                                &kCFTypeDictionaryValueCallBacks));
    ScopedCFRef<CFMutableDictionaryRef> cf_traits(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                            0,
                                                                            &kCFTypeDictionaryKeyCallBacks,
                                                                            &kCFTypeDictionaryValueCallBacks));
    if (!cf_attributes || !cf_traits) {
        return {};
    }

    CGFloat font_weight = 0.0;
    ScopedCFRef<CFNumberRef> cf_font_weight(CFNumberCreate(nullptr, kCFNumberCGFloatType, &font_weight));
    CFDictionaryAddValue(cf_traits.get(), kCTFontWeightTrait, cf_font_weight.get());
    CFDictionaryAddValue(cf_attributes.get(), kCTFontTraitsAttribute, cf_traits.get());
    CFDictionaryAddValue(cf_attributes.get(), name_attribute, font_name);

    ScopedCFRef<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(cf_attributes.get()));
    if (!descriptor) {
        return {};
    }

    const void* mandatory_keys[] = {name_attribute};
    ScopedCFRef<CFSetRef> mandatory(CFSetCreate(kCFAllocatorDefault, mandatory_keys, 1, &kCFTypeSetCallBacks));
    if (!mandatory) {
        return {};
    }

    // Require mandatory name match so CoreText does not silently substitute a default font
    return ScopedCFRef<CTFontDescriptorRef>(CTFontDescriptorCreateMatchingFontDescriptor(descriptor.get(),
                                                                                         mandatory.get()));
}

static uint16_t ReadU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static uint32_t ReadU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// Returns the 1-based fvar named instance matching the font's variation,
// or 0 if the font is at its default instance or matches no named instance
static int FindNamedInstance(CTFontRef ct_font) {
    ScopedCFRef<CFDictionaryRef> variation(CTFontCopyVariation(ct_font));
    CFIndex value_count = variation ? CFDictionaryGetCount(variation.get()) : 0;
    if (value_count <= 0) {
        return 0;
    }
    std::vector<const void*> keys(static_cast<size_t>(value_count));
    std::vector<const void*> values(static_cast<size_t>(value_count));
    CFDictionaryGetKeysAndValues(variation.get(), keys.data(), values.data());

    ScopedCFRef<CFDataRef> fvar(CTFontCopyTable(ct_font, kCTFontTableFvar, kCTFontTableOptionNoOptions));
    size_t fvar_size = fvar ? static_cast<size_t>(CFDataGetLength(fvar.get())) : 0;
    if (fvar_size < 16) {
        return 0;
    }

    const uint8_t* p = CFDataGetBytePtr(fvar.get());
    size_t axes_offset = ReadU16BE(p + 4);
    size_t axis_count = ReadU16BE(p + 8);
    size_t axis_size = ReadU16BE(p + 10);
    size_t instance_count = std::min<size_t>(ReadU16BE(p + 12), 0x7FFF);
    size_t instance_size = ReadU16BE(p + 14);
    if (axis_count == 0 || axis_size < 20 || instance_size < 4 + 4 * axis_count ||
        axes_offset + axis_count * axis_size + instance_count * instance_size > fvar_size) {
        return 0;
    }

    // CoreText omits axes at their default, and derives opsz from the point size rather than from an instance
    constexpr uint32_t kOpszTag = (uint32_t{'o'} << 24) | (uint32_t{'p'} << 16) | (uint32_t{'s'} << 8) | uint32_t{'z'};
    constexpr double kTolerance = 0.01;
    std::vector<double> targets(axis_count);
    std::vector<bool> compared(axis_count);
    bool at_default = true;
    for (size_t a = 0; a < axis_count; a++) {
        const uint8_t* axis = p + axes_offset + a * axis_size;
        uint32_t tag = ReadU32BE(axis);
        double default_value = static_cast<double>(static_cast<int32_t>(ReadU32BE(axis + 8))) / 65536.0;
        targets[a] = default_value;
        compared[a] = (tag != kOpszTag);
        for (size_t i = 0; i < keys.size(); i++) {
            auto key = static_cast<CFNumberRef>(keys[i]);
            auto value = static_cast<CFNumberRef>(values[i]);
            int64_t key_tag = 0;
            double axis_value = 0.0;
            if (CFGetTypeID(key) == CFNumberGetTypeID() && CFGetTypeID(value) == CFNumberGetTypeID() &&
                CFNumberGetValue(key, kCFNumberSInt64Type, &key_tag) && static_cast<uint32_t>(key_tag) == tag &&
                CFNumberGetValue(value, kCFNumberDoubleType, &axis_value)) {
                targets[a] = axis_value;
                break;
            }
        }
        if (compared[a] && std::fabs(targets[a] - default_value) >= kTolerance) {
            at_default = false;
        }
    }
    if (at_default) {
        return 0;
    }

    const uint8_t* instances = p + axes_offset + axis_count * axis_size;
    for (size_t i = 0; i < instance_count; i++) {
        const uint8_t* coords = instances + i * instance_size + 4;
        bool matched = true;
        for (size_t a = 0; a < axis_count && matched; a++) {
            double coord = static_cast<double>(static_cast<int32_t>(ReadU32BE(coords + a * 4))) / 65536.0;
            matched = !compared[a] || std::fabs(coord - targets[a]) < kTolerance;
        }
        if (matched) {
            return static_cast<int>(i + 1);
        }
    }

    return 0;
}

static auto BuildFontfaceInfo(ScopedCFRef<CTFontRef> ct_font) -> Result<FontfaceInfo, FontProviderError> {
    ScopedCFRef<CTFontDescriptorRef> ct_font_descriptor(CTFontCopyFontDescriptor(ct_font.get()));

    std::string filename;
    ScopedCFRef<CFStringRef> cf_family_name;
    ScopedCFRef<CFStringRef> cf_postscript_name;

    if (ct_font_descriptor) {
        ScopedCFRef<CFURLRef> cf_url(static_cast<CFURLRef>(
            CTFontDescriptorCopyAttribute(ct_font_descriptor.get(), kCTFontURLAttribute)));
        if (cf_url) {
            ScopedCFRef<CFStringRef> cf_scheme(CFURLCopyScheme(cf_url.get()));
            if (cf_scheme &&
                CFStringCompare(cf_scheme.get(), CFSTR("file"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                ScopedCFRef<CFStringRef> cf_path(CFURLCopyFileSystemPath(cf_url.get(), kCFURLPOSIXPathStyle));
                if (cf_path) {
                    filename = cfstr::CFStringToStdString(cf_path.get());
                }
            }
        }

        cf_family_name.reset(static_cast<CFStringRef>(
            CTFontDescriptorCopyAttribute(ct_font_descriptor.get(), kCTFontFamilyNameAttribute)));
        cf_postscript_name.reset(static_cast<CFStringRef>(
            CTFontDescriptorCopyAttribute(ct_font_descriptor.get(), kCTFontNameAttribute)));
    }

    if (!cf_family_name) {
        cf_family_name.reset(CTFontCopyFamilyName(ct_font.get()));
    }
    if (!cf_postscript_name) {
        cf_postscript_name.reset(CTFontCopyPostScriptName(ct_font.get()));
    }
    if (!cf_family_name && !cf_postscript_name) {
        return Err(FontProviderError::kOtherError);
    }

    FontfaceInfo info;
    info.family_name = cfstr::CFStringToStdString(cf_family_name.get());
    info.postscript_name = cfstr::CFStringToStdString(cf_postscript_name.get());
    info.filename = std::move(filename);
    info.face_index = -1;
    info.named_instance = FindNamedInstance(ct_font.get());
    info.provider_type = FontProviderType::kCoreText;

    auto priv = std::make_unique<FontfaceInfoPrivateCoreText>();
    priv->ct_font = std::move(ct_font);
    info.provider_priv = std::move(priv);

    return Ok(std::move(info));
}

static auto ResolveCTFontByName(std::string_view font_name) -> ScopedCFRef<CTFontRef> {
    ScopedCFRef<CFStringRef> fontname_request(cfstr::StdStringToCFString(font_name));
    if (!fontname_request) {
        return {};
    }

    ScopedCFRef<CTFontDescriptorRef> matched = MatchFontDescriptor(fontname_request.get(), kCTFontFamilyNameAttribute);
    if (!matched) {
        matched = MatchFontDescriptor(fontname_request.get(), kCTFontNameAttribute);
    }
    if (!matched) {
        return {};
    }

    return ScopedCFRef<CTFontRef>(CTFontCreateWithFontDescriptor(matched.get(), 0, nullptr));
}

auto FontProviderCoreText::GetFontFace(const std::string& font_name) -> Result<FontfaceInfo, FontProviderError> {
    std::string_view converted_font = ConvertFamilyName(font_name, iso6392_language_code_);
    ScopedCFRef<CTFontRef> ct_font = ResolveCTFontByName(converted_font);
    if (!ct_font) {
        log_->w("CoreText: Cannot find font %.*s", static_cast<int>(converted_font.size()), converted_font.data());
        return Err(FontProviderError::kFontNotFound);
    }

    return BuildFontfaceInfo(std::move(ct_font));
}

auto FontProviderCoreText::GetFallbackFontFace(uint32_t ucs4, std::string_view base_family_name)
    -> Result<FontfaceInfo, FontProviderError> {
    if (ucs4 == 0 || unicode::IsPrivateUseArea(ucs4)) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    CTFontRef base_font = nullptr;
    if (cached_base_font_ &&
        cached_base_language_code_ == iso6392_language_code_ &&
        cached_base_family_name_ == base_family_name) {
        base_font = cached_base_font_.get();
    } else {
        ScopedCFRef<CTFontRef> resolved;
        if (!base_family_name.empty()) {
            resolved = ResolveCTFontByName(ConvertFamilyName(base_family_name, iso6392_language_code_));
        }
        if (!resolved) {
            resolved = ResolveCTFontByName(ConvertFamilyName("sans-serif", iso6392_language_code_));
        }
        if (!resolved) {
            resolved.reset(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 0, nullptr));
        }
        if (!resolved) {
            return Err(FontProviderError::kCodePointNotFound);
        }
        cached_base_family_name_ = base_family_name;
        cached_base_language_code_ = iso6392_language_code_;
        cached_base_font_ = std::move(resolved);
        base_font = cached_base_font_.get();
    }

    std::u16string utf16;
    size_t count = utf::UTF16AppendCodePoint(utf16, ucs4);
    ScopedCFRef<CFStringRef> cf_str(CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                                 reinterpret_cast<const UniChar*>(utf16.data()),
                                                                 static_cast<CFIndex>(count)));
    if (!cf_str) {
        return Err(FontProviderError::kOtherError);
    }

    ScopedCFRef<CFStringRef> cf_lang;
    if (iso6392_language_code_) {
        cf_lang = cfstr::StdStringToCFString(ISO6392ToISO6391LanguageString(iso6392_language_code_));
    }

    ScopedCFRef<CTFontRef> fallback_font(CTFontCreateForStringWithLanguage(base_font,
                                                                           cf_str.get(),
                                                                           CFRangeMake(0, static_cast<CFIndex>(count)),
                                                                           cf_lang.get()));
    if (!fallback_font) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    CGGlyph glyphs[2] = {0};
    if (!CTFontGetGlyphsForCharacters(fallback_font.get(),
                                      reinterpret_cast<const UniChar*>(utf16.data()),
                                      glyphs,
                                      static_cast<CFIndex>(count))) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    auto info_result = BuildFontfaceInfo(std::move(fallback_font));
    if (info_result.is_ok()) {
        const FontfaceInfo& info = info_result.value();
        if (info.family_name == ".LastResort" || info.family_name == "LastResort" || info.postscript_name == "LastResort") {
            info_result = Err(FontProviderError::kCodePointNotFound);
        }
    }
    return info_result;
}

}  // namespace aribcaption
