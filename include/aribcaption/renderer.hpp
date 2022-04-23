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

#ifndef ARIBCAPTION_RENDERER_HPP
#define ARIBCAPTION_RENDERER_HPP

#include <memory>
#include <optional>
#include "aribcc_config.h"
#include "aribcc_export.h"
#include "context.hpp"
#include "caption.hpp"
#include "image.hpp"

namespace aribcaption {

/**
 * Enums for FontProvider indication
 */
enum class FontProviderType {
    /**
     * Detect and select FontProvider automatically. Should be used in most cases.
     */
    kAuto = 0,

#if defined(ARIBCC_USE_CORETEXT)
    /**
     * FontProvider relies on Apple CoreText API. Available on macOS and iOS.
     */
    kCoreText = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    /**
     * FontProvider relies on DirectWrite API. Available on Windows 7+.
     */
    kDirectWrite = 2,
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
    /**
     * FontProvider using libfontconfig. Usually been used on Linux platforms.
     */
    kFontconfig = 3,
#endif

#if defined(ARIBCC_IS_ANDROID)
    /**
     * FontProvider for Android. Available on Android 2.x+.
     */
    kAndroid = 4,
#endif

#if defined(ARIBCC_USE_GDI_FONT)
    /**
     * FontProvder based on Win32 GDI API. Available on Windows 2000+.
     */
    kGDI = 5,
#endif
};

/**
 * Enums for TextRenderer indication
 */
enum class TextRendererType {
    /**
     * Detect and select TextRenderer automatically. Should be used in most cases.
     */
    kAuto = 0,

#if defined(ARIBCC_USE_CORETEXT)
    /**
     * Apple CoreText API based TextRenderer. Available on macOS and iOS.
     */
    kCoreText = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    /**
     * DirectWrite API based TextRenderer. Available on Windows 7+.
     */
    kDirectWrite = 2,
#endif

#if defined(ARIBCC_USE_FREETYPE)
    /**
     * Freetype based TextRenderer. Available on all platforms.
     */
    kFreetype = 3,
#endif
};

namespace internal { class RendererImpl; }

/**
 * Enums for Renderer's caption storage policy indication
 */
enum class CaptionStoragePolicy {
    /**
     * The renderer will only keep minimal amount of captions for subsequent rendering if necessary.
     * This is the default behavior.
     */
    kMinimum = 0,

    /**
     * The renderer will never evict appended captions unless you call the @Renderer::Flush().
     * This may result in memory waste. Use at your own risk.
     */
    kUnlimited = 1,

    /**
     * The renderer will keep appended captions at an upper limit of count.
     */
    kUpperLimitCount = 2,

    /**
     * The renderer will keep appended captions at an upper limit of duration, in milliseconds.
     */
    kUpperLimitDuration = 3,
};

/**
 * Enums for reporting rendering status
 *
 * See @Renderer::Render()
 */
enum class RenderStatus {
    kError = 0,
    kNoImage = 1,
    kGotImage = 2,
    kGotImageUnchanged = 3,
};

/**
 * Structure for holding rendered caption images
 */
struct RenderResult {
    int64_t pts = 0;             ///< PTS of rendered caption
    int64_t duration = 0;        ///< duration of rendered caption, may be DURATION_INDEFINITE
    std::vector<Image> images;
};

/**
 * ARIB STD-B24 caption renderer
 */
class Renderer {
public:
    /**
     * A context is needed for constructing the Renderer.
     *
     * The context shouldn't be destructed before any other object constructed from the context has been destructed.
     */
    ARIBCC_API explicit Renderer(Context& context);
    ARIBCC_API ~Renderer();
    ARIBCC_API Renderer(Renderer&&) noexcept;
    ARIBCC_API Renderer& operator=(Renderer&&) noexcept;
public:
    /**
     * Initialize function must be called before calling any other member functions.
     *
     * @param caption_type        Indicate caption type (kCaption / kSuperimpose)
     * @param font_provider_type  Indicate @FontProviderType. Use kAuto in most cases.
     * @param text_renderer_type  Indicate @TextRendererType. Use kAuto in most cases.
     * @return true on success
     */
    ARIBCC_API bool Initialize(CaptionType caption_type = CaptionType::kCaption,
                               FontProviderType font_provider_type = FontProviderType::kAuto,
                               TextRendererType text_renderer_type = TextRendererType::kAuto);

    /**
     * Indicate stroke width for stroke text, in dots (relative)
     * @param dots must >= 0.0f
     */
    ARIBCC_API void SetStrokeWidth(float dots);

    /**
     * Indicate whether render replaced DRCS characters as Unicode characters
     * @param replace default as true
     */
    ARIBCC_API void SetReplaceDRCS(bool replace);

    /**
     * Indicate whether always render stroke text for all characters regardless of the indication by CharStyle
     * @param force_stroke default as false
     */
    ARIBCC_API void SetForceStrokeText(bool force_stroke);

