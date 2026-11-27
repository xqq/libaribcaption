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

#ifndef ARIBCAPTION_FALLBACK_CODEPOINT_CACHE_HPP
#define ARIBCAPTION_FALLBACK_CODEPOINT_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace aribcaption {

// Lazily allocated, bounded 2-way set-associative cache mapping UCS-4 codepoints to fallback slot IDs
class FallbackCodepointCache {
public:
    static constexpr size_t kSetBits = 10;
    static constexpr size_t kNumSets = size_t{1} << kSetBits;
    static constexpr size_t kWays = 2;
    static constexpr size_t kCapacity = kNumSets * kWays;

    std::optional<uint32_t> Find(uint32_t ucs4) {
        if (ucs4 == 0 || entries_.empty()) {
            return std::nullopt;
        }
        size_t base = SetIndex(ucs4) * kWays;
        if (entries_[base].codepoint == ucs4) {
            return entries_[base].slot_id;
        }
        if (entries_[base + 1].codepoint == ucs4) {
            std::swap(entries_[base], entries_[base + 1]);
            return entries_[base].slot_id;
        }
        return std::nullopt;
    }

    void Insert(uint32_t ucs4, uint32_t slot_id) {
        if (ucs4 == 0 || slot_id == 0) {
            return;
        }
        if (entries_.empty()) {
            entries_.resize(kCapacity);
        }
        size_t base = SetIndex(ucs4) * kWays;
        if (entries_[base].codepoint == ucs4) {
            entries_[base].slot_id = slot_id;
            return;
        }
        entries_[base + 1] = entries_[base];
        entries_[base] = {ucs4, slot_id};
    }

    void InvalidateSlot(uint32_t slot_id) {
        if (slot_id == 0 || entries_.empty()) {
            return;
        }
        for (size_t base = 0; base < entries_.size(); base += kWays) {
            if (entries_[base + 1].slot_id == slot_id) {
                entries_[base + 1] = {};
            }
            if (entries_[base].slot_id == slot_id) {
                entries_[base] = entries_[base + 1];
                entries_[base + 1] = {};
            }
        }
    }

    void Clear() {
        entries_.clear();
    }
private:
    struct Entry {
        uint32_t codepoint = 0;
        uint32_t slot_id = 0;
    };

    static size_t SetIndex(uint32_t ucs4) {
        return static_cast<size_t>((ucs4 * 2654435761u) >> (32 - kSetBits)) & (kNumSets - 1);
    }

    std::vector<Entry> entries_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FALLBACK_CODEPOINT_CACHE_HPP
