#include "core/Log.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace arcadeblocks::core {
namespace {

std::ofstream logStream;
LogLevel minLevel = LogLevel::Info;
std::mutex logMutex;

int severity(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return 0;
    case LogLevel::Debug:
        return 1;
    case LogLevel::Info:
        return 2;
    case LogLevel::Warn:
        return 3;
    case LogLevel::Error:
        return 4;
    }
    return 2;
}

std::string_view levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

} // namespace

bool Log::initialize(const std::filesystem::path& logFile, LogLevel minimumLevel) {
    std::lock_guard lock{logMutex};
    minLevel = minimumLevel;
    logStream.open(logFile, std::ios::out | std::ios::app);
    return logStream.is_open();
}

void Log::shutdown() {
    std::lock_guard lock{logMutex};
    if (logStream.is_open()) {
        logStream.flush();
        logStream.close();
    }
}

void Log::trace(const std::string& message) {
    write(LogLevel::Trace, message);
}

void Log::debug(const std::string& message) {
    write(LogLevel::Debug, message);
}

void Log::info(const std::string& message) {
    write(LogLevel::Info, message);
}

void Log::warn(const std::string& message) {
    write(LogLevel::Warn, message);
}

void Log::error(const std::string& message) {
    write(LogLevel::Error, message);
}

void Log::write(LogLevel level, const std::string& message) {
    if (severity(level) < severity(minLevel)) {
        return;
    }

    std::lock_guard lock{logMutex};

    const auto line = timestamp() + " [" + std::string{levelName(level)} + "] " + message;
    std::cerr << line << '\n';
    if (logStream.is_open()) {
        logStream << line << '\n';
        logStream.flush();
    }
}

} // namespace arcadeblocks::core
