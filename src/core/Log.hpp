#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace arcadeblocks::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

class Log {
public:
    static bool initialize(const std::filesystem::path& logFile, LogLevel minimumLevel);
    static void shutdown();

    static void trace(const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

private:
    static void write(LogLevel level, const std::string& message);
};

} // namespace arcadeblocks::core
