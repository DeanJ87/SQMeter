#include "sensors/RG15StatusLines.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The RG-15's one-line status and acknowledgement messages, recorded in the
// sensor's diagnostics (src/sensors/RG15Sensor.cpp sends and reads them).

namespace SQM
{
    namespace
    {
        // "... EmTotal 1234 ...": the integer after a whole-word label.
        bool extractIntField(const std::string &line, const char *label, int &value)
        {
            size_t labelPos = line.find(label);
            const size_t labelLength = std::strlen(label);

            while (labelPos != std::string::npos)
            {
                const bool startsField =
                    labelPos == 0 || line[labelPos - 1] == ',' || std::isspace(static_cast<unsigned char>(line[labelPos - 1]));
                const size_t valuePos = labelPos + labelLength;
                const bool hasValueSeparator = valuePos < line.length() && std::isspace(static_cast<unsigned char>(line[valuePos]));

                if (startsField && hasValueSeparator)
                {
                    const char *cursor = line.c_str() + valuePos;
                    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
                    {
                        cursor++;
                    }

                    char *end = nullptr;
                    const long parsed = std::strtol(cursor, &end, 10);
                    if (end == cursor)
                    {
                        return false;
                    }

                    value = static_cast<int>(parsed);
                    return true;
                }

                labelPos = line.find(label, labelPos + 1);
            }

            return false;
        }

        bool startsWith(const std::string &line, const char *prefix)
        {
            return line.rfind(prefix, 0) == 0;
        }

        // "SW 1.000 2020.07.30": version, then build date.
        void recordSoftware(const std::string &line, RG15Diagnostics &d)
        {
            const size_t versionStart = 3;
            const size_t versionEnd = line.find(' ', versionStart);
            if (versionEnd == std::string::npos)
                return;
            d.softwareVersion = line.substr(versionStart, versionEnd - versionStart);
            const size_t buildStart = line.find_first_not_of(' ', versionEnd);
            if (buildStart != std::string::npos)
                d.softwareBuildDate = line.substr(buildStart);
        }

        void recordPowerOnDays(const std::string &line, RG15Diagnostics &d)
        {
            char *end = nullptr;
            const float days = std::strtof(line.c_str() + 8, &end);
            if (end != line.c_str() + 8)
                d.powerOnDays = days;
        }

        void recordEmitters(const std::string &line, RG15Diagnostics &d)
        {
            int emitter1 = 0;
            int emitter2 = 0;
            int emitterTotal = 0;
            if (std::sscanf(line.c_str(), "Emitters %d %d", &emitter1, &emitter2) >= 2)
            {
                d.emitter1 = emitter1;
                d.emitter2 = emitter2;
            }
            if (extractIntField(line, "EmTotal", emitterTotal))
                d.emitterTotal = emitterTotal;
        }

        void recordEmitterTotal(const std::string &line, RG15Diagnostics &d)
        {
            int total = 0;
            if (std::sscanf(line.c_str(), "EmTotal %d", &total) == 1)
                d.emitterTotal = total;
        }
    } // namespace

    bool isAsyncAck(char c)
    {
        return c == 'p' || c == 'c' || c == 'm' || c == 'i' || c == 'h' || c == 'l' || c == 's' || c == 'x' || c == 'y' || c == 'o';
    }

    bool isStatusLine(const std::string &line)
    {
        static const char *const PREFIXES[] = {"Baud ", "Reset ", "SW ", "Emitters ", "EmTotal ", "PwrDays ", "Event", ";"};
        for (const char *prefix : PREFIXES)
            if (startsWith(line, prefix))
                return true;
        return false;
    }

    void recordStatusLine(const std::string &line, RG15Diagnostics &d)
    {
        if (startsWith(line, "Reset "))
            d.resetReason = line.substr(6);
        else if (startsWith(line, "SW "))
            recordSoftware(line, d);
        else if (startsWith(line, "PwrDays "))
            recordPowerOnDays(line, d);
        else if (startsWith(line, "Emitters "))
            recordEmitters(line, d);
        else if (startsWith(line, "EmTotal "))
            recordEmitterTotal(line, d);
    }
} // namespace SQM
