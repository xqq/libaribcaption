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

#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "renderer/font_provider_android.hpp"

using namespace tinyxml2;
using namespace aribcaption::internal;

inline constexpr const char* kAndroidFontsXML_LMP = "/system/etc/fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_System = "/system/etc/system_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Fallback = "/system/etc/fallback_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Fallback_JA = "/system/etc/fallback_fonts-ja.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Vendor = "/vendor/etc/fallback_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Vendor_JA = "/vendor/etc/fallback_fonts-ja.xml";

namespace aribcaption {

FontProviderAndroid::FontProviderAndroid(Context& context) : log_(GetContextLogger(context)) {}

FontProviderType FontProviderAndroid::GetType() {
    return FontProviderType::kAndroid;
}

bool FontProviderAndroid::Initialize() {
    base_font_path_ = getenv("ANDROID_ROOT");
    base_font_path_.append("/fonts/");

    bool ret = ParseAndroidSystemFonts();
    return ret;
}

void FontProviderAndroid::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

auto FontProviderAndroid::GetFontFace(const std::string &font_name,
                                      std::optional<uint32_t> ucs4) -> Result<FontfaceInfo, FontProviderError> {
    FontFile* font_file = nullptr;

    if (iso6392_language_code_ == ThreeCC("jpn") || iso6392_language_code_ == 0) {
        FontFamily* family = FindFallbackFamilyByLanguageAndFallbackFor("ja", font_name.c_str());
        if (!family) {
            family = FindFallbackFamilyByLanguageAndFallbackFor("ja", "sans-serif");
        }
        if (!family) {
            return Err(FontProviderError::kFontNotFound);
        }
        for (auto& font : family->fonts) {
            if (font.weight == 400 && !font.is_italic) {
                font_file = &font;
                break;
            }
        }
    } else {
        FontFamily* family = FindFamilyByName(font_name.c_str());
        if (!family) {
            return Err(FontProviderError::kFontNotFound);
        }
        for (auto& font : family->fonts) {
            if (font.weight == 400 && !font.is_italic) {
                font_file = &font;
                break;
            }
        }
    }

    FontfaceInfo info;
    info.family_name = font_name;
    info.postscript_name = font_file->postscript_name;
    info.filename = base_font_path_ + font_file->filename;
    info.face_index = font_file->collection_index;
    info.provider_type = FontProviderType::kAndroid;

    return Ok(std::move(info));
}

FontFamily* FontProviderAndroid::FindFamilyByName(const char *search_name) {
    for (auto& family : font_families_) {
        for (auto& name : family.names) {
            if (name == search_name) {
                return &family;
            }
        }
    }

    return nullptr;
}

FontFamily* FontProviderAndroid::FindFallbackFamilyByLanguageAndFallbackFor(const char* lang,
                                                                            const char *fallback_for) {
    for (auto& family : font_families_) {
        if (!family.is_fallback) {
            continue;
        }
        if (family.fallback_for != fallback_for) {
            continue;
        }
        for (auto& language : family.languages) {
            if (language == lang) {
                return &family;
            }
        }
    }

    return nullptr;
}

bool FontProviderAndroid::ParseAndroidSystemFonts() {
    // First, try parse the fonts.xml with new format (Lollipop+)
    // /system/etc/fonts.xml
    if (!ParseFontsXML(kAndroidFontsXML_LMP)) {
        log_->w("FontProviderAndroid: Load Lollipop+ config %s failed", kAndroidFontsXML_LMP);

        bool legacy_config_load_succeed = false;

        // If failed, fallback to parse the old system_fonts.xml & fallback_fonts.xml
        // /system/etc/system_fonts.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_System)) {
            log_->w("FontProviderAndroid: Load legacy config %s failed", kAndroidFontsXML_OLD_System);
        } else {
            legacy_config_load_succeed = true;
        }

        // /system/etc/fallback_fonts-ja.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_Fallback_JA)) {
            // /system/etc/fallback_fonts.xml
            if (!ParseFontsXML(kAndroidFontsXML_OLD_Fallback)) {
                log_->w("FontProviderAndroid: Load legacy fallback config %s failed", kAndroidFontsXML_OLD_Fallback);
            }
        }

