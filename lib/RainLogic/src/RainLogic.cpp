#include "RainLogic.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace SQM
{
    namespace Rain
    {
        namespace
        {
            bool isSpace(char c)
            {
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            }

            // "<label> <number>" where label starts a field (line start, after
            // a comma or whitespace) - so "Acc" doesn't match inside "EventAcc".
            bool extractFloatField(const std::string &line, const char *label, float &value)
            {
                const size_t labelLength = std::strlen(label);
                for (size_t pos = line.find(label); pos != std::string::npos; pos = line.find(label, pos + 1))
                {
                    const bool startsField = pos == 0 || line[pos - 1] == ',' || isSpace(line[pos - 1]);
                    const size_t valuePos = pos + labelLength;
                    if (!startsField || valuePos >= line.length() || !isSpace(line[valuePos]))
                        continue;

                    const char *cursor = line.c_str() + valuePos;
                    while (*cursor != '\0' && isSpace(*cursor))
                        cursor++;
                    char *end = nullptr;
                    const float parsed = std::strtof(cursor, &end);
                    if (end == cursor)
                        return false;
                    value = parsed;
                    return true;
                }
                return false;
            }

            bool hasFlagToken(const std::string &flags, const char *token)
            {
                const size_t tokenLength = std::strlen(token);
                for (size_t pos = flags.find(token); pos != std::string::npos; pos = flags.find(token, pos + 1))
                {
                    const bool leftOk = pos == 0 || isSpace(flags[pos - 1]) || flags[pos - 1] == ',';
                    const size_t right = pos + tokenLength;
                    const bool rightOk = right >= flags.length() || isSpace(flags[right]) || flags[right] == ',';
                    if (leftOk && rightOk)
                        return true;
                }
                return false;
            }
        } // namespace

        namespace
        {
            bool within(float value, float max)
            {
                return value >= 0.0f && value <= max; // false for NaN
            }

            bool fieldsInRange(const Line &parsed)
            {
                return within(parsed.acc, 9999.0f) && within(parsed.eventAcc, 9999.0f) && within(parsed.totalAcc, 999999.0f) &&
                       within(parsed.rInt, 9999.0f);
            }

            // Unit after RInt, then optional flags.
            void readUnitAndFlags(const std::string &line, Line &parsed)
            {
                const size_t rIntPos = line.find("RInt");
                size_t unitPos = line.find("mmph", rIntPos);
                if (unitPos == std::string::npos)
                {
                    unitPos = line.find("iph", rIntPos);
                    parsed.imperial = unitPos != std::string::npos;
                }
                if (unitPos == std::string::npos)
                    return;
                const size_t flagsStart = line.find(' ', unitPos);
                if (flagsStart == std::string::npos)
                    return;
                const std::string flags = line.substr(flagsStart);
                parsed.lensBad = hasFlagToken(flags, "i") || hasFlagToken(flags, "LensBad");
                parsed.emSat = hasFlagToken(flags, "o") || hasFlagToken(flags, "EmSat");
            }
        } // namespace

        ParseResult parseLine(const std::string &line, Line &out)
        {
            if (line.length() < 20)
                return ParseResult::TooShort;

            Line parsed;
            if (!extractFloatField(line, "Acc", parsed.acc) || !extractFloatField(line, "EventAcc", parsed.eventAcc) ||
                !extractFloatField(line, "TotalAcc", parsed.totalAcc) || !extractFloatField(line, "RInt", parsed.rInt))
                return ParseResult::MissingField;

            if (!fieldsInRange(parsed))
                return ParseResult::OutOfRange;
            readUnitAndFlags(line, parsed);
            out = parsed;
            return ParseResult::Ok;
        }

        void observe(Latch &latch, float rInt, float acc, uint32_t now, uint32_t clearDelayMs)
        {
            if (rInt > 0.0f || acc > 0.0f)
            {
                if (!latch.latched)
                    latch.eventAccumulation = 0.0f;
                latch.eventAccumulation += acc;
                latch.lastRainMs = now == 0 ? 1 : now; // 0 means "never"
                latch.latched = true;
                return;
            }
            expire(latch, now, clearDelayMs);
        }

        void expire(Latch &latch, uint32_t now, uint32_t clearDelayMs)
        {
            // Unsigned subtraction handles millis() wrap-around.
            if (latch.lastRainMs == 0 || now - latch.lastRainMs > clearDelayMs)
            {
                latch.latched = false;
                latch.eventAccumulation = 0.0f;
            }
        }

        uint32_t clearRemainingMs(const Latch &latch, uint32_t now, uint32_t clearDelayMs)
        {
            if (!latch.latched || latch.lastRainMs == 0)
                return 0;
            const uint32_t since = now - latch.lastRainMs; // wrap-safe
            return since < clearDelayMs ? clearDelayMs - since : 0;
        }

        namespace
        {
            constexpr int MINUTES_PER_DAY = 24 * 60;

            bool isLeapYear(int year)
            {
                return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
            }

            int32_t daysBeforeYear(int year)
            {
                int32_t days = 0;
                for (int y = 1970; y < year; y++)
                {
                    days += isLeapYear(y) ? 366 : 365;
                }
                return days;
            }
        } // namespace

        int32_t resetDay(const LocalTime &now, uint8_t resetHour, uint8_t resetMinute)
        {
            const int32_t day = daysBeforeYear(now.year) + now.yearDay;
            const int minuteOfDay = now.hour * 60 + now.minute;
            const int resetMinuteOfDay = (resetHour * 60 + resetMinute) % MINUTES_PER_DAY;
            // Before HH:MM the time still belongs to yesterday's reset day.
            return minuteOfDay >= resetMinuteOfDay ? day : day - 1;
        }

        ResetDecision dailyReset(const LocalTime &now, uint8_t resetHour, uint8_t resetMinute, int32_t lastResetDay)
        {
            const int32_t day = resetDay(now, resetHour, resetMinute);
            if (lastResetDay == NO_RESET_DAY)
            {
                return ResetDecision::Adopt;
            }
            // Strictly later only: a clock that steps backwards never resets twice.
            return day > lastResetDay ? ResetDecision::Reset : ResetDecision::Wait;
        }
    } // namespace Rain
} // namespace SQM
