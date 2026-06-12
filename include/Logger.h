#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <cstdint>

enum class LogLevel : std::uint8_t {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

class Logger {
public:
    static auto getInstance() -> Logger&;

    Logger(const Logger&) = delete;
    auto operator=(const Logger&) -> Logger& = delete;
    Logger(Logger&&) = delete;
    auto operator=(Logger&&) -> Logger& = delete;

    // Configure the log file. Once set, logs write to this file and console output is disabled.
    auto setLogFile(const std::string& path) -> void;
    auto setMinLogLevel(LogLevel level) -> void;
    auto suppressConsole(bool suppress) -> void;

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto log(LogLevel level, const std::string& message, const std::string& file = "", int line = 0) -> void;

private:
    Logger() = default;
    ~Logger();

    std::mutex mutex_;
    std::ofstream file_stream_;
    LogLevel min_level_{LogLevel::Info};
    bool suppress_console_{false};
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::Debug, msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::Info, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::getInstance().log(LogLevel::Warning, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::Error, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) Logger::getInstance().log(LogLevel::Fatal, msg, __FILE__, __LINE__)
// NOLINTEND(cppcoreguidelines-macro-usage)
