#include <ctime>
#include <cstdarg>
#include "logger.h"
#include "libretro.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <algorithm>

static std::string coreName_;
static Logger::LogLevel globalLogLevel = Logger::ERR;  // Default to INFO

Logger::Logger(LogLevel level) : logLevel_(level) {}

void Logger::setLogLevel(LogLevel level) {
    logLevel_ = level;
    globalLogLevel = level; // Ensure the global log level is updated
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (level >= logLevel_) {
        static const char* labels[] = {"[DEBUG] ", "[INFO] ", "[WARNING] ", "[ERROR] "};
        char buffer[4096];
        int used = std::snprintf(buffer, sizeof(buffer), " > RetroRun <  %s", labels[level]);
        if (used < 0) return;
        va_list args;
        va_start(args, format);
        int written = std::vsnprintf(buffer + used, sizeof(buffer) - used, format, args);
        va_end(args);
        if (written < 0) return;
        size_t length = std::min(sizeof(buffer) - 1,
                                 static_cast<size_t>(used) + static_cast<size_t>(written));
        if (length == 0 || buffer[length - 1] != '\n') buffer[length++] = '\n';
        std::fwrite(buffer, 1, length, stdout);
        if (level >= WARN) std::fflush(stdout);
    }
}

void Logger::setCoreName(const std::string &coreName) {
    coreName_ = coreName;
}

void Logger::core_log(enum retro_log_level level, const char* fmt, ...) {
    static Logger logger(globalLogLevel); // Ensure it uses the correct log level

    if (coreName_.empty()) {
        coreName_ = "Unknown Core";
    }

    // Correctly map `retro_log_level` to `Logger::LogLevel`
    Logger::LogLevel coreLogLevel;
    switch (level) {
    case RETRO_LOG_DEBUG: coreLogLevel = DEB; break;
    case RETRO_LOG_INFO: coreLogLevel = INF; break;
    case RETRO_LOG_WARN: coreLogLevel = WARN; break;
    case RETRO_LOG_ERROR: coreLogLevel = ERR; break;
    default: coreLogLevel = INF; break;
    }

    
    if (coreLogLevel < globalLogLevel) {
        return; // Don't log messages that are below the set logging level
    }

    static const char* labels[] = {"[DEBUG] ", "[INFO] ", "[WARNING] ", "[ERROR] "};
    char buffer[4096];
    int used = std::snprintf(buffer, sizeof(buffer), "> %s < %s",
                             coreName_.c_str(), labels[coreLogLevel]);
    if (used < 0) return;
    va_list args;
    va_start(args, fmt);
    int written = std::vsnprintf(buffer + used, sizeof(buffer) - used, fmt, args);
    va_end(args);
    if (written < 0) return;
    size_t length = std::min(sizeof(buffer) - 1,
                             static_cast<size_t>(used) + static_cast<size_t>(written));
    std::fwrite(buffer, 1, length, stdout);
    if (coreLogLevel >= WARN) std::fflush(stdout);
}