        // /vendor/etc/fallback_fonts-ja.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_Vendor_JA)) {
            // /vendor/etc/fallback_fonts.xml
            if (!ParseFontsXML(kAndroidFontsXML_OLD_Vendor)) {
                log_->v("FontProviderAndroid: Cannot load legacy vendor config %s", kAndroidFontsXML_OLD_Vendor);
            }
        }

        // If failed, consider Android version is below 4.x (e.g. 2.x, 3.x)
        if (!legacy_config_load_succeed) {
            if (!PrepareFontsForGingerbread()) {
                log_->w("FontProviderAndroid: Search fonts for Android 2.x (Gingerbread) failed");
                return false;
            }
        }

        AnnotateLanguageForOldFamilySets();
    }

    return true;
}

static bool CheckFileExists(const char* filename) {
    return access(filename, R_OK) == 0;
}

bool FontProviderAndroid::CheckFileAndAppendFontFamily(const char* family_name, const char* filename, bool is_fallback) {
    std::string full_filename = base_font_path_ + filename;
    if (!CheckFileExists(full_filename.c_str())) {
        return false;
    }

    FontFamily& family = font_families_.emplace_back();
    if (family_name) {
        family.names.emplace_back(family_name);
    }
    if (is_fallback) {
        family.is_fallback = true;
        family.fallback_for = "sans-serif";
    }

    FontFile& font = family.fonts.emplace_back();
    font.filename = filename;
    font.weight = 400;
    return true;
}

bool FontProviderAndroid::PrepareFontsForGingerbread() {
    bool ret = CheckFileAndAppendFontFamily("sans-serif", "DroidSans.ttf", false);
    if (!ret) {
        return false;
    }

    CheckFileAndAppendFontFamily("serif", "DroidSerif-Regular.ttf", false);
    CheckFileAndAppendFontFamily("monospace", "DroidSansMono.ttf", false);

    CheckFileAndAppendFontFamily(nullptr, "MTLmr3m.ttf", true);
    CheckFileAndAppendFontFamily(nullptr, "DroidSansJapanese.ttf", true);
    CheckFileAndAppendFontFamily(nullptr, "DroidSansFallback.ttf", true);
    return true;
}

static bool AnnotateJAForSpecificFontFile(std::vector<FontFamily>& font_families, const char* font_filename) {
    bool detected_filename = false;

    for (auto& family : font_families) {
        for (auto& font : family.fonts) {
            if (font.filename == font_filename) {
                detected_filename = true;
                family.languages.emplace_back("ja");
            }
        }
    }

    return detected_filename;
}

bool FontProviderAndroid::AnnotateLanguageForOldFamilySets() {
    bool detected_ja_font = false;

    for (auto& family : font_families_) {
        for (auto& lang : family.languages) {
            if (lang == "ja") detected_ja_font = true;
        }
    }

    if (detected_ja_font) {
        return true;
    }
    // else: annotate lang="ja" for MTLmr3m.ttf, DroidSansJapanese.ttf or DroidSansFallback.ttf

    bool ret = AnnotateJAForSpecificFontFile(font_families_, "MTLmr3m.ttf");
    if (ret) {
        return true;
    }

    ret = AnnotateJAForSpecificFontFile(font_families_, "DroidSansJapanese.ttf");
    if (ret) {
        return true;
    }

    ret = AnnotateJAForSpecificFontFile(font_families_, "DroidSansFallback.ttf");
    if (ret) {
        return true;
    }

    return false;
}

