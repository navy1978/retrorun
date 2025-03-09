#include <ctime>
#include <cstdarg>
#include "logger.h"
#include "libretro.h"
#include <iostream>

static std::string coreName_;
static Logger::LogLevel globalLogLevel = Logger::INF;  // Default to INFO

Logger::Logger(LogLevel level) : logLevel_(level) {}

void Logger::setLogLevel(LogLevel level) {
    logLevel_ = level;
    globalLogLevel = level; // Ensure the global log level is updated
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (level >= logLevel_) {
        std::string prefix = " > RetroRun < ";
        printf("%s ", prefix.c_str());

        switch (level) {
        case DEB: printf("[DEBUG] "); break;
        case INF: printf("[INFO] "); break;
        case WARN: printf("[WARNING] "); break;
        case ERR: printf("[ERROR] "); break;
        }

        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
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

    printf("> %s < ", coreName_.c_str());

    switch (coreLogLevel) {
    case DEB: printf("[DEBUG] "); break;
    case INF: printf("[INFO] "); break;
    case WARN: printf("[WARNING] "); break;
    case ERR: printf("[ERROR] "); break;
    }

    // Print log message without an extra newline
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    //printf("\n"); // Only add one newline at the end
    fflush(stdout); // Ensure immediate printing
}