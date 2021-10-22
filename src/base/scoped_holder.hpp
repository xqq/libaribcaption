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

#ifndef ARIBCAPTION_SCOPED_HOLDER_HPP
#define ARIBCAPTION_SCOPED_HOLDER_HPP

#include <memory>

namespace aribcaption {

template <class T, class Deleter = void(*)(T)>
class ScopedHolder {
public:
    ScopedHolder() : inner_(0), deleter_(nullptr) {}

    ScopedHolder(T inner, Deleter deleter) : inner_(inner), deleter_(deleter) {}

    template <class ParamType, class RetType>
    ScopedHolder(T inner, RetType(*deleter)(ParamType)) : inner_(inner), deleter_(reinterpret_cast<Deleter>(deleter)) {}

    // Allow move construct
    ScopedHolder(ScopedHolder<T, Deleter>&& holder) noexcept : inner_(holder.inner_), deleter_(holder.deleter_) {
        holder.inner_ = 0;
        holder.deleter_ = nullptr;
    }

    ~ScopedHolder() noexcept {
        if (inner_) {
            deleter_(inner_);
            inner_ = 0;
        }
    }

    [[nodiscard]]
    T Get() const noexcept {
        return inner_;
    }

    operator T() const noexcept {
        return inner_;
    }

    T operator->() const noexcept {
        return inner_;
    }

    T* operator&() noexcept {
        return std::addressof(inner_);
    }

    [[nodiscard]]
    T* GetAddressOf() noexcept {
        return std::addressof(inner_);
    }

    [[nodiscard]]
    T* ReleaseAndGetAddressOf() noexcept {
        if (inner_) {
            deleter_(inner_);
            inner_ = 0;
        }
        return std::addressof(inner_);
    }

    [[nodiscard]]
    T Take() noexcept {
        T inner = inner_;
        inner_ = 0;
        return inner;
    }

    void Reset() noexcept {
        if (inner_) {
            deleter_(inner_);
            inner_ = 0;
        }
    }

    ScopedHolder& operator=(T new_inner) noexcept {
        if (inner_) {
            deleter_(inner_);
        }
        inner_ = new_inner;
        return *this;
    }

    // Allow move assign
    ScopedHolder& operator=(ScopedHolder<T, Deleter>&& rhs) noexcept {
        if (inner_) {
            deleter_(inner_);
        }
        inner_ = rhs.inner_;
        deleter_ = rhs.deleter_;
        rhs.inner_ = 0;
        rhs.deleter_ = nullptr;
        return *this;
    }
public:
    // Disallow copy and assign
    ScopedHolder(const ScopedHolder&) = delete;
    ScopedHolder& operator=(const ScopedHolder&) = delete;
private:
    T inner_;
    Deleter deleter_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_SCOPED_HOLDER_HPP