bool FontProviderAndroid::ParseFontsXML(const char *xml_path) {
    XMLDocument doc(true, Whitespace::COLLAPSE_WHITESPACE);
    XMLError err = doc.LoadFile(xml_path);
    if (err != XMLError::XML_SUCCESS) {
        log_->v("FontProviderAndroid: Open %s failed", xml_path);
        return false;
    }

    XMLElement* root = doc.RootElement();
    if (strcmp(root->Name(), "familyset") != 0) {
        log_->e("FontProviderAndroid: Root element must be familyset, found %s", root->Name());
        return false;
    }

    const XMLAttribute* version_attr = root->FindAttribute("version");
    if (version_attr && version_attr->UnsignedValue() >= 21) {
        return HandleFamilySetLMP(root);
    } else {
        return HandleFamilySetOLD(root);
    }
}

static void SplitByComma(const char* input, std::vector<std::string>& out) {
    std::string str;

    for (const char* s = input; *s != 0; s++) {
        if (*s == ',') {
            out.push_back(std::move(str));
            str = std::string();
        } else if (*s == ' ' || *s == '\t') {
            continue;
        } else {
            str.push_back(*s);
        }
    }

    if (!str.empty()) {
        out.push_back(std::move(str));
    }
}

// Android 5.0+ fonts.xml
bool FontProviderAndroid::HandleFamilySetLMP(XMLElement* root) {
    XMLElement* element = root->FirstChildElement();

    FontFamily current_family;

    while (element) {
        if (strcmp(element->Name(), "family") == 0) {
            LMPHandleFamily(element);
        } else if (strcmp(element->Name(), "alias") == 0) {
            LMPHandleAlias(element);
        }
        element = element->NextSiblingElement();
    }

    return true;
}

bool FontProviderAndroid::LMPHandleFamily(XMLElement* element) {
    FontFamily& current_family = font_families_.emplace_back();

    if (const char* name = element->Attribute("name")) {
        current_family.names.emplace_back(name);
    }

    if (const char* lang = element->Attribute("lang")) {
        SplitByComma(lang, current_family.languages);
        current_family.is_fallback = true;
        current_family.fallback_for = "sans-serif";
    }

    if (const char* variant = element->Attribute("variant")) {
        if (strcmp(variant, "compact") == 0) {
            current_family.variant = FontVariant::kCompact;
        } else if (strcmp(variant, "elegant") == 0) {
            current_family.variant = FontVariant::kElegant;
        }
    }

    element = element->FirstChildElement();

    while (element) {
        if (strcmp(element->Name(), "font") == 0) {
            LMPHandleFont(element, current_family);
        }
        element = element->NextSiblingElement();
    }

    return true;
}

bool FontProviderAndroid::LMPHandleFont(XMLElement* element, internal::FontFamily& family) {
    FontFile font;
    font.filename = element->GetText();
    font.weight = element->IntAttribute("weight", 400);
    font.collection_index = element->IntAttribute("index", 0);

    if (const char* style = element->Attribute("style")) {
        if (strcmp(style, "italic") == 0) {
            font.is_italic = true;
        }
    }

    if (const char* postscript_name = element->Attribute("postScriptName")) {
        font.postscript_name = postscript_name;
    }

    if (const char* fallback_for = element->Attribute("fallbackFor")) {
        FontFamily* fallback_family =
                FindFallbackFamilyByLanguageAndFallbackFor(family.languages[0].c_str(),
                                                           fallback_for);
        if (!fallback_family) {
            FontFamily& new_fallback_family = font_families_.emplace_back();
            new_fallback_family.languages = family.languages;
            new_fallback_family.variant = family.variant;
            new_fallback_family.is_fallback = true;
            new_fallback_family.fallback_for = fallback_for;
            new_fallback_family.fonts.push_back(std::move(font));
        } else {
            fallback_family->fonts.push_back(std::move(font));
        }
    } else {
        family.fonts.push_back(std::move(font));
    }

    return true;
}

