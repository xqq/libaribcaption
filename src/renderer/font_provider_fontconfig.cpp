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

#include <cassert>
#include <cctype>
#include <string_view>
#include "base/language_code.hpp"
#include "base/scoped_holder.hpp"
#include "base/unicode_helper.hpp"
#include "renderer/font_provider_fontconfig.hpp"

#ifndef FC_POSTSCRIPT_NAME
    #define FC_POSTSCRIPT_NAME "postscriptname"
#endif

namespace aribcaption {

FontProviderFontconfig::FontProviderFontconfig(Context& context) :
      log_(GetContextLogger(context)) {}

FontProviderFontconfig::~FontProviderFontconfig() = default;

FontProviderType FontProviderFontconfig::GetType() {
    return FontProviderType::kFontconfig;
}

bool FontProviderFontconfig::Initialize() {
    FcConfig* config = nullptr;
    if (!(config = FcInitLoadConfigAndFonts())) {
        log_->e("Fontconfig: FcInitLoadConfigAndFonts() failed");
        return false;
    }
    config_ = ScopedHolder<FcConfig*>(config, FcConfigDestroy);
    return true;
}

void FontProviderFontconfig::SetLanguage(uint32_t iso6392_language_code) {
    if (iso6392_language_code_ != iso6392_language_code) {
        has_cached_fallback_ = false;
        cached_fallback_base_family_.clear();
        cached_fallback_lang_ = 0;
        fallback_candidates_.clear();
        fallback_sort_set_.Reset();
    }
    iso6392_language_code_ = iso6392_language_code;
}

static bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static bool EqualsIgnoreCaseAndBlanks(std::string_view a, std::string_view b) {
    size_t ia = 0;
    size_t ib = 0;

    while (ia < a.size() || ib < b.size()) {
        while (ia < a.size() && (a[ia] == ' ' || a[ia] == '\t'))
            ia++;
        while (ib < b.size() && (b[ib] == ' ' || b[ib] == '\t'))
            ib++;

        if (ia == a.size() || ib == b.size()) {
            return ia == a.size() && ib == b.size();
        }
        if (std::tolower(static_cast<unsigned char>(a[ia])) != std::tolower(static_cast<unsigned char>(b[ib]))) {
            return false;
        }
        ia++;
        ib++;
    }

    return true;
}

static bool IsGenericFamilyName(std::string_view name) {
    size_t colon = name.find(':');
    if (colon != std::string_view::npos) {
        name = name.substr(0, colon);
    }
    static constexpr std::string_view kGenericFamilies[] = {
        "sans-serif", "sans", "serif", "monospace", "mono",
        "cursive", "fantasy", "system-ui", "ui-sans-serif",
        "ui-serif", "ui-monospace", "ui-rounded", "emoji", "math", "fangsong",
    };
    for (std::string_view generic : kGenericFamilies) {
        if (EqualsIgnoreCase(name, generic)) {
            return true;
        }
    }
    return false;
}

static bool PatternContainsName(FcPattern* font_pattern, const char* object, std::string_view expected) {
    for (int i = 0;; i++) {
        FcChar8* value = nullptr;
        if (FcPatternGetString(font_pattern, object, i, &value) != FcResultMatch) {
            break;
        }
        if (value && EqualsIgnoreCaseAndBlanks(reinterpret_cast<const char*>(value), expected)) {
            return true;
        }
    }
    return false;
}

static bool MatchesConcreteFamily(FcPattern* request_pattern, FcPattern* matched_font, std::string_view requested_name) {
    auto font_matches = [&](std::string_view candidate) -> bool {
        return PatternContainsName(matched_font, FC_FAMILY, candidate) ||
               PatternContainsName(matched_font, FC_FULLNAME, candidate) ||
               PatternContainsName(matched_font, FC_POSTSCRIPT_NAME, candidate);
    };

    if (font_matches(requested_name)) {
        return true;
    }

#if defined(FC_VERSION) && FC_VERSION >= 21205
    // Accept explicit strong/same Fontconfig aliases, but reject weak generic fallbacks
    for (int i = 0;; i++) {
        FcValue val{};
        FcValueBinding binding = FcValueBindingWeak;
        if (FcPatternGetWithBinding(request_pattern, FC_FAMILY, i, &val, &binding) != FcResultMatch) {
            break;
        }
        if (binding == FcValueBindingStrong && val.type == FcTypeString && val.u.s) {
            if (font_matches(reinterpret_cast<const char*>(val.u.s))) {
                return true;
            }
        }
    }
#else
    (void)request_pattern;
#endif

    return false;
}

static bool IsLastResortFont(std::string_view family_name) {
    return EqualsIgnoreCase(family_name, ".LastResort") || EqualsIgnoreCase(family_name, "LastResort");
}

static auto BuildFontfaceInfo(FcPattern* best, Logger& log, std::string_view label)
    -> Result<FontfaceInfo, FontProviderError> {
    FcChar8* filename = nullptr;
    if (FcResultMatch != FcPatternGetString(best, FC_FILE, 0, &filename)) {
        log.e("Fontconfig: Retrieve font filename failed for %.*s", static_cast<int>(label.size()), label.data());
        return Err(FontProviderError::kOtherError);
    }

    int fc_index = 0;
    if (FcResultMatch != FcPatternGetInteger(best, FC_INDEX, 0, &fc_index)) {
        log.e("Fontconfig: Retrieve font FC_INDEX failed for %.*s", static_cast<int>(label.size()), label.data());
        return Err(FontProviderError::kOtherError);
    }

    FcChar8* fc_family_name = nullptr;
    if (FcResultMatch != FcPatternGetString(best, FC_FAMILY, 0, &fc_family_name)) {
        log.e("Fontconfig: Retrieve font FC_FAMILY failed for %.*s", static_cast<int>(label.size()), label.data());
        return Err(FontProviderError::kOtherError);
    }

    std::string postscript_name;
    FcChar8* fc_postscript_name = nullptr;
    if (FcResultMatch == FcPatternGetString(best, FC_POSTSCRIPT_NAME, 0, &fc_postscript_name) && fc_postscript_name) {
        postscript_name = reinterpret_cast<char*>(fc_postscript_name);
    }

    FontfaceInfo info;
    info.family_name = reinterpret_cast<char*>(fc_family_name);
    info.postscript_name = std::move(postscript_name);
    info.filename = reinterpret_cast<char*>(filename);
    info.face_index = fc_index;
    info.provider_type = FontProviderType::kFontconfig;

    return Ok(std::move(info));
}

auto FontProviderFontconfig::GetFontFace(const std::string& font_name) -> Result<FontfaceInfo, FontProviderError> {
    assert(config_);

    ScopedHolder<FcPattern*> pattern(
        FcNameParse(reinterpret_cast<const FcChar8*>(font_name.c_str())),
        FcPatternDestroy
    );
    if (!pattern) {
        log_->e("Fontconfig: Cannot parse font pattern string");
        return Err(FontProviderError::kFontNotFound);
    }

    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(font_name.c_str()));
    FcPatternAddBool(pattern, FC_OUTLINE, FcTrue);

