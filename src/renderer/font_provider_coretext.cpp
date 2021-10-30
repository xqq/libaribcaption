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
#if TARGET_OS_IPHONE
    #include <CoreText/CoreText.h>
#else
    #include <ApplicationServices/ApplicationServices.h>
#endif
#include "base/scoped_cfref.hpp"
#include "base/scoped_holder.hpp"
#include "base/utf_helper.hpp"
#include "renderer/font_provider_coretext.hpp"

namespace aribcaption {

FontProviderCoreText::FontProviderCoreText(Context& context) : log_(GetContextLogger(context)) {}

bool FontProviderCoreText::Initialize() {
    return true;
}

void FontProviderCoreText::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

static std::string CFStringToStdString(CFStringRef cfstr) {
    std::string str;

    if (!cfstr) {
        return str;
    }

    const char* cstring_ptr = CFStringGetCStringPtr(cfstr, kCFStringEncodingUTF8);

    if (cstring_ptr) {
        str = cstring_ptr;
    } else {  // cstring_ptr == nullptr
        CFIndex buf_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
        str.resize(buf_size);
        CFStringGetCString(cfstr, str.data(), buf_size, kCFStringEncodingUTF8);
        size_t len = strlen(str.data());
        str.resize(len);
    }

    return str;
}

static ScopedCFRef<CFStringRef> StdStringToCFString(const std::string& str) {
    ScopedCFRef<CFStringRef> cfstr(CFStringCreateWithCString(nullptr, str.c_str(), kCFStringEncodingUTF8));
    return cfstr;
}

static std::string ConvertFamilyName(const std::string& family_name, uint32_t iso6392_language_code) {
    std::string font_name = family_name;

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

auto FontProviderCoreText::GetFontFace(const std::string& font_name,
                                       std::optional<uint32_t> ucs4) -> Result<FontfaceInfo, FontProviderError> {
    std::string converted_font = ConvertFamilyName(font_name, iso6392_language_code_);
    ScopedCFRef<CFStringRef> fontname_request(StdStringToCFString(converted_font));
    if (!fontname_request)
        return Err(FontProviderError::kOtherError);

    ScopedCFRef<CFMutableDictionaryRef> cf_attributes(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                                0,
                                                                                &kCFTypeDictionaryKeyCallBacks,
                                                                                &kCFTypeDictionaryValueCallBacks));
    ScopedCFRef<CFMutableDictionaryRef> cf_traits(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                            0,
                                                                            &kCFTypeDictionaryKeyCallBacks,
                                                                            &kCFTypeDictionaryValueCallBacks));
    if (!cf_attributes || !cf_traits)
        return Err(FontProviderError::kOtherError);

    // Set expected font weight to Regular/Medium (could be overrided by Font family name)
    CGFloat font_weight = 0.0f;
    ScopedCFRef<CFNumberRef> cf_font_weight(CFNumberCreate(nullptr, kCFNumberFloatType, &font_weight));
    CFDictionaryAddValue(cf_traits.get(), kCTFontWeightTrait, cf_font_weight.get());

    // Set cf_traits into cf_attributes
    CFDictionaryAddValue(cf_attributes.get(), kCTFontTraitsAttribute, cf_traits.get());

    // Set requested font name
    CFDictionaryAddValue(cf_attributes.get(), kCTFontFamilyNameAttribute, fontname_request.get());

    // Create font descriptor
    ScopedCFRef<CTFontDescriptorRef> descriptor_for_find(CTFontDescriptorCreateWithAttributes(cf_attributes.get()));
    if (!descriptor_for_find)
        return Err(FontProviderError::kOtherError);

    // Create CTFont by specified descriptor with attributes
    ScopedCFRef<CTFontRef> ct_font(CTFontCreateWithFontDescriptor(descriptor_for_find.get(), 0, nullptr));
    if (!ct_font) {
        log_->e("CoreText: CTFontCreateWithFontDescriptor() returned with NULL");
        return Err(FontProviderError::kFontNotFound);
    }

    // Check whether the font contains the required codepoint if needed
    if (ucs4.has_value()) {
        std::u16string utf16;
        size_t count = utf::UTF16AppendCodePoint(utf16, ucs4.value());
        CGGlyph glyphs[2] = {0};
        bool ret = CTFontGetGlyphsForCharacters(ct_font.get(),
                                                reinterpret_cast<UniChar*>(utf16.data()),
                                                glyphs,
                                                static_cast<CFIndex>(count));
        if (!ret) {
            log_->w("CoreText: Font ", converted_font, " doesn't contain U+", std::hex, ucs4.value());
            return Err(FontProviderError::kCodePointNotFound);
        }
    }

    // Retrieve descriptor associated with the CTFont
    ScopedCFRef<CTFontDescriptorRef> ct_font_descriptor(CTFontCopyFontDescriptor(ct_font.get()));
    if (!ct_font_descriptor)
        return Err(FontProviderError::kOtherError);

    // Retrieve font filename (path) from descriptor
    ScopedCFRef<CFURLRef> cf_url(static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(ct_font_descriptor.get(),
                                                                                     kCTFontURLAttribute)));
    if (!cf_url)
        return Err(FontProviderError::kOtherError);

    ScopedCFRef<CFStringRef> cf_path(CFURLCopyFileSystemPath(cf_url.get(), kCFURLPOSIXPathStyle));
    if (!cf_path)
        return Err(FontProviderError::kOtherError);

    // Retrieve font family name from descriptor
    ScopedCFRef<CFStringRef> cf_family_name(
        static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(ct_font_descriptor.get(), kCTFontFamilyNameAttribute)));
    if (!cf_family_name)
        return Err(FontProviderError::kOtherError);

    // Retrieve font PostScript name from descriptor
    ScopedCFRef<CFStringRef> cf_postscript_name(
        static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(ct_font_descriptor.get(), kCTFontNameAttribute)));
    if (!cf_postscript_name)
        return Err(FontProviderError::kOtherError);

    FontfaceInfo info;
    info.family_name = CFStringToStdString(cf_family_name.get());
    info.postscript_name = CFStringToStdString(cf_postscript_name.get());
    info.filename = CFStringToStdString(cf_path.get());;
    info.face_index = -1;
    info.provider_type = FontProviderType::kCoreText;

    auto priv = std::make_unique<FontProviderCoreTextPrivate>();
    priv->ct_font = std::move(ct_font);
    info.provider_priv = std::move(priv);

    return Ok(std::move(info));
}

}  // namespace aribcaption
