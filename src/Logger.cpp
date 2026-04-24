#include "Logger.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

namespace SQM
{

    LogLevel Logger::currentLevel = LogLevel::DEBUG;

    void Logger::init()
    {
        currentLevel = LogLevel::DEBUG;
    }

    void Logger::setLevel(LogLevel level)
    {
        currentLevel = level;
    }

    LogLevel Logger::getLevel()
    {
        return currentLevel;
    }

    void Logger::debug(const char *tag, const char *format, ...)
    {
        if (currentLevel <= LogLevel::DEBUG)
        {
            va_list args;
            va_start(args, format);
            log(LogLevel::DEBUG, tag, format, args);
            va_end(args);
        }
    }

    void Logger::info(const char *tag, const char *format, ...)
    {
        if (currentLevel <= LogLevel::INFO)
        {
            va_list args;
            va_start(args, format);
            log(LogLevel::INFO, tag, format, args);
            va_end(args);
        }
    }

    void Logger::warn(const char *tag, const char *format, ...)
    {
        if (currentLevel <= LogLevel::WARN)
        {
            va_list args;
            va_start(args, format);
            log(LogLevel::WARN, tag, format, args);
            va_end(args);
        }
    }

    void Logger::error(const char *tag, const char *format, ...)
    {
        if (currentLevel <= LogLevel::ERROR)
        {
            va_list args;
            va_start(args, format);
            log(LogLevel::ERROR, tag, format, args);
            va_end(args);
        }
    }

    void Logger::fatal(const char *tag, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        log(LogLevel::FATAL, tag, format, args);
        va_end(args);
    }

    void Logger::log(LogLevel level, const char *tag, const char *format, va_list args)
    {
        const uint32_t timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

        printf("%s[%u] [%s] [%s] ",
               levelToColor(level),
               timestamp,
               levelToString(level),
               tag);

        vprintf(format, args);
        printf("\033[0m\n"); // Reset color
    }

    const char *Logger::levelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO ";
        case LogLevel::WARN:
            return "WARN ";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "?????";
        }
    }

    const char *Logger::levelToColor(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "\033[36m"; // Cyan
        case LogLevel::INFO:
            return "\033[32m"; // Green
        case LogLevel::WARN:
            return "\033[33m"; // Yellow
        case LogLevel::ERROR:
            return "\033[31m"; // Red
        case LogLevel::FATAL:
            return "\033[35m"; // Magenta
        default:
            return "\033[0m";
        }
    }

} // namespace SQM
