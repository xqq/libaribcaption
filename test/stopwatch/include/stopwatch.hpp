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

#ifndef ARIBCAPTION_STOPWATCH_HPP
#define ARIBCAPTION_STOPWATCH_HPP

#ifdef __MACH__
    #include <mach/clock.h>
    #include <mach/mach.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

#include <ctime>
#include <memory>

class StopWatch {
public:
    static inline std::unique_ptr<StopWatch> Create();
public:
    StopWatch() = default;
    virtual ~StopWatch() = default;
public:
    virtual void Start() = 0;

    virtual void Stop() = 0;

    virtual void Reset() = 0;

    virtual int64_t GetMicroseconds() = 0;
};

#ifdef __MACH__

class StopWatchMac : public StopWatch {
public:
    StopWatchMac() {
        host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_serv_);
    }
    ~StopWatchMac() override {
        mach_port_deallocate(mach_host_self(), clock_serv_);
    }
public:
    void Start() override {
        clock_get_time(clock_serv_, &start_mts_);
    }

    void Stop() override {
        mach_timespec_t current_mts{};
        clock_get_time(clock_serv_, &current_mts);
        elapsed_mts_.tv_sec = current_mts.tv_sec - start_mts_.tv_sec;
        elapsed_mts_.tv_nsec = current_mts.tv_nsec - start_mts_.tv_nsec;
    }

    void Reset() override {
        start_mts_ = mach_timespec_t{};
        elapsed_mts_ = mach_timespec_t{};
    }

    int64_t GetMicroseconds() override {
        return elapsed_mts_.tv_sec * 1000000 + elapsed_mts_.tv_nsec / 1000;
    }
private:
    clock_serv_t clock_serv_{};
    mach_timespec_t start_mts_{};
    mach_timespec_t elapsed_mts_{};
};

#elif defined(__linux__) || defined(__unix__)

class StopWatchPOSIX : public StopWatch {
public:
    StopWatchPOSIX() = default;
    ~StopWatchPOSIX() override = default;
public:
    void Start() override {
        clock_gettime(CLOCK_MONOTONIC, &start_time_);
    }

    void Stop() override {
        timespec current_time{};
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed_time_.tv_sec = current_time.tv_sec - start_time_.tv_sec;
        elapsed_time_.tv_nsec = current_time.tv_nsec - start_time_.tv_nsec;
    }

    void Reset() override {
        start_time_ = timespec{};
        elapsed_time_ = timespec{};
    }

    int64_t GetMicroseconds() override {
        return elapsed_time_.tv_sec * 1000000 + elapsed_time_.tv_nsec / 1000;
    }
private:
    timespec start_time_{};
    timespec elapsed_time_{};
};

#elif defined(_WIN32)

class StopWatchWin : public StopWatch {
public:
    StopWatchWin() {
        QueryPerformanceFrequency(&frequency_);
    }

    ~StopWatchWin() override = default;
public:
    void Start() override {
        QueryPerformanceCounter(&start_time_);
    }

    void Stop() override {
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        elapsed_time_.QuadPart = current_time.QuadPart - start_time_.QuadPart;
    }

    void Reset() override {
        start_time_ = LARGE_INTEGER{};
        elapsed_time_ = LARGE_INTEGER{};
    }

    int64_t GetMicroseconds() override {
        return elapsed_time_.QuadPart * 1000000 / frequency_.QuadPart;
    }
private:
    LARGE_INTEGER frequency_{};
    LARGE_INTEGER start_time_{};
    LARGE_INTEGER elapsed_time_{};
};

#endif

inline std::unique_ptr<StopWatch> StopWatch::Create() {
#ifdef __MACH__
    return std::make_unique<StopWatchMac>();
#elif defined(__linux__) || defined(__unix__)
    return std::make_unique<StopWatchPOSIX>();
#elif defined(_WIN32)
    return std::make_unique<StopWatchWin>();
#else
    static_assert(false, "No available StopWatch implementation");
#endif
}

#endif  // ARIBCAPTION_STOPWATCH_HPP
