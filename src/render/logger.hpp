#pragma once

#include <exception>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace Foyer {
    namespace logger {
        template <typename... Args>
        inline void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->debug(fmt, std::forward<Args>(args)...);
        }
        template <typename... Args>
        inline void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->info(fmt, std::forward<Args>(args)...);
        }
        template <typename... Args>
        inline void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->warn(fmt, std::forward<Args>(args)...);
        }
        template <typename... Args>
        inline void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->error(fmt, std::forward<Args>(args)...);
        }
        template <typename... Args>
        inline void error_throw(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->error(fmt, std::forward<Args>(args)...);
            throw std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...));
        }
        template <typename... Args>
        inline void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
            spdlog::default_logger_raw()->critical(fmt, std::forward<Args>(args)...);
            throw std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...));
        }
    }
}