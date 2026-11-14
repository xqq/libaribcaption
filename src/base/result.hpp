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

#ifndef ARIBCAPTION_RESULT_HPP
#define ARIBCAPTION_RESULT_HPP

#include <cassert>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace aribcaption {

namespace internal {

// Holds references only: consume within the same full-expression, i.e. `return Ok(...);`
template <typename... Args>
struct Ok {
    std::tuple<Args&&...> args;
};

template <typename E>
struct Err {
    E&& val;
};

}  // namespace internal


template <typename... Args>
constexpr internal::Ok<Args...> Ok(Args&&... args) noexcept {
    return internal::Ok<Args...>{std::forward_as_tuple(std::forward<Args>(args)...)};
}

template <typename E>
constexpr internal::Err<E> Err(E&& val) noexcept {
    return internal::Err<E>{std::forward<E>(val)};
}


template <typename T, typename E>
class [[nodiscard]] Result {
    static_assert(!std::is_same<T, void>::value);
    static_assert(!std::is_same<E, void>::value);
public:
    Result() = delete;
    ~Result() = default;

    template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args&&...>, int> = 0>
    constexpr Result(internal::Ok<Args...>&& ok) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
        : Result(std::move(ok), std::index_sequence_for<Args...>{}) {}

    template <typename U, std::enable_if_t<std::is_constructible_v<E, U&&>, int> = 0>
    constexpr Result(internal::Err<U>&& err) noexcept(std::is_nothrow_constructible_v<E, U&&>)
        : variant_(std::in_place_index<1>, std::forward<U>(err.val)) {}

    constexpr Result(Result&&) noexcept = default;
    constexpr Result(const Result&) = default;

    Result& operator=(Result&&) noexcept = default;
    Result& operator=(const Result&) = default;
public:
    [[nodiscard]]
    constexpr bool is_ok() const noexcept {
        return variant_.index() == 0;
    }

    [[nodiscard]]
    constexpr bool is_err() const noexcept {
        return variant_.index() == 1;
    }

    //----------------------------------------------------
    // ok
    [[nodiscard]]
    constexpr T& value() & noexcept {
        assert(is_ok());
        T* val = std::get_if<0>(&variant_);
        return *val;
    }

    [[nodiscard]]
    constexpr const T& value() const& noexcept {
        assert(is_ok());
        const T* val = std::get_if<0>(&variant_);
        return *val;
    }

    [[nodiscard]]
    constexpr T&& value() && noexcept {
        assert(is_ok());
        T* val = std::get_if<0>(&variant_);
        return std::move(*val);
    }

    constexpr const T& operator*() const& noexcept {
        assert(is_ok());
        const T* val = std::get_if<0>(&variant_);
        return *val;
    }

    constexpr T& operator*() & noexcept {
        assert(is_ok());
        T* val = std::get_if<0>(&variant_);
        return *val;
    }

    constexpr T&& operator*() && noexcept {
        assert(is_ok());
        T* val = std::get_if<0>(&variant_);
        return std::move(*val);
    }

    constexpr const T* operator->() const noexcept {
        assert(is_ok());
        return std::get_if<0>(&variant_);
    }

    constexpr T* operator->() noexcept {
        assert(is_ok());
        return std::get_if<0>(&variant_);
    }

    //----------------------------------------------------
    // error
    [[nodiscard]]
    constexpr E& error() & noexcept {
        assert(is_err());
        E* err = std::get_if<1>(&variant_);
        return *err;
    }

    [[nodiscard]]
    constexpr const E& error() const& noexcept {
        assert(is_err());
        const E* err = std::get_if<1>(&variant_);
        return *err;
    }

    [[nodiscard]]
    constexpr E&& error() && noexcept {
        assert(is_err());
        E* err = std::get_if<1>(&variant_);
        return std::move(*err);
    }
public:
    friend constexpr bool operator==(const Result<T, E>& lhs, const Result<T, E>& rhs) {
        return lhs.variant_ == rhs.variant_;
    }

    friend constexpr bool operator!=(const Result<T, E>& lhs, const Result<T, E>& rhs) {
        return lhs.variant_ != rhs.variant_;
    }
private:
    template <typename... Args, size_t... I>
    constexpr Result(internal::Ok<Args...>&& ok, std::index_sequence<I...>)
        : variant_(std::in_place_index<0>, std::get<I>(std::move(ok.args))...) {}
private:
    std::variant<T, E> variant_;
};


}  // namespace aribcaption

#endif  // ARIBCAPTION_RESULT_HPP