    /**
     * Indicate whether ignore rendering for ruby-like (furigana) characters
     * @param force_no_ruby default as false
     */
    ARIBCC_API void SetForceNoRuby(bool force_no_ruby);

    /**
     * Indicate whether ignore background color rendering
     * @param force_no_background default as false
     */
    ARIBCC_API void SetForceNoBackground(bool force_no_background);

    /**
     * Merge rendered region images into one big image on Render() call.
     * @param merge default as false
     */
    ARIBCC_API void SetMergeRegionImages(bool merge);

    /**
     * Indicate font families (an array of font family names) for default usage
     *
     * Default font family will be used only if captions' language is unknown (iso6392_language_code == 0).
     * Indicate force_default = true to force use these fonts always.
     *
     * The renderer contains an auto-fallback mechanism among indicated font families.
     * The renderer also contains predefined font indications for Windows / macOS / Linux / Android.
     *
     * @param font_family    Array of font family names
     * @param force_default  Whether force use these font families for all languages
     * @return true on success
     */
    ARIBCC_API bool SetDefaultFontFamily(const std::vector<std::string>& font_family, bool force_default);

    /**
     * Indicate font families (an array of font family names) for specific language
     *
     * The renderer contains an auto-fallback mechanism among indicated font families.
     * The renderer also contains predefined font indications for Windows / macOS / Linux / Android.
     *
     * @param language_code ISO639-2 Language Code, e.g. ThreeCC("jpn")
     * @param font_family   Array of font family names
     * @return true on success
     */
    ARIBCC_API bool SetLanguageSpecificFontFamily(uint32_t language_code, const std::vector<std::string>& font_family);

    /**
     * Set the renderer frame size in pixels, include margins. This function must be called before any @Render() call.
     *
     * Usually rendered images will be inside this frame area, unless negative margin values are specified.
     *
     * @param frame_width   must be >= 0
     * @param frame_height  must be >= 0
     * @return true on success
     */
    ARIBCC_API bool SetFrameSize(int frame_width, int frame_height);

    /**
     * Set the frame margins in pixels. This function must be called after calling to @SetFrameSize().
     * Call to this function is optional.
     *
     * Each value specifics the distance from the video rectangle to the renderer frame.
     * Positive margin value means there will be free space between renderer frame and video area,
     * while negative margin value lets renderer frame (visible area) inside the video, i.e. the video is cropped.
     *
     * If negative margin value has been indicated, rendered images may outside the renderer frame.
     *
     * @return true on success
     */
    ARIBCC_API bool SetMargins(int top, int bottom, int left, int right);

    /**
     * Set storage policy for renderer's internal caption storage
     *
     * @param policy       See @CaptionStoragePolicy
     * @param upper_limit  Optional parameter, but must has a value for kUpperLimitCount & kUpperLimitDuration
     */
    ARIBCC_API void SetStoragePolicy(CaptionStoragePolicy policy, std::optional<size_t> upper_limit = std::nullopt);

    /**
     * Append a caption into renderer's internal storage for subsequent rendering
     *
     * If a caption with same PTS already exists in the storage, it will be replaced by the new one.
     *
     * @param caption Caption's const reference
     * @return true on success
     */
    ARIBCC_API bool AppendCaption(const Caption& caption);

    /**
     * Append a caption into renderer's internal storage for subsequent rendering
     *
     * If a caption with same PTS already exists in the storage, it will be replaced by the new one.
     *
     * @param caption Caption's Rvalue reference, use std::move()
     * @return true on success
     */
    ARIBCC_API bool AppendCaption(Caption&& caption);

    /**
     * Retrieve expected RenderStatus at specific PTS, rather than actually do rendering.
     *
     * Useful for detecting whether will got an identical image that is unchanged from the previous rendering.
     *
     * @param pts    Presentation timestamp, in milliseconds
     * @return       kError / kNoImage / kGotImage / kGotImageUnchanged
     *               kGotImageUnchanged means a Render() call at this PTS will return an image identical to the previous
     */
    ARIBCC_API RenderStatus TryRender(int64_t pts);

    /**
     * Render caption at specific PTS
     *
     * This function queries a caption from the specified PTS and render it into images if exists.
     *
     * @param pts         Presentation timestamp, in milliseconds
     * @param out_result  Write back parameter for passing rendered images, will be empty if status is kError / kNoImage
     *
     * @return            kGotImage / kGotImageUnchanged if rendered images provided
     *                    kGotImageUnchanged means this batch of images is completely identical to the previous call
     */
    ARIBCC_API RenderStatus Render(int64_t pts, RenderResult& out_result);

    /**
     * Clear caption storage inside the renderer. Will evict all the appended captions.
     *
     * Call this function if the stream has been seeked, or met non-monotonic PTS.
     */
    ARIBCC_API void Flush();
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
private:
    std::unique_ptr<internal::RendererImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_RENDERER_HPP
