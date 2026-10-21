#pragma once

#include <cstdint>

// Glyph lookup for DisplayFrame.cpp (private to lib/DisplayLogic).

namespace SQM::Display::Fonts
{
    constexpr int SMALL_COLUMNS = 5;
    constexpr uint32_t DEGREE_SIGN = 0xB0;
    constexpr uint32_t HORIZONTAL_ELLIPSIS = 0x2026;

    // Five column bytes (bit 0 = top row) for a code point; '?' when the
    // font has no glyph for it.
    const uint8_t *glyph(uint32_t codepoint);

    // Decodes one UTF-8 code point and advances `text`; 0 at the end.
    uint32_t nextCodepoint(const char *&text);
} // namespace SQM::Display::Fonts
