#pragma once
namespace SQM
{
    struct Logger
    {
        template <typename... A> static void info(A...) {}
        template <typename... A> static void warn(A...) {}
        template <typename... A> static void error(A...) {}
        template <typename... A> static void debug(A...) {}
    };
} // namespace SQM
