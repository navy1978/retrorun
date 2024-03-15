#ifndef LOGGER_H
#define LOGGER_H

#include <ctime>
#include <cstdarg>
#include <string>
#include "libretro.h"

class Logger {
public:
    enum LogLevel {
        DEB,
        INF,
        WARN,
        ERR
    };

    Logger(LogLevel level = INF);

    void setLogLevel(LogLevel level);

    void log(LogLevel level, const char* format, ...);

    static void setCoreName(const std::string& coreName);

    static void core_log(enum retro_log_level level, const char *fmt, ...);
    

private:
    LogLevel logLevel_;
    
};

#endif // LOGGER_H