    if (FcTrue != FcConfigSubstitute(config_, pattern, FcMatchPattern)) {
        log_->e("Fontconfig: Substitution cannot be performed");
        return Err(FontProviderError::kOtherError);
    }
    FcDefaultSubstitute(pattern);

    FcPatternDel(pattern, FC_LANG);
    if (iso6392_language_code_) {
        ScopedHolder<FcLangSet*> fc_langset(FcLangSetCreate(), FcLangSetDestroy);
        FcLangSetAdd(fc_langset,
                     reinterpret_cast<const FcChar8*>(ISO6392ToISO6391LanguageString(iso6392_language_code_)));
        FcPatternAddLangSet(pattern, FC_LANG, fc_langset);
    }

    FcResult result = FcResultMatch;
    FcPattern* matched = FcFontMatch(config_, pattern, &result);
    if (!matched || result != FcResultMatch) {
        log_->w("Fontconfig: Cannot find a suitable font for %s", font_name.c_str());
        return Err(FontProviderError::kFontNotFound);
    }

    ScopedHolder<FcPattern*> best(matched, FcPatternDestroy);

    if (!IsGenericFamilyName(font_name) && !MatchesConcreteFamily(pattern, best, font_name)) {
        log_->w("Fontconfig: Concrete font family %s not found", font_name.c_str());
        return Err(FontProviderError::kFontNotFound);
    }

    return BuildFontfaceInfo(best, *log_, font_name);
}

