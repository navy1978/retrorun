#ifndef LOGGER_H
#define LOGGER_H

#include <ctime>
#include <cstdarg>

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
    

private:
    LogLevel logLevel_;
};

#endif // LOGGER_H
