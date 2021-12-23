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

#ifndef ARIBCAPTION_FONT_PROVIDER_ANDROID_HPP
#define ARIBCAPTION_FONT_PROVIDER_ANDROID_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "aribcaption/context.hpp"
#include "base/logger.hpp"
#include "base/tinyxml2.h"
#include "renderer/font_provider.hpp"

namespace aribcaption {

namespace internal {

enum class FontVariant {
    kDefault = 0x01,
    kCompact = 0x02,
    kElegant = 0x04
};

struct FontFile {
    std::string filename;
    int weight = 0;
    bool is_italic = false;
    int collection_index = 0;
    std::string postscript_name;
};

struct FontFamily {
    std::vector<std::string> names;
    std::vector<FontFile> fonts;
    std::vector<std::string> languages;

    FontVariant variant = FontVariant::kDefault;

    bool is_fallback = false;
    std::string fallback_for;
};

}  // namespace internal

class FontProviderAndroid : public FontProvider {
public:
    explicit FontProviderAndroid(Context& context);
    ~FontProviderAndroid() override = default;
public:
    FontProviderType GetType() override;
    bool Initialize() override;
    void SetLanguage(uint32_t iso6392_language_code) override;
    auto GetFontFace(const std::string& font_name,
                     std::optional<uint32_t> ucs4) -> Result<FontfaceInfo, FontProviderError> override;
private:
    internal::FontFamily* FindFamilyByName(const char* name);
    internal::FontFamily* FindFallbackFamilyByLanguageAndFallbackFor(const char* lang,
                                                                     const char* fallback_for);
    bool ParseAndroidSystemFonts();
    bool ParseFontsXML(const char* xml_path);
    bool PrepareFontsForGingerbread();
    bool AnnotateLanguageForOldFamilySets();
private:
    bool CheckFileAndAppendFontFamily(const char* family_name, const char* filename, bool is_fallback);
private:
    bool HandleFamilySetLMP(tinyxml2::XMLElement* root);
    bool LMPHandleFamily(tinyxml2::XMLElement* element);
    bool LMPHandleFont(tinyxml2::XMLElement* element, internal::FontFamily& family);
    bool LMPHandleAlias(tinyxml2::XMLElement* element);
private:
    bool HandleFamilySetOLD(tinyxml2::XMLElement* root);
    static bool JBHandleFamily(tinyxml2::XMLElement* element, internal::FontFamily& family);
    static bool JBHandleNameset(tinyxml2::XMLElement* element, internal::FontFamily& family);
    static bool JBHandleFileset(tinyxml2::XMLElement* element, internal::FontFamily& family);
    static bool JBHandleFile(tinyxml2::XMLElement* element, internal::FontFamily& family);
private:
    std::shared_ptr<Logger> log_;

    std::string base_font_path_;
    std::vector<internal::FontFamily> font_families_;

    uint32_t iso6392_language_code_ = 0;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FONT_PROVIDER_ANDROID_HPP
