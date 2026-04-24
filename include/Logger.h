#pragma once

#include <string>
#include <cstdint>

namespace SQM
{

    enum class LogLevel : uint8_t
    {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        FATAL = 4
    };

    class Logger
    {
    public:
        static void init();
        static void setLevel(LogLevel level);
        static LogLevel getLevel();

        static void debug(const char *tag, const char *format, ...);
        static void info(const char *tag, const char *format, ...);
        static void warn(const char *tag, const char *format, ...);
        static void error(const char *tag, const char *format, ...);
        static void fatal(const char *tag, const char *format, ...);

    private:
        static LogLevel currentLevel;
        static void log(LogLevel level, const char *tag, const char *format, va_list args);
        static const char *levelToString(LogLevel level);
        static const char *levelToColor(LogLevel level);
    };

} // namespace SQM
