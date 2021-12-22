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

#include <cstring>
#include <cstdlib>
#include "renderer/font_provider_android.hpp"

using namespace tinyxml2;
using namespace aribcaption::internal;

inline constexpr const char* kAndroidFontsXML_LMP = "/system/etc/fonts.xml";
inline constexpr const char* kAndroidFontsXML_Old_System = "/system/etc/system_fonts.xml";
inline constexpr const char* kAndroidFontsXML_Old_Fallback = "/system/etc/fallback_fonts.xml";
inline constexpr const char* kAndroidFontsXML_Old_Fallback_JA = "/system/etc/fallback_fonts-ja.xml";
inline constexpr const char* kAndroidFontsXML_Old_Vendor = "/vendor/etc/fallback_fonts.xml";

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
    FontFamily* family = nullptr;
    FontFile* font_file = nullptr;

    if (iso6392_language_code_ == ThreeCC("jpn") || iso6392_language_code_ == 0) {
        family = FindFallbackFamilyByLanguageAndFallbackFor("ja", font_name.c_str());
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
        family = FindFamilyByName(font_name.c_str());
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
    FontFamily* found_family = nullptr;

    for (auto& family : font_families_) {
        for (auto& name : family.names) {
            if (name == search_name) {
                found_family = &family;
                break;
            }
        }
    }

    return found_family;
}

FontFamily* FontProviderAndroid::FindFallbackFamilyByLanguageAndFallbackFor(const char* lang,
                                                                            const char *fallback_for) {
    FontFamily* found_family = nullptr;

    for (auto& family : font_families_) {
        if (!family.is_fallback) {
            continue;
        }
        if (family.fallback_for != fallback_for) {
            continue;
        }
        for (auto& language : family.languages) {
            if (language == lang) {
                found_family = &family;
                break;
            }
        }
    }

    return found_family;
}

bool FontProviderAndroid::ParseAndroidSystemFonts() {
    if (!ParseFontsXML(kAndroidFontsXML_LMP)) {
        log_->w("FontProviderAndroid: Load or parse ", kAndroidFontsXML_LMP, " failed");

        if (!ParseFontsXML(kAndroidFontsXML_Old_System)) {
            log_->e("FontProviderAndroid: Load or parse ", kAndroidFontsXML_Old_System, " failed");
            return false;
        }

        if (!ParseFontsXML(kAndroidFontsXML_Old_Fallback_JA)) {
            if (!ParseFontsXML(kAndroidFontsXML_Old_Fallback)) {
                log_->e("FontProviderAndroid: Load or parse ", kAndroidFontsXML_Old_Fallback, " failed");
                return false;
            }
        }

        if (!ParseFontsXML(kAndroidFontsXML_Old_Vendor)) {
            log_->e("FontProviderAndroid: Load or parse ", kAndroidFontsXML_Old_Vendor, " failed");
            return false;
        }
    }

    return true;
}

bool FontProviderAndroid::ParseFontsXML(const char *xml_path) {
    XMLDocument doc(true, Whitespace::COLLAPSE_WHITESPACE);
    XMLError err = doc.LoadFile(xml_path);
    if (err != XMLError::XML_SUCCESS) {
        log_->w("FontProviderAndroid: Open ", xml_path, " failed");
        return false;
    }

    XMLElement* root = doc.RootElement();
    if (strcmp(root->Name(), "familyset") != 0) {
        log_->e("FontProviderAndroid: Root element must be familyset, found ", root->Name());
        return false;
    }

    const XMLAttribute* version_attr = root->FindAttribute("version");
    if (version_attr && version_attr->UnsignedValue() >= 21) {
        return HandleFamilySetLMP(root);
    } else {
        return HandleFamilySetOld(root);
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

bool FontProviderAndroid::HandleFamilySetLMP(XMLElement* root) {
    XMLElement* element = root->FirstChildElement();

    FontFamily current_family;

    while (element) {
        if (strcmp(element->Name(), "family") == 0) {
            current_family = FontFamily();
            if (element->FindAttribute("name")) {
                current_family.names.emplace_back(element->Attribute("name"));
            } else if (element->FindAttribute("lang")) {
                SplitByComma(element->Attribute("lang"), current_family.languages);
                current_family.is_fallback = true;
                current_family.fallback_for = "sans-serif";
                if (const char* variant = element->Attribute("variant")) {
                    if (strcmp(variant, "compact") == 0) {
                        current_family.variant = FontVariant::kCompact;
                    } else if (strcmp(variant, "elegant") == 0) {
                        current_family.variant = FontVariant::kElegant;
                    }
                }
            } else {
                // Ignore family without name or lang
                element = element->NextSiblingElement();
                continue;
            }
            // Visit child elements (font)
            element = element->FirstChildElement();
        } else if (strcmp(element->Name(), "font") == 0) {
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
                        FindFallbackFamilyByLanguageAndFallbackFor(current_family.languages[0].c_str(),
                                                                   fallback_for);
                if (!fallback_family) {
                    FontFamily& new_fallback_family = font_families_.emplace_back();
                    new_fallback_family.languages = current_family.languages;
                    new_fallback_family.variant = current_family.variant;
                    new_fallback_family.is_fallback = true;
                    new_fallback_family.fallback_for = fallback_for;
                    new_fallback_family.fonts.push_back(std::move(font));
                } else {
                    fallback_family->fonts.push_back(std::move(font));
                }
            } else {
                current_family.fonts.push_back(std::move(font));
            }

            XMLElement* next = element->NextSiblingElement();
            if (next) {
                // Visit next font element
                element = next;
            } else {
                // End of family element, store current_family
                font_families_.push_back(std::move(current_family));
                current_family = FontFamily();
                // Jump out of family element, move to next sibling element
                XMLElement* parent = element->Parent()->ToElement();
                element = parent->NextSiblingElement();
            }
        } else if (strcmp(element->Name(), "alias") == 0) {
            if (element->FindAttribute("name") && element->FindAttribute("to")) {
                if (const XMLAttribute* weight_attr = element->FindAttribute("weight")) {
                    FontFamily& new_family = font_families_.emplace_back();
                    FontFamily* target_family = FindFamilyByName(element->Attribute("to"));
                    if (!target_family) {
                        log_->e("FontProviderAndroid: Alias target not found: ", element->Attribute("to"));
                        // Skip
                        element = element->NextSiblingElement();
                        continue;
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
                        log_->e("FontProviderAndroid: Alias target not found: ", element->Attribute("to"));
                        // Skip
                        element = element->NextSiblingElement();
                        continue;
                    }
                    target_family->names.emplace_back(element->Attribute("name"));
                }
            }
            element = element->NextSiblingElement();
        }
    }

    return true;
}

bool FontProviderAndroid::HandleFamilySetOld(XMLElement* root) {
    // TODO
    return true;
}

}