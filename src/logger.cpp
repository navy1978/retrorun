
#include <ctime>
#include <cstdarg> // Include this for va_start, va_list, etc.
#include "logger.h"
#include "libretro.h"
#include <iostream>


static std::string coreName_; 


Logger::Logger(LogLevel level) : logLevel_(level) {}

void Logger::setLogLevel(LogLevel level)
{
    logLevel_ = level;
}

void Logger::log(LogLevel level, const char *format, ...)
{

    if (level >= logLevel_)
    {

        // Get current time
        /* std::time_t now = std::time(nullptr);
         char timeStr[64];
         std::strftime(timeStr, sizeof(timeStr), "[RR %Y-%m-%d %H:%M:%S]", std::localtime(&now));*/
        std::string prefix = " > RetroRun < ";

        // Print log message to console
        // printf("%s ", timeStr);
        printf("%s ", prefix.c_str());
        // Print log level
        switch (level)
        {
        case DEB:
            printf("[DEBUG] ");
            break;
        case INF:
            printf("[INFO] ");
            break;
        case WARN:
            printf("[WARNING] ");
            break;
        case ERR:
            printf("[ERROR] ");
            break;
        }

        // Print log message
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);

        printf("\n");
    }
}

void Logger::setCoreName(const std::string &coreName)
{
    coreName_ = coreName;
}

void Logger::core_log(enum retro_log_level level, const char *fmt, ...)
{
    // Create a Logger instance
    Logger logger;

    if (coreName_.empty())
    {
        // Handle error or set a default core name
        coreName_ = "Unknown Core";
    }

    // Map retro_log_level to LogLevel
    Logger::LogLevel logLevel;
    switch (level)
    {
    case 0:
        logLevel = DEB;
        break;
    case 1:
        logLevel = INF;
        break;
    case 2:
        logLevel = WARN;
        break;
    case 3:
        logLevel = ERR;
        break;
    default:
        logLevel = INF;
        break;
    }
    // Print core name
    printf("> %s < ", coreName_.c_str());

    // Print log level
    switch (logLevel)
    {
    case DEB:
        printf("[DEBUG] ");
        break;
    case INF:
        printf("[INFO] ");
        break;
    case WARN:
        printf("[WARNING] ");
        break;
    case ERR:
        printf("[ERROR] ");
        break;
    }

    // Print log message
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    //printf("\n");
}