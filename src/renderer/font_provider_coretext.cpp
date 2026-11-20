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
#include <string_view>
#if TARGET_OS_IPHONE
    #include <CoreText/CoreText.h>
#else
    #include <ApplicationServices/ApplicationServices.h>
#endif
#include "base/cfstr_helper.hpp"
#include "base/scoped_cfref.hpp"
#include "base/scoped_holder.hpp"
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

auto FontProviderCoreText::GetFontFace(const std::string& font_name) -> Result<FontfaceInfo, FontProviderError> {
    std::string_view converted_font = ConvertFamilyName(font_name, iso6392_language_code_);
    ScopedCFRef<CFStringRef> fontname_request(cfstr::StdStringToCFString(converted_font));
    if (!fontname_request)
        return Err(FontProviderError::kOtherError);

    ScopedCFRef<CTFontDescriptorRef> matched_descriptor = MatchFontDescriptor(fontname_request.get(),
                                                                              kCTFontFamilyNameAttribute);
    if (!matched_descriptor) {
        matched_descriptor = MatchFontDescriptor(fontname_request.get(), kCTFontNameAttribute);
    }
    if (!matched_descriptor) {
        log_->w("CoreText: Cannot find font %.*s",
                static_cast<int>(converted_font.size()), converted_font.data());
        return Err(FontProviderError::kFontNotFound);
    }

    ScopedCFRef<CTFontRef> ct_font(CTFontCreateWithFontDescriptor(matched_descriptor.get(), 0, nullptr));
    if (!ct_font) {
        log_->e("CoreText: CTFontCreateWithFontDescriptor() returned with NULL");
        return Err(FontProviderError::kFontNotFound);
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
    info.family_name = cfstr::CFStringToStdString(cf_family_name.get());
    info.postscript_name = cfstr::CFStringToStdString(cf_postscript_name.get());
    info.filename = cfstr::CFStringToStdString(cf_path.get());;
    info.face_index = -1;
    info.provider_type = FontProviderType::kCoreText;

    auto priv = std::make_unique<FontfaceInfoPrivateCoreText>();
    priv->ct_font = std::move(ct_font);
    info.provider_priv = std::move(priv);

    return Ok(std::move(info));
}

}  // namespace aribcaption