auto FontProviderFontconfig::EnsureFallbackCandidates(std::string_view base_family)
    -> std::optional<FontProviderError> {
    if (has_cached_fallback_ &&
        cached_fallback_lang_ == iso6392_language_code_ &&
        cached_fallback_base_family_ == base_family) {
        return std::nullopt;
    }

    ScopedHolder<FcPattern*> pattern(FcPatternCreate(), FcPatternDestroy);
    if (!pattern) {
        return FontProviderError::kOtherError;
    }

    if (!base_family.empty()) {
        std::string base_family_str(base_family);
        FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(base_family_str.c_str()));
    }
    FcPatternAddBool(pattern, FC_OUTLINE, FcTrue);

    if (FcTrue != FcConfigSubstitute(config_, pattern, FcMatchPattern)) {
        log_->e("Fontconfig: Substitution cannot be performed");
        return FontProviderError::kOtherError;
    }
    FcDefaultSubstitute(pattern);

    FcPatternDel(pattern, FC_LANG);
    if (iso6392_language_code_) {
        ScopedHolder<FcLangSet*> fc_langset(FcLangSetCreate(), FcLangSetDestroy);
        FcLangSetAdd(fc_langset,
                     reinterpret_cast<const FcChar8*>(ISO6392ToISO6391LanguageString(iso6392_language_code_)));
        FcPatternAddLangSet(pattern, FC_LANG, fc_langset);
    }

    // Request an untrimmed sort so earlier bitmap-only or placeholder fonts (e.g. .LastResort)
    // cannot trim away valid outline fonts behind them before we filter by FC_OUTLINE.
    FcResult result = FcResultMatch;
    FcFontSet* raw_sorted = FcFontSort(config_, pattern, FcFalse, nullptr, &result);
    ScopedHolder<FcFontSet*> sorted(raw_sorted, FcFontSetDestroy);
    if (!sorted || result != FcResultMatch) {
        return FontProviderError::kCodePointNotFound;
    }

    ScopedHolder<FcCharSet*> covered(FcCharSetCreate(), FcCharSetDestroy);
    if (!covered) {
        return FontProviderError::kOtherError;
    }

    std::vector<FallbackCandidate> candidates;
    for (int i = 0; i < sorted->nfont; i++) {
        FcPattern* font = sorted->fonts[i];
        if (!font) {
            continue;
        }

        FcBool is_outline = FcTrue;
        if (FcPatternGetBool(font, FC_OUTLINE, 0, &is_outline) == FcResultMatch && is_outline != FcTrue) {
            continue;
        }

        FcChar8* fc_family_name = nullptr;
        if (FcResultMatch == FcPatternGetString(font, FC_FAMILY, 0, &fc_family_name) &&
            fc_family_name &&
            IsLastResortFont(reinterpret_cast<const char*>(fc_family_name))) {
            continue;
        }

        FcCharSet* charset = nullptr;
        if (FcResultMatch != FcPatternGetCharSet(font, FC_CHARSET, 0, &charset) || !charset) {
            continue;
        }

        FcBool changed = FcFalse;
        if (FcTrue != FcCharSetMerge(covered, charset, &changed)) {
            return FontProviderError::kOtherError;
        }
        if (!changed) {
            continue;
        }

        candidates.push_back({font, charset});
    }

    fallback_candidates_.clear();
    fallback_sort_set_ = std::move(sorted);
    fallback_candidates_ = std::move(candidates);
    cached_fallback_base_family_ = base_family;
    cached_fallback_lang_ = iso6392_language_code_;
    has_cached_fallback_ = true;
    return std::nullopt;
}

auto FontProviderFontconfig::GetFallbackFontFace(uint32_t ucs4, std::string_view base_family_name)
    -> Result<FontfaceInfo, FontProviderError> {
    assert(config_);

    if (ucs4 == 0 || unicode::IsPrivateUseArea(ucs4)) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    // base_family_name may be a pattern string such as "Family:style=Regular", keep only the family part.
    // FcNameParse() is not used because it also treats '-' as a size separator ("sans-serif" -> "sans").
    std::string_view base_family = base_family_name.substr(0, base_family_name.find(':'));
    if (auto err = EnsureFallbackCandidates(base_family)) {
        return Err(*err);
    }

    for (const auto& candidate : fallback_candidates_) {
        if (FcTrue == FcCharSetHasChar(candidate.charset, ucs4)) {
            return BuildFontfaceInfo(candidate.pattern, *log_, "fallback");
        }
    }

    return Err(FontProviderError::kCodePointNotFound);
}

}  // namespace aribcaption