bool FontProviderAndroid::LMPHandleAlias(tinyxml2::XMLElement* element) {
    if (!element->FindAttribute("name") | !element->FindAttribute("to")) {
        log_->e("FontProviderAndroid: Missing name/to for alias in fonts.xml");
        return false;
    }

    if (const XMLAttribute* weight_attr = element->FindAttribute("weight")) {
        FontFamily& new_family = font_families_.emplace_back();
        FontFamily* target_family = FindFamilyByName(element->Attribute("to"));
        if (!target_family) {
            log_->e("FontProviderAndroid: Alias target not found: %s", element->Attribute("to"));
            return false;
        }

        new_family.names = {element->Attribute("name")};
        new_family.languages = target_family->languages;
        new_family.variant = target_family->variant;
        new_family.is_fallback = target_family->is_fallback;
        new_family.fallback_for = target_family->fallback_for;

        int request_weight = weight_attr->IntValue();

        for (auto& font : target_family->fonts) {
            if (font.weight == request_weight) {
                new_family.fonts.push_back(font);
            }
        }
    } else {
        FontFamily* target_family = FindFamilyByName(element->Attribute("to"));
        if (!target_family) {
            log_->e("FontProviderAndroid: Alias target not found: %s", element->Attribute("to"));
            return false;
        }
        target_family->names.emplace_back(element->Attribute("name"));
    }

    return true;
}

// Before Android 5.0, system_fonts.xml / fallback_fonts.xml
bool FontProviderAndroid::HandleFamilySetOLD(XMLElement* root) {
    XMLElement* element = root->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "family") == 0) {
            FontFamily family;
            if (JBHandleFamily(element, family)) {
                // Parse success
                font_families_.push_back(std::move(family));
            }
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFamily(XMLElement* element, FontFamily& family) {
    bool has_nameset = false;
    element = element->FirstChildElement();

    while (element) {
        if (strcmp(element->Name(), "nameset") == 0) {
            has_nameset = true;
            if (!JBHandleNameset(element, family))
                return false;
        } else if (strcmp(element->Name(), "fileset") == 0) {
            if (!JBHandleFileset(element, family))
                return false;
        }
        element = element->NextSiblingElement();
    }

    if (!has_nameset) {
        family.is_fallback = true;
        family.fallback_for = "sans-serif";
    }

    return true;
}

bool FontProviderAndroid::JBHandleNameset(XMLElement* element, FontFamily& family) {
    element = element->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "name") == 0) {
            family.names.emplace_back(element->GetText());
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFileset(XMLElement* element, FontFamily& family) {
    element = element->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "file") == 0) {
            if (!JBHandleFile(element, family))
                return false;
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFile(XMLElement* element, FontFamily& family) {
    FontFile& font = family.fonts.emplace_back();
    font.filename = element->GetText();

    std::string filename_lcase(font.filename.length(), 0);
    std::transform(font.filename.begin(),
                   font.filename.end(),
                   filename_lcase.begin(),
                   [](char c) { return std::tolower(c); });

    if (filename_lcase.find("thin") != std::string::npos) {
        font.weight = 100;
    } else if (filename_lcase.find("light") != std::string::npos) {
        font.weight = 300;
    } else if (filename_lcase.find("regular") != std::string::npos) {
        font.weight = 400;
    } else if (filename_lcase.find("medium") != std::string::npos) {
        font.weight = 500;
    } else if (filename_lcase.find("black") != std::string::npos) {
        font.weight = 900;
    } else if (filename_lcase.find("bold") != std::string::npos) {
        font.weight = 700;
    } else {
        // default as 400 (Regular)
        font.weight = 400;
    }

    if (filename_lcase.find("italic") != std::string::npos) {
        font.is_italic = true;
    }

    if (const char* lang = element->Attribute("lang")) {
        auto& languages = family.languages;
        if (languages.empty() || std::find(languages.begin(), languages.end(), lang) == languages.end()) {
            languages.emplace_back(lang);
        }
    }

    if (const char* variant = element->Attribute("variant")) {
        if (strcmp(variant, "elegant") == 0) {
            family.variant = FontVariant::kElegant;
        } else if (strcmp(variant, "compact") == 0) {
            family.variant = FontVariant::kCompact;
        }
    }

    return true;
}

}  // namespace aribcaption
