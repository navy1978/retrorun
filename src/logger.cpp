
#include <ctime>
#include <cstdarg> // Include this for va_start, va_list, etc.
#include "logger.h"
#include <iostream>

Logger::Logger(LogLevel level) : logLevel_(level) {}

void Logger::setLogLevel(LogLevel level) {
    logLevel_ = level;
}

void Logger::log(LogLevel level, const char* format, ...) {
    
    if (level >= logLevel_) {
        
        // Get current time
       /* std::time_t now = std::time(nullptr);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "[RR %Y-%m-%d %H:%M:%S]", std::localtime(&now));*/
        std::string prefix=" > RetroRun < ";

        // Print log message to console
        //printf("%s ", timeStr);
        printf("%s ", prefix.c_str());
        // Print log level
        switch (level) {
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
