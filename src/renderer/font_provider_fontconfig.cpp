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
#include "base/scoped_holder.hpp"
#include "renderer/font_provider_fontconfig.hpp"

namespace aribcaption {

FontProviderFontconfig::FontProviderFontconfig(Context& context) :
      log_(GetContextLogger(context)) {}

FontProviderFontconfig::~FontProviderFontconfig() {
    if (config_) {
        FcConfigDestroy(config_);
        config_ = nullptr;
    }
}

bool FontProviderFontconfig::Initialize() {
    if (!(config_ = FcInitLoadConfigAndFonts())) {
        log_->e("Fontconfig: FcInitLoadConfigAndFonts() failed");
        return false;
    }
    return true;
}

std::optional<FontfaceInfo> FontProviderFontconfig::GetFontFace(const std::string& font_name,
                                                                std::optional<uint32_t> ucs4) {
    assert(config_);
    std::optional<FontfaceInfo> ret;

    ScopedHolder<FcPattern*> pattern(
        FcNameParse(reinterpret_cast<const FcChar8*>(font_name.c_str())),
        FcPatternDestroy
    );
    if (!pattern) {
        log_->e("Fontconfig: Cannot parse font pattern string");
        return ret;
    }

    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(font_name.c_str()));
    FcPatternAddBool(pattern, FC_OUTLINE, FcTrue);

    if (FcTrue != FcConfigSubstitute(config_, pattern, FcMatchPattern)) {
        log_->e("Fontconfig: Substitution cannot be performed");
        return ret;  // std::nullopt
    }
    FcDefaultSubstitute(pattern);

    FcPatternDel(pattern, FC_LANG);

    FcResult result = FcResultMatch;
    FcPattern* matched = FcFontMatch(config_, pattern, &result);
    if (!matched || result != FcResultMatch) {
        log_->w("Fontconfig: Cannot find a suitable font for ", font_name);
        return ret;  // std::nullopt
    }

    ScopedHolder<FcPattern*> best(matched, FcPatternDestroy);

    FcChar8* filename = nullptr;
    if (FcResultMatch != FcPatternGetString(best, FC_FILE, 0, &filename)) {
        log_->e("Fontconfig: Retrieve font filename failed for ", font_name);
        return ret;  // std::nullopt
    }

    int fc_index = 0;
    if (FcResultMatch != FcPatternGetInteger(best, FC_INDEX, 0, &fc_index)) {
        log_->e("Fontconfig: Retrieve font FC_INDEX failed for ", font_name);
        return ret;  // std::nullopt
    }

    if (ucs4.has_value()) {
        FcCharSet* charset = nullptr;

        if (FcResultMatch != FcPatternGetCharSet(best, FC_CHARSET, 0, &charset)) {
            log_->e("Fontconfig: Retrieve font charset failed for ", font_name);
            return ret;  // std::nullopt;
        }

        if (FcTrue != FcCharSetHasChar(charset, ucs4.value())) {
            log_->w("Fontconfig: Font ", font_name, " doesn't contain Unicode codepoint ", std::hex, ucs4.value());
            return ret;  // std::nullopt;
        }
    }

    ret.emplace();
    ret->filename = reinterpret_cast<const char*>(filename);
    ret->face_index = fc_index;
    ret->provider_type = FontProviderType::kFontProviderFontconfig;

    return ret;
}

}  // namespace aribcaption
