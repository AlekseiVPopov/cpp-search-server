#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define PROFILE_CONCAT_INTERNAL2(X, Y) X##Y
#define PROFILE_CONCAT2(X, Y) PROFILE_CONCAT_INTERNAL2(X, Y)
#define UNIQUE_VAR_NAME_PROFILE2 PROFILE_CONCAT2(profileGuard2, __LINE__)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства

    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string &label, std::ostream &out = std::cerr) : label_(label), out_(out) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;

        out_ << label_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const std::string label_;
    std::ostream &out_;
    const Clock::time_point start_time_ = Clock::now();
};