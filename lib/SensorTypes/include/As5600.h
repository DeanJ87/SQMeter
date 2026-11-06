#pragma once

// AS5600 magnetic angle sensor (the wind vane), decoded from its registers.
// No hardware headers: shared by the wind driver and the native tests.

#include <cstdint>

namespace SQM
{
    namespace As5600
    {
        constexpr uint8_t ADDRESS = 0x36;
        // STATUS, then RAW ANGLE (high, low) in one read. RAW ANGLE ignores
        // any start/stop positions programmed into the chip; the firmware's
        // north offset corrects the mounting instead.
        constexpr uint8_t REG_STATUS = 0x0B;
        constexpr uint8_t REG_AGC = 0x1A;

        enum class Magnet : uint8_t
        {
            Off,      // vane switched off
            NoAnswer, // nothing at 0x36
            None,     // chip answers but sees no magnet
            Weak,     // too far from the chip: angle still read
            Strong,   // too close: angle still read
            Ok,
        };

        // STATUS bits: MD magnet detected, ML too weak, MH too strong.
        inline Magnet magnetFromStatus(uint8_t status)
        {
            if ((status & 0x20) == 0)
                return Magnet::None;
            if (status & 0x10)
                return Magnet::Weak;
            if (status & 0x08)
                return Magnet::Strong;
            return Magnet::Ok;
        }

        // A reading the vane direction can use.
        inline bool hasAngle(Magnet magnet)
        {
            return magnet == Magnet::Ok || magnet == Magnet::Weak || magnet == Magnet::Strong;
        }

        // 12-bit angle, 0 at the chip's zero, rising clockwise with DIR to GND.
        inline float angleDeg(uint8_t high, uint8_t low)
        {
            const uint16_t raw = static_cast<uint16_t>(((high & 0x0F) << 8) | low);
            return static_cast<float>(raw) * 360.0f / 4096.0f;
        }

        inline const char *magnetName(Magnet magnet)
        {
            switch (magnet)
            {
            case Magnet::NoAnswer:
                return "noAnswer";
            case Magnet::None:
                return "none";
            case Magnet::Weak:
                return "weak";
            case Magnet::Strong:
                return "strong";
            case Magnet::Ok:
                return "ok";
            case Magnet::Off:
            default:
                return "off";
            }
        }
    } // namespace As5600
} // namespace SQM
