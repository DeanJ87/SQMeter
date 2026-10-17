#include "DisplayFrame.h"

#include "DisplayFonts.h"

#include <cstring>

namespace SQM::Display
{
    namespace
    {
        constexpr uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
        constexpr uint64_t FNV_PRIME = 0x100000001B3ULL;

        // FNV-1a over one 128-byte page: a changed page is resent, an
        // unchanged one isn't (research D2). `data` null: a blank page.
        uint64_t hashPage(const uint8_t *data)
        {
            uint64_t hash = FNV_OFFSET;
            for (int i = 0; i < WIDTH; ++i)
            {
                hash ^= data != nullptr ? data[i] : 0U;
                hash *= FNV_PRIME;
            }
            return hash;
        }

        int glyphCount(const char *text)
        {
            int count = 0;
            while (text != nullptr && Fonts::nextCodepoint(text) != 0)
                ++count;
            return count;
        }

        int widthOf(int glyphs, int glyphWidth, int advance)
        {
            return glyphs == 0 ? 0 : (glyphs - 1) * advance + glyphWidth;
        }
    } // namespace

    Shift shiftOffset(uint8_t position)
    {
        static const Shift STEPS[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        return STEPS[position % 4];
    }

    Frame::Frame(uint8_t *buffer)
        : buf(buffer)
    {
        const uint64_t blank = hashPage(nullptr);
        for (uint64_t &h : pageHash)
            h = blank;
    }

    void Frame::clear()
    {
        std::memset(buf, 0, FRAME_BYTES);
    }

    void Frame::setPixel(int x, int y, bool on)
    {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
            return;
        uint8_t &byte = buf[static_cast<size_t>(y / 8) * WIDTH + static_cast<size_t>(x)];
        const auto bit = static_cast<uint8_t>(1U << (y % 8));
        byte = on ? static_cast<uint8_t>(byte | bit) : static_cast<uint8_t>(byte & ~bit);
    }

    bool Frame::pixel(int x, int y) const
    {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
            return false;
        return (buf[static_cast<size_t>(y / 8) * WIDTH + static_cast<size_t>(x)] >> (y % 8)) & 1U;
    }

    void Frame::fillRect(int x, int y, int w, int h, bool on)
    {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                setPixel(xx, yy, on);
    }

    int Frame::textWidth(const char *text)
    {
        return widthOf(glyphCount(text), SMALL_W, SMALL_ADVANCE);
    }

    int Frame::largeWidth(const char *text)
    {
        return widthOf(glyphCount(text), LARGE_W, LARGE_ADVANCE);
    }

    int Frame::drawText(int x, int y, const char *text, int maxWidth, bool on)
    {
        if (text == nullptr)
            return 0;
        const int total = glyphCount(text);
        int shown = total;
        bool cut = false;
        if (widthOf(total, SMALL_W, SMALL_ADVANCE) > maxWidth)
        {
            // As many glyphs as fit with the ellipsis after them.
            cut = true;
            shown = 0;
            while (widthOf(shown + 2, SMALL_W, SMALL_ADVANCE) <= maxWidth)
                ++shown;
            if (widthOf(1, SMALL_W, SMALL_ADVANCE) > maxWidth)
                return 0;
        }
        auto drawGlyph = [&](int gx, const uint8_t *columns)
        {
            for (int col = 0; col < Fonts::SMALL_COLUMNS; ++col)
                for (int row = 0; row < SMALL_H; ++row)
                    if ((columns[col] >> row) & 1U)
                        setPixel(gx + col, y + row, on);
        };
        const char *p = text;
        int gx = x;
        for (int i = 0; i < shown; ++i, gx += SMALL_ADVANCE)
            drawGlyph(gx, Fonts::glyph(Fonts::nextCodepoint(p)));
        if (cut)
        {
            drawGlyph(gx, Fonts::glyph(Fonts::HORIZONTAL_ELLIPSIS));
            ++shown;
        }
        return widthOf(shown, SMALL_W, SMALL_ADVANCE);
    }

    int Frame::drawLarge(int x, int y, const char *text, bool on)
    {
        if (text == nullptr)
            return 0;
        const char *p = text;
        int gx = x;
        int count = 0;
        for (uint32_t cp = Fonts::nextCodepoint(p); cp != 0; cp = Fonts::nextCodepoint(p), gx += LARGE_ADVANCE, ++count)
        {
            const uint8_t *columns = Fonts::glyph(cp);
            // Each font pixel becomes 2 x 2, one row down in the 16-row cell.
            for (int col = 0; col < Fonts::SMALL_COLUMNS; ++col)
                for (int row = 0; row < SMALL_H; ++row)
                    if ((columns[col] >> row) & 1U)
                        fillRect(gx + col * 2, y + 1 + row * 2, 2, 2, on);
        }
        return widthOf(count, LARGE_W, LARGE_ADVANCE);
    }

    void Frame::commit()
    {
        for (int p = 0; p < PAGES; ++p)
        {
            const uint64_t h = hashPage(page(p));
            if (h != pageHash[p])
            {
                pageHash[p] = h;
                dirty = static_cast<uint8_t>(dirty | (1U << p));
            }
        }
    }

    void Frame::markClean(int index)
    {
        if (index >= 0 && index < PAGES)
            dirty = static_cast<uint8_t>(dirty & ~(1U << index));
    }

    int Frame::nextDirtyPage() const
    {
        for (int p = 0; p < PAGES; ++p)
            if ((dirty >> p) & 1U)
                return p;
        return -1;
    }
} // namespace SQM::Display
