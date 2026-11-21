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
        rr.SetTargetCaptionAreaRect(Rect(0, 0, 960, 540));

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
