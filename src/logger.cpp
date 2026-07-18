#include <ctime>
#include <cstdarg>
#include "logger.h"
#include "libretro.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <algorithm>

static std::string coreName_;
static Logger::LogLevel coreLogLevel_ = Logger::ERR;
static std::FILE *logFile_ = nullptr;

static void writeLogBuffer(const char *buffer, size_t length, bool flush)
{
    std::fwrite(buffer, 1, length, stdout);
    if (logFile_)
    {
        std::fwrite(buffer, 1, length, logFile_);
        if (flush)
            std::fflush(logFile_);
    }
    if (flush)
        std::fflush(stdout);
}

static int writeTimestamp(char *buffer, size_t size)
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    return static_cast<int>(std::strftime(buffer, size, "[%Y-%m-%d %H:%M:%S] ", &localTime));
}

Logger::Logger(LogLevel level) : logLevel_(level) {}

Logger::~Logger()
{
    if (logFile_)
    {
        std::fclose(logFile_);
        logFile_ = nullptr;
    }
}

void Logger::setLogLevel(LogLevel level) {
    logLevel_ = level;
}

bool Logger::enableFileLogging(const std::string& path)
{
    std::FILE *file = std::fopen(path.c_str(), "w");
    if (!file)
        return false;

    if (logFile_)
        std::fclose(logFile_);
    logFile_ = file;
    std::setvbuf(logFile_, nullptr, _IOLBF, 0);
    return true;
}

void Logger::setCoreLogLevel(LogLevel level) {
    coreLogLevel_ = level;
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (level >= logLevel_) {
        static const char* labels[] = {"[DEBUG] ", "[INFO] ", "[WARNING] ", "[ERROR] "};
        char buffer[4096];
        int used = writeTimestamp(buffer, sizeof(buffer));
        if (used <= 0) return;
        const int prefix = std::snprintf(buffer + used, sizeof(buffer) - used,
                                         "> RetroRun <  %s", labels[level]);
        if (prefix < 0) return;
        used += prefix;
        if (used < 0) return;
        va_list args;
        va_start(args, format);
        int written = std::vsnprintf(buffer + used, sizeof(buffer) - used, format, args);
        va_end(args);
        if (written < 0) return;
        size_t length = std::min(sizeof(buffer) - 1,
                                 static_cast<size_t>(used) + static_cast<size_t>(written));
        if (length == 0 || buffer[length - 1] != '\n') buffer[length++] = '\n';
        writeLogBuffer(buffer, length, level >= WARN);
    }
}

void Logger::setCoreName(const std::string &coreName) {
    coreName_ = coreName;
}

void Logger::core_log(enum retro_log_level level, const char* fmt, ...) {
    if (coreName_.empty()) {
        coreName_ = "Unknown Core";
    }

    // Correctly map `retro_log_level` to `Logger::LogLevel`
    Logger::LogLevel messageLevel;
    switch (level) {
    case RETRO_LOG_DEBUG: messageLevel = DEB; break;
    case RETRO_LOG_INFO: messageLevel = INF; break;
    case RETRO_LOG_WARN: messageLevel = WARN; break;
    case RETRO_LOG_ERROR: messageLevel = ERR; break;
    default: messageLevel = INF; break;
    }

    
    if (messageLevel < coreLogLevel_) {
        return; // Don't log messages that are below the set logging level
    }

    static const char* labels[] = {"[DEBUG] ", "[INFO] ", "[WARNING] ", "[ERROR] "};
    char buffer[4096];
    int used = writeTimestamp(buffer, sizeof(buffer));
    if (used <= 0) return;
    const int prefix = std::snprintf(buffer + used, sizeof(buffer) - used, "> %s < %s",
                                     coreName_.c_str(), labels[messageLevel]);
    if (prefix < 0) return;
    used += prefix;
    if (used < 0) return;
    va_list args;
    va_start(args, fmt);
    int written = std::vsnprintf(buffer + used, sizeof(buffer) - used, fmt, args);
    va_end(args);
    if (written < 0) return;
    size_t length = std::min(sizeof(buffer) - 1,
                             static_cast<size_t>(used) + static_cast<size_t>(written));
    if (length == 0 || buffer[length - 1] != '\n') buffer[length++] = '\n';
    writeLogBuffer(buffer, length, messageLevel >= WARN);
}
