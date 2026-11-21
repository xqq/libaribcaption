/*
 * Copyright (C) 2026 magicxqq <xqq@xqq.im>. All rights reserved.
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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "aribcaption/aribcaption.hpp"
#include "aribcc_config.h"
#include "base/language_code.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/font_provider.hpp"
#include <TargetConditionals.h>
#if defined(ARIBCC_USE_CORETEXT)
    #include <CoreFoundation/CoreFoundation.h>
    #if TARGET_OS_IPHONE
        #include <CoreGraphics/CoreGraphics.h>
    #endif
    #include "base/scoped_cfref.hpp"
    #include "renderer/font_provider_coretext.hpp"
#endif
#include "renderer/region_renderer.hpp"
#include "renderer/text_renderer.hpp"

using namespace aribcaption;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            g_failures++;                                                                \
        }                                                                                \
    } while (0)

constexpr ColorRGBA kWhite(255, 255, 255, 255);
constexpr ColorRGBA kBlack(0, 0, 0, 255);
constexpr const char* kNonExistentFont = "AribccNonExistentFontFamily_12345";

TextRenderStatus DrawSingleChar(TextRenderer& renderer,
                                uint32_t ucs4,
                                TextRenderFallbackPolicy policy,
                                Bitmap* out_bitmap = nullptr) {
    Bitmap local_bmp(64, 64, PixelFormat::kRGBA8888);
    Bitmap& bmp = out_bitmap ? *out_bitmap : local_bmp;
    TextRenderContext rctx = renderer.BeginDraw(bmp);
    TextRenderStatus status = renderer.DrawChar(
        rctx, 0, 0, ucs4, CharStyle::kCharStyleDefault, kWhite, kBlack, 0.0f, 36, 36, 1.0f, std::nullopt, policy);
    renderer.EndDraw(rctx);
    return status;
}

bool BitmapsEqual(const Bitmap& a, const Bitmap& b) {
    if (a.width() != b.width() || a.height() != b.height() || a.stride() != b.stride()) {
        return false;
    }
    return std::memcmp(a.data(), b.data(), static_cast<size_t>(a.stride()) * static_cast<size_t>(a.height())) == 0;
}

bool BitmapHasNonZeroPixels(const Bitmap& bmp) {
    const uint8_t* data = bmp.data();
    size_t size = static_cast<size_t>(bmp.stride()) * static_cast<size_t>(bmp.height());
    for (size_t i = 0; i < size; i++) {
        if (data[i] != 0) {
            return true;
        }
    }
    return false;
}

#if defined(ARIBCC_USE_FREETYPE) || defined(ARIBCC_USE_CORETEXT)
class ProxyFontProvider : public FontProvider {
public:
    explicit ProxyFontProvider(FontProvider& backing) : backing_(backing) {}

    FontProviderType GetType() override {
        return backing_.GetType();
    }

    bool Initialize() override {
        return true;
    }

    void SetLanguage(uint32_t iso6392_language_code) override {
        lang_ = iso6392_language_code;
        backing_.SetLanguage(iso6392_language_code);
    }

    void MapFont(const std::string& virtual_name, const std::string& real_name, uint32_t real_lang) {
        font_map_[virtual_name] = {real_name, real_lang};
    }

    void SetAllowSystemFallback(bool allow) {
        allow_system_fallback_ = allow;
    }

    void SetForcedSystemFallback(const std::string& real_name, uint32_t real_lang) {
        forced_fallback_ = {real_name, real_lang};
    }

    void ResetCounts() {
        get_font_calls_.clear();
        get_fallback_calls_ = 0;
    }

    int GetFontCallCount(const std::string& name) const {
        auto it = get_font_calls_.find(name);
        return it == get_font_calls_.end() ? 0 : it->second;
    }

    int GetFallbackCallCount() const {
        return get_fallback_calls_;
    }

    const std::string& GetLastFallbackBaseFamily() const {
        return last_fallback_base_family_;
    }

    Result<FontfaceInfo, FontProviderError> GetFontFace(const std::string& font_name) override {
        get_font_calls_[font_name]++;
        auto it = font_map_.find(font_name);
        if (it == font_map_.end()) {
            return Err(FontProviderError::kFontNotFound);
        }
        backing_.SetLanguage(it->second.lang);
        auto res = backing_.GetFontFace(it->second.real_name);
        backing_.SetLanguage(lang_);
        return res;
    }

    Result<FontfaceInfo, FontProviderError> GetFallbackFontFace(uint32_t ucs4,
                                                                std::string_view base_family_name) override {
        get_fallback_calls_++;
        last_fallback_base_family_ = std::string(base_family_name);
        if (!allow_system_fallback_) {
            return Err(FontProviderError::kCodePointNotFound);
        }
        if (forced_fallback_) {
            backing_.SetLanguage(forced_fallback_->lang);
            auto res = backing_.GetFontFace(forced_fallback_->real_name);
            backing_.SetLanguage(lang_);
            return res;
        }
        return backing_.GetFallbackFontFace(ucs4, base_family_name);
    }
private:
    struct MappedFont {
        std::string real_name;
        uint32_t lang = 0;
    };

    FontProvider& backing_;
    uint32_t lang_ = 0;
    std::unordered_map<std::string, MappedFont> font_map_;
    std::unordered_map<std::string, int> get_font_calls_;
    bool allow_system_fallback_ = false;
    std::optional<MappedFont> forced_fallback_;
    int get_fallback_calls_ = 0;
    std::string last_fallback_base_family_;
};

void RunMockProviderTests(Context& ctx, FontProvider& backing, TextRendererType renderer_type, const char* label) {
    printf("Running mock-provider tests (%s)...\n", label);

    ProxyFontProvider proxy(backing);
    proxy.MapFont("MockLatin", "sans-serif", ThreeCC("eng"));
    proxy.MapFont("MockCJK", "sans-serif", ThreeCC("jpn"));
    proxy.MapFont("MockCJKAlias", "sans-serif", ThreeCC("jpn"));
    proxy.MapFont("MockSerif", "serif", ThreeCC("jpn"));

    // Verify MockLatin actually lacks U+3042 on this host; skip Latin-vs-CJK sub-checks if it has it
    bool latin_lacks_hiragana = false;
    {
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"MockLatin"});
        latin_lacks_hiragana =
            (DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kFailOnCodePointNotFound) ==
             TextRenderStatus::kCodePointNotFound);
    }

    // 1. Empty font_family_ returns kFontNotFound without UB
    {
        proxy.ResetCounts();
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kFontNotFound);
    }

    // 2. All-missing fonts returns kFontNotFound and caches unavailable slots
    {
        proxy.ResetCounts();
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"Missing1", "Missing2"});
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kFontNotFound);
        CHECK(DrawSingleChar(*tr, 0x0042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kFontNotFound);
        CHECK(proxy.GetFontCallCount("Missing1") == 1);
        CHECK(proxy.GetFontCallCount("Missing2") == 1);
    }

    // 3. Error masking: trailing uninstalled font must return kCodePointNotFound, not kFontNotFound
    if (latin_lacks_hiragana) {
        proxy.ResetCounts();
        proxy.SetAllowSystemFallback(false);
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"MockLatin", "MissingTrailing"});
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
        // Repeated missing codepoint must hit negative cache without re-querying
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
        CHECK(proxy.GetFontCallCount("MockLatin") == 1);
        CHECK(proxy.GetFontCallCount("MissingTrailing") == 1);
        CHECK(proxy.GetFallbackCallCount() == 1);
    }

    // 4. kFailOnCodePointNotFound stops at main font without loading fallback slots
    if (latin_lacks_hiragana) {
        proxy.ResetCounts();
        proxy.SetAllowSystemFallback(false);
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"MockLatin", "MockCJK"});
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kFailOnCodePointNotFound) ==
              TextRenderStatus::kCodePointNotFound);
        CHECK(proxy.GetFontCallCount("MockCJK") == 0);
        CHECK(proxy.GetFallbackCallCount() == 0);
    }

    // 5. Caching, deduplication, and no thrashing across multiple fallback slots
    if (latin_lacks_hiragana) {
        proxy.ResetCounts();
        proxy.SetAllowSystemFallback(false);
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"MockLatin", "MockCJK", "MockCJKAlias", "MockSerif"});
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x3044, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x0042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x3046, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(proxy.GetFontCallCount("MockLatin") == 1);
        CHECK(proxy.GetFontCallCount("MockCJK") == 1);
        CHECK(proxy.GetFontCallCount("MockCJKAlias") == 0);

        // Unassigned codepoint forces scanning MockCJKAlias (deduplicated) and MockSerif, then caches miss
        CHECK(DrawSingleChar(*tr, 0x3FFFE, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
        CHECK(DrawSingleChar(*tr, 0x3FFFE, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
        CHECK(proxy.GetFontCallCount("MockCJKAlias") == 1);
        CHECK(proxy.GetFontCallCount("MockSerif") == 1);
    }

    // 6. Priority order (no priority inversion): loading a lower-priority fallback first must not
    // override a higher-priority fallback on subsequent characters
    if (latin_lacks_hiragana) {
        Bitmap ref_cjk(64, 64, PixelFormat::kRGBA8888);
        Bitmap ref_serif(64, 64, PixelFormat::kRGBA8888);
        bool serif_differs_from_cjk = false;
        {
            proxy.SetAllowSystemFallback(false);
            auto tr1 = TextRenderer::Create(renderer_type, ctx, proxy);
            auto tr2 = TextRenderer::Create(renderer_type, ctx, proxy);
            CHECK(tr1 && tr1->Initialize() && tr2 && tr2->Initialize());
            tr1->SetFontFamily({"MockCJK"});
            tr2->SetFontFamily({"MockSerif"});
            if (DrawSingleChar(*tr1, 0x3042, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &ref_cjk) ==
                    TextRenderStatus::kOK &&
                DrawSingleChar(*tr2, 0x3042, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &ref_serif) ==
                    TextRenderStatus::kOK) {
                serif_differs_from_cjk = !BitmapsEqual(ref_cjk, ref_serif);
            }
        }
        if (serif_differs_from_cjk) {
            proxy.ResetCounts();
            proxy.SetAllowSystemFallback(false);
            auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
            CHECK(tr && tr->Initialize());
            tr->SetFontFamily({"MockLatin", "MockCJK", "MockSerif"});
            // Force MockSerif to be loaded via a missing codepoint
            CHECK(DrawSingleChar(*tr, 0x3FFFE, TextRenderFallbackPolicy::kAutoFallback) ==
                  TextRenderStatus::kCodePointNotFound);
            // Now draw U+3042: must use higher-priority MockCJK, NOT lower-priority MockSerif
            Bitmap actual(64, 64, PixelFormat::kRGBA8888);
            CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback, &actual) ==
                  TextRenderStatus::kOK);
            CHECK(BitmapsEqual(actual, ref_cjk));
        }
    }

    // 7. System fallback caching and PUA bypass
    if (latin_lacks_hiragana) {
        proxy.ResetCounts();
        proxy.SetAllowSystemFallback(true);
        proxy.SetForcedSystemFallback("sans-serif", ThreeCC("jpn"));
        auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"MockLatin"});

        // PUA codepoint must NOT invoke GetFallbackFontFace
        CHECK(DrawSingleChar(*tr, 0xE000, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
        CHECK(proxy.GetFallbackCallCount() == 0);

        // First missing non-PUA character triggers GetFallbackFontFace once and caches it
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(proxy.GetFallbackCallCount() == 1);
        // base_family_name must be the requested font family entry, not the loaded face's family name
        CHECK(proxy.GetLastFallbackBaseFamily() == "MockLatin");

        // Second character covered by the cached system fallback must NOT call GetFallbackFontFace again
        CHECK(DrawSingleChar(*tr, 0x3044, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(proxy.GetFallbackCallCount() == 1);
    }

    // 8. kFontFamilyFallbackOnly searches the font family list but never system fallback,
    // so gaiji can try a listed font's PUA glyph before system fallback picks a font for the Unicode codepoint
    if (latin_lacks_hiragana) {
        proxy.ResetCounts();
        proxy.SetAllowSystemFallback(true);
        proxy.SetForcedSystemFallback("sans-serif", ThreeCC("jpn"));
        {
            auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
            CHECK(tr && tr->Initialize());
            tr->SetFontFamily({"MockLatin"});

            CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
                  TextRenderStatus::kCodePointNotFound);
            CHECK(proxy.GetFallbackCallCount() == 0);

            // The font-family-only miss must not be negatively cached, system fallback must still be reachable
            CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
            CHECK(proxy.GetFallbackCallCount() == 1);

            // PUA misses are final under either policy
            CHECK(DrawSingleChar(*tr, 0xE000, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
                  TextRenderStatus::kCodePointNotFound);
            CHECK(DrawSingleChar(*tr, 0xE000, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
                  TextRenderStatus::kCodePointNotFound);
            CHECK(proxy.GetFallbackCallCount() == 1);
        }

        proxy.ResetCounts();
        {
            auto tr = TextRenderer::Create(renderer_type, ctx, proxy);
            CHECK(tr && tr->Initialize());
            tr->SetFontFamily({"MockLatin", "MockCJK"});

            // Listed fallback fonts are still used
            CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
                  TextRenderStatus::kOK);
            CHECK(proxy.GetFontCallCount("MockCJK") == 1);
            CHECK(proxy.GetFallbackCallCount() == 0);
        }
    }
}
#endif

void RunRealPlatformTests(Context& ctx,
                          FontProviderType fp_type,
                          TextRendererType tr_type,
                          const char* label,
                          bool expect_os_fallback) {
    printf("Running real-platform tests (%s)...\n", label);

    auto fp = FontProvider::Create(fp_type, ctx);
    CHECK(fp && fp->Initialize());
    if (!fp) {
        return;
    }

    // 1. Concrete nonexistent family name must return kFontNotFound; generic "sans-serif" must succeed
    fp->SetLanguage(ThreeCC("eng"));
    auto missing = fp->GetFontFace(kNonExistentFont);
    CHECK(missing.is_err() && missing.error() == FontProviderError::kFontNotFound);
    auto generic_en = fp->GetFontFace("sans-serif");
    CHECK(generic_en.is_ok());

    fp->SetLanguage(ThreeCC("jpn"));
    auto generic_jp = fp->GetFontFace("sans-serif");
    CHECK(generic_jp.is_ok());

    // 2. Renderer advances past nonexistent font to select next valid main font
    {
        auto tr = TextRenderer::Create(tr_type, ctx, *fp);
        CHECK(tr && tr->Initialize());
        fp->SetLanguage(ThreeCC("jpn"));
        tr->SetLanguage(ThreeCC("jpn"));
        tr->SetFontFamily({kNonExistentFont, "sans-serif"});
        Bitmap bmp(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(bmp));
    }

    // 3. Single-font OS-level fallback for symbols/CJK/non-BMP and PUA bypass
    if (expect_os_fallback) {
        auto tr = TextRenderer::Create(tr_type, ctx, *fp);
        CHECK(tr && tr->Initialize());
        fp->SetLanguage(ThreeCC("eng"));
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({"sans-serif"});

        CHECK(DrawSingleChar(*tr, 0x266C, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x301C, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x3042, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0x20BB7, TextRenderFallbackPolicy::kAutoFallback) == TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0xE000, TextRenderFallbackPolicy::kAutoFallback) ==
              TextRenderStatus::kCodePointNotFound);
    }

    // 4. RegionRenderer gaiji-only region rendered via fallback must succeed (succeed++ regression)
    {
        RegionRenderer rr(ctx);
        CHECK(rr.Initialize(fp_type, tr_type));
        rr.SetFontLanguage(ThreeCC("eng"));
        rr.SetFontFamily({"sans-serif", kNonExistentFont});
        rr.SetOriginalPlaneSize(960, 540);
        rr.SetTargetCaptionAreaRect(aribcaption::Rect(0, 0, 960, 540));

        CaptionRegion region;
        region.x = 60;
        region.y = 60;
        region.width = 80;
        region.height = 80;

        CaptionChar ch;
        ch.type = CaptionCharType::kText;
        ch.codepoint = 0x3042;
        ch.pua_codepoint = 0xE000;
        ch.x = 60;
        ch.y = 60;
        ch.char_width = 36;
        ch.char_height = 36;
        ch.char_horizontal_spacing = 4;
        ch.char_vertical_spacing = 24;
        ch.char_horizontal_scale = 1.0f;
        ch.char_vertical_scale = 1.0f;
        ch.text_color = kWhite;
        ch.back_color = ColorRGBA(0, 0, 0, 128);
        region.chars.push_back(ch);

        std::unordered_map<uint32_t, DRCS> empty_drcs;
        auto rendered = rr.RenderCaptionRegion(region, empty_drcs);
        if (expect_os_fallback) {
            CHECK(rendered.is_ok());
        } else {
            // Even without OS fallback, trailing nonexistent font must not produce kFontNotFound
            CHECK(rendered.is_ok() || rendered.error() == RegionRenderError::kCodePointNotFound);
        }
    }
}

#if defined(ARIBCC_USE_FREETYPE) && defined(ARIBCC_USE_CORETEXT)
class StaticFontProvider : public FontProvider {
public:
    StaticFontProvider(std::string filename,
                       std::string family_name,
                       std::string postscript_name,
                       int face_index)
        : filename_(std::move(filename)),
          family_name_(std::move(family_name)),
          postscript_name_(std::move(postscript_name)),
          face_index_(face_index) {}

    FontProviderType GetType() override {
        return FontProviderType::kCoreText;
    }

    bool Initialize() override {
        return true;
    }

    void SetLanguage(uint32_t iso6392_language_code) override {
        (void)iso6392_language_code;
    }

    Result<FontfaceInfo, FontProviderError> GetFontFace(const std::string& font_name) override {
        (void)font_name;
        FontfaceInfo info;
        info.filename = filename_;
        info.family_name = family_name_;
        info.postscript_name = postscript_name_;
        info.face_index = face_index_;
        info.provider_type = FontProviderType::kCoreText;
        return Ok(std::move(info));
    }
private:
    std::string filename_;
    std::string family_name_;
    std::string postscript_name_;
    int face_index_ = 0;
};

void RunFreeTypeCollectionScanTests(Context& ctx, FontProvider& coretext_fp) {
    printf("Running FreeType collection scan tests (FreeType+CoreText)...\n");

    coretext_fp.SetLanguage(ThreeCC("eng"));
    auto regular_res = coretext_fp.GetFontFace("Avenir Next Condensed");
    auto bold_res = coretext_fp.GetFontFace("AvenirNextCondensed-Bold");
    if (regular_res.is_err() || bold_res.is_err()) {
        return;
    }

    const FontfaceInfo& regular = regular_res.value();
    const FontfaceInfo& bold = bold_res.value();
    if (regular.filename.empty() || regular.filename != bold.filename || regular.face_index >= 0 ||
        regular.family_name != bold.family_name || regular.postscript_name == bold.postscript_name) {
        return;
    }

    // 1. Exact PostScript match wins over earlier family-name match in a collection
    Bitmap regular_bmp(64, 64, PixelFormat::kRGBA8888);
    Bitmap bold_bmp(64, 64, PixelFormat::kRGBA8888);
    Bitmap ibreve_bmp(64, 64, PixelFormat::kRGBA8888);
    {
        auto tr_regular = TextRenderer::Create(TextRendererType::kFreetype, ctx, coretext_fp);
        auto tr_bold = TextRenderer::Create(TextRendererType::kFreetype, ctx, coretext_fp);
        CHECK(tr_regular && tr_regular->Initialize() && tr_bold && tr_bold->Initialize());

        tr_regular->SetFontFamily({"Avenir Next Condensed"});
        tr_bold->SetFontFamily({"AvenirNextCondensed-Bold"});

        CHECK(DrawSingleChar(*tr_regular, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &regular_bmp) ==
              TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr_bold, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &bold_bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(regular_bmp));
        CHECK(BitmapHasNonZeroPixels(bold_bmp));
        CHECK(!BitmapsEqual(regular_bmp, bold_bmp));

        // U+012C exists in Regular (face 7) but not in Bold (face 0)
        CHECK(DrawSingleChar(*tr_regular, 0x012C, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &ibreve_bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(ibreve_bmp));

        CHECK(DrawSingleChar(*tr_bold, 0x012C, TextRenderFallbackPolicy::kFailOnCodePointNotFound) ==
              TextRenderStatus::kCodePointNotFound);
    }

    // 2. Two subfaces from the same collection file load as distinct slots without false deduplication
    {
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, coretext_fp);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"AvenirNextCondensed-Bold", "Avenir Next Condensed"});
        Bitmap actual(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x012C, TextRenderFallbackPolicy::kFontFamilyFallbackOnly, &actual) ==
              TextRenderStatus::kOK);
        CHECK(BitmapsEqual(actual, ibreve_bmp));
    }

    // 3. Remaining scan branches via StaticFontProvider with face_index = -1
    {
        // Empty postscript_name + valid family_name: opens first family match (face 0, Bold)
        StaticFontProvider fp(regular.filename, regular.family_name, "", -1);
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, fp);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"StaticTest"});
        Bitmap actual(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &actual) ==
              TextRenderStatus::kOK);
        CHECK(BitmapsEqual(actual, bold_bmp));
    }

    {
        // Unmatched postscript_name + valid family_name: falls back to first family match after scanning all faces
        StaticFontProvider fp(regular.filename, regular.family_name, "AribccNonExistentPostScriptName", -1);
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, fp);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"StaticTest"});
        Bitmap actual(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &actual) ==
              TextRenderStatus::kOK);
        CHECK(BitmapsEqual(actual, bold_bmp));
    }

    {
        // Valid postscript_name + empty family_name: opens exact PostScript face (face 7, Regular)
        StaticFontProvider fp(regular.filename, "", regular.postscript_name, -1);
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, fp);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"StaticTest"});
        Bitmap actual(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x012C, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &actual) ==
              TextRenderStatus::kOK);
        CHECK(BitmapsEqual(actual, ibreve_bmp));
    }

    {
        // Neither postscript_name nor family_name matches: returns kFontNotFound
        StaticFontProvider fp(regular.filename, kNonExistentFont, "AribccNonExistentPostScriptName", -1);
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, fp);
        CHECK(tr && tr->Initialize());
        tr->SetFontFamily({"StaticTest"});
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound) ==
              TextRenderStatus::kFontNotFound);
    }
}
#endif

#if defined(ARIBCC_USE_CORETEXT)
void AppendBE16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void AppendBE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void AppendBE64(std::vector<uint8_t>& out, uint64_t v) {
    AppendBE32(out, static_cast<uint32_t>(v >> 32));
    AppendBE32(out, static_cast<uint32_t>(v & 0xFFFFFFFFu));
}

void WriteBE32At(std::vector<uint8_t>& buf, size_t offset, uint32_t v) {
    buf[offset + 0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>(v & 0xFF);
}

uint32_t CalcSFNTChecksum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    size_t n = data.size();
    for (size_t i = 0; i < n; i += 4) {
        uint32_t word = 0;
        for (size_t j = 0; j < 4; j++) {
            uint8_t b = (i + j < n) ? data[i + j] : 0;
            word = (word << 8) | b;
        }
        sum += word;
    }
    return sum;
}

std::vector<uint8_t> BuildMinimalTestTTF(std::string_view family_name, std::string_view postscript_name) {
    struct SFNTTable {
        uint32_t tag;
        std::vector<uint8_t> data;
    };

    auto make_tag = [](const char (&t)[5]) -> uint32_t {
        return (static_cast<uint32_t>(static_cast<uint8_t>(t[0])) << 24) |
               (static_cast<uint32_t>(static_cast<uint8_t>(t[1])) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(t[2])) << 8) |
               static_cast<uint32_t>(static_cast<uint8_t>(t[3]));
    };

    // OS/2 (version 4, 96 bytes)
    std::vector<uint8_t> os2_tbl;
    AppendBE16(os2_tbl, 4);      // version
    AppendBE16(os2_tbl, 700);    // xAvgCharWidth
    AppendBE16(os2_tbl, 400);    // usWeightClass (Regular)
    AppendBE16(os2_tbl, 5);      // usWidthClass (Medium)
    AppendBE16(os2_tbl, 0);      // fsType
    AppendBE16(os2_tbl, 650);    // ySubscriptXSize
    AppendBE16(os2_tbl, 600);    // ySubscriptYSize
    AppendBE16(os2_tbl, 0);      // ySubscriptXOffset
    AppendBE16(os2_tbl, 75);     // ySubscriptYOffset
    AppendBE16(os2_tbl, 650);    // ySuperscriptXSize
    AppendBE16(os2_tbl, 600);    // ySuperscriptYSize
    AppendBE16(os2_tbl, 0);      // ySuperscriptXOffset
    AppendBE16(os2_tbl, 350);    // ySuperscriptYOffset
    AppendBE16(os2_tbl, 50);     // yStrikeoutSize
    AppendBE16(os2_tbl, 300);    // yStrikeoutPosition
    AppendBE16(os2_tbl, 0);      // sFamilyClass
    os2_tbl.resize(os2_tbl.size() + 10, 0);  // panose[10]
    AppendBE32(os2_tbl, 1);      // ulUnicodeRange1 (Basic Latin)
    AppendBE32(os2_tbl, 0);      // ulUnicodeRange2
    AppendBE32(os2_tbl, 0);      // ulUnicodeRange3
    AppendBE32(os2_tbl, 0);      // ulUnicodeRange4
    os2_tbl.push_back('T');
    os2_tbl.push_back('E');
    os2_tbl.push_back('S');
    os2_tbl.push_back('T');      // achVendID
    AppendBE16(os2_tbl, 0x0040); // fsSelection (REGULAR)
    AppendBE16(os2_tbl, 0x0041); // usFirstCharIndex
    AppendBE16(os2_tbl, 0xE123); // usLastCharIndex
    AppendBE16(os2_tbl, 800);    // sTypoAscender
    AppendBE16(os2_tbl, static_cast<uint16_t>(static_cast<int16_t>(-200)));  // sTypoDescender
    AppendBE16(os2_tbl, 0);      // sTypoLineGap
    AppendBE16(os2_tbl, 800);    // usWinAscent
    AppendBE16(os2_tbl, 200);    // usWinDescent
    AppendBE32(os2_tbl, 1);      // ulCodePageRange1
    AppendBE32(os2_tbl, 0);      // ulCodePageRange2
    AppendBE16(os2_tbl, 500);    // sxHeight
    AppendBE16(os2_tbl, 700);    // sCapHeight
    AppendBE16(os2_tbl, 0);      // usDefaultChar
    AppendBE16(os2_tbl, 0x0020); // usBreakChar
    AppendBE16(os2_tbl, 0);      // usMaxContext

    // cmap: Format 4 with U+0041 -> glyph 1, U+E123 -> glyph 2, sentinel 0xFFFF -> glyph 0
    std::vector<uint8_t> cmap_tbl;
    AppendBE16(cmap_tbl, 0);   // version
    AppendBE16(cmap_tbl, 2);   // numTables
    AppendBE16(cmap_tbl, 0);   // platformID 0 (Unicode)
    AppendBE16(cmap_tbl, 3);   // encodingID 3 (Unicode BMP)
    AppendBE32(cmap_tbl, 20);  // subtable offset
    AppendBE16(cmap_tbl, 3);   // platformID 3 (Windows)
    AppendBE16(cmap_tbl, 1);   // encodingID 1 (Unicode BMP)
    AppendBE32(cmap_tbl, 20);  // subtable offset
    // Format 4 subtable (40 bytes)
    AppendBE16(cmap_tbl, 4);   // format
    AppendBE16(cmap_tbl, 40);  // length
    AppendBE16(cmap_tbl, 0);   // language
    AppendBE16(cmap_tbl, 6);   // segCountX2 (3 * 2)
    AppendBE16(cmap_tbl, 4);   // searchRange
    AppendBE16(cmap_tbl, 1);   // entrySelector
    AppendBE16(cmap_tbl, 2);   // rangeShift
    AppendBE16(cmap_tbl, 0x0041);
    AppendBE16(cmap_tbl, 0xE123);
    AppendBE16(cmap_tbl, 0xFFFF);
    AppendBE16(cmap_tbl, 0);   // reservedPad
    AppendBE16(cmap_tbl, 0x0041);
    AppendBE16(cmap_tbl, 0xE123);
    AppendBE16(cmap_tbl, 0xFFFF);
    AppendBE16(cmap_tbl, static_cast<uint16_t>(1 - 0x0041));
    AppendBE16(cmap_tbl, static_cast<uint16_t>(2 - 0xE123));
    AppendBE16(cmap_tbl, 1);
    AppendBE16(cmap_tbl, 0);
    AppendBE16(cmap_tbl, 0);
    AppendBE16(cmap_tbl, 0);

    // glyf: glyph 0 empty, glyph 1 and glyph 2 are clockwise square outlines (36 bytes each)
    auto append_square_glyph = [](std::vector<uint8_t>& out) {
        AppendBE16(out, 1);    // numberOfContours
        AppendBE16(out, 100);  // xMin
        AppendBE16(out, 100);  // yMin
        AppendBE16(out, 700);  // xMax
        AppendBE16(out, 700);  // yMax
        AppendBE16(out, 3);    // endPtsOfContours[0]
        AppendBE16(out, 0);    // instructionLength
        // flags[4]: on-curve, 16-bit x/y deltas
        out.push_back(0x01);
        out.push_back(0x01);
        out.push_back(0x01);
        out.push_back(0x01);
        // x deltas: 100, 0, 600, 0
        AppendBE16(out, 100);
        AppendBE16(out, 0);
        AppendBE16(out, 600);
        AppendBE16(out, 0);
        // y deltas: 100, 600, 0, -600
        AppendBE16(out, 100);
        AppendBE16(out, 600);
        AppendBE16(out, 0);
        AppendBE16(out, static_cast<uint16_t>(static_cast<int16_t>(-600)));
        // 4-byte pad (34 -> 36 bytes)
        out.push_back(0);
        out.push_back(0);
    };
    std::vector<uint8_t> glyf_tbl;
    append_square_glyph(glyf_tbl);
    append_square_glyph(glyf_tbl);

    // head (54 bytes)
    std::vector<uint8_t> head_tbl;
    AppendBE16(head_tbl, 1);           // majorVersion
    AppendBE16(head_tbl, 0);           // minorVersion
    AppendBE32(head_tbl, 0x00010000);  // fontRevision
    AppendBE32(head_tbl, 0);           // checkSumAdjustment (patched at end)
    AppendBE32(head_tbl, 0x5F0F3CF5);  // magicNumber
    AppendBE16(head_tbl, 0x000B);      // flags
    AppendBE16(head_tbl, 1000);        // unitsPerEm
    AppendBE64(head_tbl, 0);           // created
    AppendBE64(head_tbl, 0);           // modified
    AppendBE16(head_tbl, 100);         // xMin
    AppendBE16(head_tbl, 100);         // yMin
    AppendBE16(head_tbl, 700);         // xMax
    AppendBE16(head_tbl, 700);         // yMax
    AppendBE16(head_tbl, 0);           // macStyle
    AppendBE16(head_tbl, 8);           // lowestRecPPEM
    AppendBE16(head_tbl, 2);           // fontDirectionHint
    AppendBE16(head_tbl, 1);           // indexToLocFormat (long)
    AppendBE16(head_tbl, 0);           // glyphDataFormat

    // hhea (36 bytes)
    std::vector<uint8_t> hhea_tbl;
    AppendBE16(hhea_tbl, 1);     // majorVersion
    AppendBE16(hhea_tbl, 0);     // minorVersion
    AppendBE16(hhea_tbl, 800);   // ascender
    AppendBE16(hhea_tbl, static_cast<uint16_t>(static_cast<int16_t>(-200)));  // descender
    AppendBE16(hhea_tbl, 0);     // lineGap
    AppendBE16(hhea_tbl, 800);   // advanceWidthMax
    AppendBE16(hhea_tbl, 0);     // minLeftSideBearing
    AppendBE16(hhea_tbl, 100);   // minRightSideBearing
    AppendBE16(hhea_tbl, 700);   // xMaxExtent
    AppendBE16(hhea_tbl, 1);     // caretSlopeRise
    AppendBE16(hhea_tbl, 0);     // caretSlopeRun
    AppendBE16(hhea_tbl, 0);     // caretOffset
    AppendBE16(hhea_tbl, 0);     // reserved1
    AppendBE16(hhea_tbl, 0);     // reserved2
    AppendBE16(hhea_tbl, 0);     // reserved3
    AppendBE16(hhea_tbl, 0);     // reserved4
    AppendBE16(hhea_tbl, 0);     // metricDataFormat
    AppendBE16(hhea_tbl, 3);     // numberOfHMetrics

    // hmtx (3 entries = 12 bytes)
    std::vector<uint8_t> hmtx_tbl;
    AppendBE16(hmtx_tbl, 500);
    AppendBE16(hmtx_tbl, 0);
    AppendBE16(hmtx_tbl, 800);
    AppendBE16(hmtx_tbl, 100);
    AppendBE16(hmtx_tbl, 800);
    AppendBE16(hmtx_tbl, 100);

    // loca (long 32-bit offsets for 3 glyphs + sentinel = 16 bytes)
    std::vector<uint8_t> loca_tbl;
    AppendBE32(loca_tbl, 0);
    AppendBE32(loca_tbl, 0);
    AppendBE32(loca_tbl, 36);
    AppendBE32(loca_tbl, 72);

    // maxp (version 1.0, 32 bytes)
    std::vector<uint8_t> maxp_tbl;
    AppendBE32(maxp_tbl, 0x00010000);  // version 1.0
    AppendBE16(maxp_tbl, 3);           // numGlyphs
    AppendBE16(maxp_tbl, 4);           // maxPoints
    AppendBE16(maxp_tbl, 1);           // maxContours
    AppendBE16(maxp_tbl, 0);           // maxCompositePoints
    AppendBE16(maxp_tbl, 0);           // maxCompositeContours
    AppendBE16(maxp_tbl, 2);           // maxZones
    AppendBE16(maxp_tbl, 0);           // maxTwilightPoints
    AppendBE16(maxp_tbl, 0);           // maxStorage
    AppendBE16(maxp_tbl, 0);           // maxFunctionDefs
    AppendBE16(maxp_tbl, 0);           // maxInstructionDefs
    AppendBE16(maxp_tbl, 0);           // maxStackElements
    AppendBE16(maxp_tbl, 0);           // maxSizeOfInstructions
    AppendBE16(maxp_tbl, 0);           // maxComponentElements
    AppendBE16(maxp_tbl, 0);           // maxComponentDepth

    // name (Format 0 with Mac Roman ASCII + Windows Unicode BMP UTF-16BE records sorted by platform/encoding/lang/nameID)
    std::string full_name = std::string(family_name) + " Regular";
    struct NameEntry {
        uint16_t name_id;
        std::string_view value;
    };
    const NameEntry name_entries[] = {
        {1, family_name},
        {2, "Regular"},
        {3, postscript_name},
        {4, full_name},
        {5, "Version 1.0"},
        {6, postscript_name},
    };

    constexpr uint16_t kNameRecordCount = 12;
    std::vector<uint8_t> name_tbl;
    AppendBE16(name_tbl, 0);  // format
    AppendBE16(name_tbl, kNameRecordCount);
    AppendBE16(name_tbl, static_cast<uint16_t>(6 + kNameRecordCount * 12));  // stringOffset

    std::vector<uint8_t> string_storage;
    // Platform 1 (Macintosh Roman, lang 0): ASCII
    for (const auto& entry : name_entries) {
        uint16_t offset = static_cast<uint16_t>(string_storage.size());
        uint16_t len = static_cast<uint16_t>(entry.value.size());
        string_storage.insert(string_storage.end(), entry.value.begin(), entry.value.end());
        AppendBE16(name_tbl, 1);
        AppendBE16(name_tbl, 0);
        AppendBE16(name_tbl, 0);
        AppendBE16(name_tbl, entry.name_id);
        AppendBE16(name_tbl, len);
        AppendBE16(name_tbl, offset);
    }
    // Platform 3 (Windows Unicode BMP, lang 0x0409): UTF-16BE
    for (const auto& entry : name_entries) {
        uint16_t offset = static_cast<uint16_t>(string_storage.size());
        uint16_t len = static_cast<uint16_t>(entry.value.size() * 2);
        for (unsigned char ch : entry.value) {
            string_storage.push_back(0);
            string_storage.push_back(ch);
        }
        AppendBE16(name_tbl, 3);
        AppendBE16(name_tbl, 1);
        AppendBE16(name_tbl, 0x0409);
        AppendBE16(name_tbl, entry.name_id);
        AppendBE16(name_tbl, len);
        AppendBE16(name_tbl, offset);
    }
    name_tbl.insert(name_tbl.end(), string_storage.begin(), string_storage.end());

    // post (version 3.0, 32 bytes)
    std::vector<uint8_t> post_tbl;
    AppendBE32(post_tbl, 0x00030000);  // version 3.0
    AppendBE32(post_tbl, 0);           // italicAngle
    AppendBE16(post_tbl, static_cast<uint16_t>(static_cast<int16_t>(-100)));  // underlinePosition
    AppendBE16(post_tbl, 50);          // underlineThickness
    AppendBE32(post_tbl, 0);           // isFixedPitch
    AppendBE32(post_tbl, 0);
    AppendBE32(post_tbl, 0);
    AppendBE32(post_tbl, 0);
    AppendBE32(post_tbl, 0);

    // Tables sorted strictly in ascending ASCII tag order: OS/2, cmap, glyf, head, hhea, hmtx, loca, maxp, name, post
    std::vector<SFNTTable> tables = {
        {make_tag("OS/2"), std::move(os2_tbl)},
        {make_tag("cmap"), std::move(cmap_tbl)},
        {make_tag("glyf"), std::move(glyf_tbl)},
        {make_tag("head"), std::move(head_tbl)},
        {make_tag("hhea"), std::move(hhea_tbl)},
        {make_tag("hmtx"), std::move(hmtx_tbl)},
        {make_tag("loca"), std::move(loca_tbl)},
        {make_tag("maxp"), std::move(maxp_tbl)},
        {make_tag("name"), std::move(name_tbl)},
        {make_tag("post"), std::move(post_tbl)},
    };

    const uint16_t num_tables = static_cast<uint16_t>(tables.size());
    std::vector<uint8_t> font;
    // Offset table (12 bytes)
    AppendBE32(font, 0x00010000);
    AppendBE16(font, num_tables);
    AppendBE16(font, 128);  // searchRange: 8 * 16
    AppendBE16(font, 3);    // entrySelector: log2(8)
    AppendBE16(font, static_cast<uint16_t>(num_tables * 16 - 128));  // rangeShift

    // Reserve table directory (16 bytes per table)
    const size_t dir_offset = font.size();
    font.resize(dir_offset + num_tables * 16, 0);

    size_t head_offset = 0;
    for (size_t i = 0; i < tables.size(); i++) {
        size_t tbl_offset = font.size();
        if (tables[i].tag == make_tag("head")) {
            head_offset = tbl_offset;
        }
        uint32_t length = static_cast<uint32_t>(tables[i].data.size());
        uint32_t checksum = CalcSFNTChecksum(tables[i].data);

        size_t entry = dir_offset + i * 16;
        WriteBE32At(font, entry + 0, tables[i].tag);
        WriteBE32At(font, entry + 4, checksum);
        WriteBE32At(font, entry + 8, static_cast<uint32_t>(tbl_offset));
        WriteBE32At(font, entry + 12, length);

        font.insert(font.end(), tables[i].data.begin(), tables[i].data.end());
        while (font.size() % 4 != 0) {
            font.push_back(0);
        }
    }

    uint32_t whole_checksum = CalcSFNTChecksum(font);
    WriteBE32At(font, head_offset + 8, 0xB1B0AFBAu - whole_checksum);
    return font;
}

class ScopedCoreTextMemoryFont {
public:
    explicit ScopedCoreTextMemoryFont(const std::vector<uint8_t>& font_bytes) {
        ScopedCFRef<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,
                                                 font_bytes.data(),
                                                 static_cast<CFIndex>(font_bytes.size())));
        if (!data) {
            return;
        }
        ScopedCFRef<CGDataProviderRef> provider(CGDataProviderCreateWithCFData(data.get()));
        if (!provider) {
            return;
        }
        cg_font_.reset(CGFontCreateWithDataProvider(provider.get()));
        if (!cg_font_) {
            return;
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        registered_ = CTFontManagerRegisterGraphicsFont(cg_font_.get(), nullptr);
#pragma clang diagnostic pop
    }

    ~ScopedCoreTextMemoryFont() {
        if (registered_ && cg_font_) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CTFontManagerUnregisterGraphicsFont(cg_font_.get(), nullptr);
#pragma clang diagnostic pop
        }
    }

    ScopedCoreTextMemoryFont(const ScopedCoreTextMemoryFont&) = delete;
    ScopedCoreTextMemoryFont& operator=(const ScopedCoreTextMemoryFont&) = delete;

    bool Registered() const {
        return registered_;
    }
private:
    ScopedCFRef<CGFontRef> cg_font_;
    bool registered_ = false;
};

void RunCoreTextMemoryFontTests(Context& ctx) {
    printf("Running CoreText in-memory font tests...\n");

    static constexpr const char* kMemFamily = "AribccMemTestFont";
    static constexpr const char* kMemPostScript = "AribccMemTestFont-Regular";
    static constexpr uint32_t kMemPUACodePoint = 0xE123;

    std::vector<uint8_t> ttf_bytes = BuildMinimalTestTTF(kMemFamily, kMemPostScript);
    ScopedCoreTextMemoryFont mem_font(ttf_bytes);
    CHECK(mem_font.Registered());
    if (!mem_font.Registered()) {
        return;
    }

    auto fp = FontProvider::Create(FontProviderType::kCoreText, ctx);
    CHECK(fp && fp->Initialize());
    if (!fp) {
        return;
    }
    fp->SetLanguage(ThreeCC("eng"));

    // 1. FontProviderCoreText resolves memory-registered font by family and PostScript names with empty filename
    {
        auto by_family = fp->GetFontFace(kMemFamily);
        CHECK(by_family.is_ok());
        if (by_family.is_ok()) {
            const FontfaceInfo& info = by_family.value();
            CHECK(info.filename.empty());
            CHECK(info.family_name == kMemFamily);
            CHECK(info.postscript_name == kMemPostScript);
            auto priv = static_cast<FontfaceInfoPrivateCoreText*>(info.provider_priv.get());
            CHECK(priv && priv->ct_font);
        }

        auto by_ps = fp->GetFontFace(kMemPostScript);
        CHECK(by_ps.is_ok());
        if (by_ps.is_ok()) {
            const FontfaceInfo& info = by_ps.value();
            CHECK(info.filename.empty());
            CHECK(info.family_name == kMemFamily);
            CHECK(info.postscript_name == kMemPostScript);
            auto priv = static_cast<FontfaceInfoPrivateCoreText*>(info.provider_priv.get());
            CHECK(priv && priv->ct_font);
        }
    }

    // 2. TextRendererCoreText uses memory-registered font as primary font
    {
        auto tr = TextRenderer::Create(TextRendererType::kCoreText, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({kMemFamily});
        Bitmap bmp(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(bmp));
    }

    // 3. TextRendererCoreText uses memory-registered font as configured fallback by family name
    {
        auto tr = TextRenderer::Create(TextRendererType::kCoreText, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({"sans-serif", kMemFamily});
        Bitmap bmp(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, kMemPUACodePoint, TextRenderFallbackPolicy::kFontFamilyFallbackOnly, &bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(bmp));
    }

    // 4. TextRendererCoreText uses memory-registered font as configured fallback by PostScript name
    {
        auto tr = TextRenderer::Create(TextRendererType::kCoreText, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({"sans-serif", kMemPostScript});
        Bitmap bmp(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, kMemPUACodePoint, TextRenderFallbackPolicy::kFontFamilyFallbackOnly, &bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(bmp));
    }

    // 5. TextRendererCoreText deduplicates family and PostScript aliases cleanly
    {
        auto tr = TextRenderer::Create(TextRendererType::kCoreText, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({kMemFamily, kMemPostScript});
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
              TextRenderStatus::kOK);
        CHECK(DrawSingleChar(*tr, 0xE124, TextRenderFallbackPolicy::kFontFamilyFallbackOnly) ==
              TextRenderStatus::kCodePointNotFound);
    }

#if defined(ARIBCC_USE_FREETYPE)
    // 6. FreeType+CoreText skips URL-less memory font without failing fatally when another font exists
    {
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({kMemFamily});
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound) ==
              TextRenderStatus::kFontNotFound);
    }
    {
        auto tr = TextRenderer::Create(TextRendererType::kFreetype, ctx, *fp);
        CHECK(tr && tr->Initialize());
        tr->SetLanguage(ThreeCC("eng"));
        tr->SetFontFamily({kMemFamily, "sans-serif"});
        Bitmap bmp(64, 64, PixelFormat::kRGBA8888);
        CHECK(DrawSingleChar(*tr, 0x0041, TextRenderFallbackPolicy::kFailOnCodePointNotFound, &bmp) ==
              TextRenderStatus::kOK);
        CHECK(BitmapHasNonZeroPixels(bmp));
    }
#endif
}
#endif

}  // namespace

int main() {
    Context ctx;
    ctx.SetLogcatCallback([](LogLevel level, const char* msg) {
        if (level == LogLevel::kError) {
            fprintf(stderr, "[ERROR] %s\n", msg);
        }
    });

#if defined(ARIBCC_USE_CORETEXT)
    {
        auto fp = FontProvider::Create(FontProviderType::kCoreText, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kCoreText, "CoreText");
        }
        RunRealPlatformTests(ctx, FontProviderType::kCoreText, TextRendererType::kCoreText, "CoreText", true);
        RunCoreTextMemoryFontTests(ctx);
    }
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    RunRealPlatformTests(ctx, FontProviderType::kDirectWrite, TextRendererType::kDirectWrite, "DirectWrite", true);
#endif

#if defined(ARIBCC_USE_FREETYPE)
    #if defined(ARIBCC_USE_CORETEXT)
    {
        auto fp = FontProvider::Create(FontProviderType::kCoreText, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kFreetype, "FreeType+CoreText");
            RunFreeTypeCollectionScanTests(ctx, *fp);
        }
        RunRealPlatformTests(ctx, FontProviderType::kCoreText, TextRendererType::kFreetype, "FreeType+CoreText", true);
    }
    #endif

    #if defined(ARIBCC_USE_FONTCONFIG)
    {
        auto fp = FontProvider::Create(FontProviderType::kFontconfig, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kFreetype, "FreeType+Fontconfig");
        }
        RunRealPlatformTests(
            ctx, FontProviderType::kFontconfig, TextRendererType::kFreetype, "FreeType+Fontconfig", true);
    }
    #endif

    #if defined(ARIBCC_USE_DIRECTWRITE)
    {
        auto fp = FontProvider::Create(FontProviderType::kDirectWrite, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kFreetype, "FreeType+DirectWrite");
        }
        RunRealPlatformTests(
            ctx, FontProviderType::kDirectWrite, TextRendererType::kFreetype, "FreeType+DirectWrite", true);
    }
    #endif

    #if defined(ARIBCC_USE_GDI_FONT)
    {
        auto fp = FontProvider::Create(FontProviderType::kGDI, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kFreetype, "FreeType+GDI");
        }
        RunRealPlatformTests(ctx, FontProviderType::kGDI, TextRendererType::kFreetype, "FreeType+GDI", false);
    }
    #endif

    #if defined(ARIBCC_IS_ANDROID)
    {
        auto fp = FontProvider::Create(FontProviderType::kAndroid, ctx);
        CHECK(fp && fp->Initialize());
        if (fp) {
            RunMockProviderTests(ctx, *fp, TextRendererType::kFreetype, "FreeType+Android");
        }
        RunRealPlatformTests(ctx, FontProviderType::kAndroid, TextRendererType::kFreetype, "FreeType+Android", true);
    }
    #endif
#endif

    if (g_failures > 0) {
        fprintf(stderr, "FAILED (%d failures)\n", g_failures);
        return 1;
    }

    printf("PASSED\n");
    return 0;
}
