#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>

auto Logger::getInstance() -> Logger& {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

auto Logger::setLogFile(const std::string& path) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_stream_.open(path, std::ios::out | std::ios::app);
    if (!file_stream_.is_open()) {
        std::cerr << "[Logger Error] Failed to open log file at: " << path << "\n";
    }
}

auto Logger::setMinLogLevel(LogLevel level) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

auto Logger::suppressConsole(bool suppress) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    suppress_console_ = suppress;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Logger::log(LogLevel level, const std::string& message, const std::string& file, int line) -> void {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < min_level_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_struct{};
    localtime_r(&time_t_now, &tm_struct);

    std::ostringstream oss;
    oss << std::put_time(&tm_struct, "%Y-%m-%d %H:%M:%S");
    std::string timestamp = oss.str();

    std::string level_str;
    switch (level) {
        case LogLevel::Debug:   level_str = "DEBUG";   break;
        case LogLevel::Info:    level_str = "INFO";    break;
        case LogLevel::Warning: level_str = "WARNING"; break;
        case LogLevel::Error:   level_str = "ERROR";   break;
        case LogLevel::Fatal:   level_str = "FATAL";   break;
    }

    std::string file_basename = file;
    if (!file.empty()) {
        std::filesystem::path filepath(file);
        file_basename = filepath.filename().string();
    }

    std::ostringstream log_line;
    log_line << "[" << timestamp << "] [" << level_str << "] " << message;
    if (!file_basename.empty()) {
        log_line << " (" << file_basename << ":" << line << ")";
    }
    log_line << "\n";

    std::string log_str = log_line.str();

    if (file_stream_.is_open()) {
        file_stream_ << log_str;
        file_stream_.flush();
    } else if (!suppress_console_) {
        if (level >= LogLevel::Warning) {
            std::cerr << log_str;
            std::cerr.flush();
        } else {
            std::cout << log_str;
            std::cout.flush();
        }
    }
}
