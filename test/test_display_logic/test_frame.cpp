#include <unity.h>

#include <cstring>
#include <string>

#include "DisplayFrame.h"

using namespace SQM::Display;

namespace
{
    uint8_t buffer[FRAME_BYTES];

    int litInRect(const Frame &f, int x0, int y0, int w, int h)
    {
        int count = 0;
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x)
                count += f.pixel(x, y) ? 1 : 0;
        return count;
    }

    // Rightmost lit column in rows y0..y0+h-1, or -1.
    int rightmost(const Frame &f, int y0, int h)
    {
        for (int x = WIDTH - 1; x >= 0; --x)
            for (int y = y0; y < y0 + h; ++y)
                if (f.pixel(x, y))
                    return x;
        return -1;
    }
} // namespace

void test_frame_clear_and_pixels(void)
{
    std::memset(buffer, 0xAA, sizeof buffer);
    Frame f(buffer);
    f.clear();
    TEST_ASSERT_EQUAL(0, litInRect(f, 0, 0, WIDTH, HEIGHT));
    f.setPixel(0, 0, true);
    f.setPixel(127, 63, true);
    f.setPixel(5, 9, true);
    TEST_ASSERT_TRUE(f.pixel(0, 0));
    TEST_ASSERT_TRUE(f.pixel(127, 63));
    TEST_ASSERT_TRUE(f.pixel(5, 9));
    // SSD1306 page layout: byte = page * 128 + x, bit = y % 8.
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, buffer[7 * WIDTH + 127]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[1 * WIDTH + 5]);
    // Off the edge is ignored, never written.
    f.setPixel(-1, 0, true);
    f.setPixel(128, 0, true);
    f.setPixel(0, 64, true);
    TEST_ASSERT_FALSE(f.pixel(-1, 0));
    TEST_ASSERT_EQUAL(3, litInRect(f, 0, 0, WIDTH, HEIGHT));
    f.setPixel(5, 9, false);
    TEST_ASSERT_FALSE(f.pixel(5, 9));
}

void test_small_text_advance_and_glyphs(void)
{
    Frame f(buffer);
    f.clear();
    TEST_ASSERT_EQUAL(0, Frame::textWidth(""));
    TEST_ASSERT_EQUAL(5, Frame::textWidth("A"));
    TEST_ASSERT_EQUAL(17, Frame::textWidth("SQM"));
    // UTF-8 degree and ellipsis are one glyph each.
    TEST_ASSERT_EQUAL(
        17,
        Frame::textWidth("1\xC2\xB0"
                         "C"));
    TEST_ASSERT_EQUAL(5, Frame::textWidth("\xE2\x80\xA6"));

    const int drawn = f.drawText(0, 0, "II");
    TEST_ASSERT_EQUAL(11, drawn);
    // 'I' is 00 41 7F 41 00: its stem is column 2, the second one column 8 (6 px advance).
    for (int y = 0; y < 7; ++y)
    {
        TEST_ASSERT_TRUE(f.pixel(2, y));
        TEST_ASSERT_TRUE(f.pixel(8, y));
        TEST_ASSERT_FALSE(f.pixel(5, y)); // the gap between glyphs
    }
    TEST_ASSERT_FALSE(f.pixel(2, 7)); // 7 rows tall
    // A space draws nothing.
    f.clear();
    f.drawText(0, 0, "   ");
    TEST_ASSERT_EQUAL(0, litInRect(f, 0, 0, WIDTH, HEIGHT));
    // The degree sign is a small ring at the top.
    f.drawText(0, 0, "\xC2\xB0");
    TEST_ASSERT_TRUE(litInRect(f, 0, 0, 5, 4) > 0);
    TEST_ASSERT_EQUAL(0, litInRect(f, 0, 4, 5, 4));
}

void test_large_digits(void)
{
    Frame f(buffer);
    f.clear();
    TEST_ASSERT_EQUAL(10, Frame::largeWidth("1"));
    TEST_ASSERT_EQUAL(58, Frame::largeWidth("21.34")); // 5 glyphs, 12 px advance
    const int drawn = f.drawLarge(0, 0, "8");
    TEST_ASSERT_EQUAL(10, drawn);
    // Inside a 10 x 16 cell, nothing outside it.
    TEST_ASSERT_TRUE(litInRect(f, 0, 0, 10, 16) > 20);
    TEST_ASSERT_EQUAL(0, litInRect(f, 10, 0, 118, 64));
    TEST_ASSERT_EQUAL(0, litInRect(f, 0, 16, 128, 48));
    // '-', '.' and '%' are drawn.
    f.clear();
    f.drawLarge(0, 0, "-");
    TEST_ASSERT_TRUE(litInRect(f, 0, 0, 10, 16) > 0);
    f.clear();
    f.drawLarge(0, 0, ".");
    TEST_ASSERT_TRUE(litInRect(f, 0, 8, 10, 8) > 0);
    TEST_ASSERT_EQUAL(0, litInRect(f, 0, 0, 10, 8));
    f.clear();
    f.drawLarge(0, 0, "%");
    TEST_ASSERT_TRUE(litInRect(f, 0, 0, 10, 16) > 0);
}

void test_text_wider_than_line_ends_in_ellipsis(void)
{
    Frame f(buffer);
    f.clear();
    // 21 glyphs fit a 128 px line; 30 don't.
    const std::string fits(21, 'W');
    TEST_ASSERT_EQUAL(125, f.drawText(0, 0, fits.c_str(), WIDTH));
    const std::string longText(30, 'W');
    const int drawn = f.drawText(0, 8, longText.c_str(), WIDTH);
    TEST_ASSERT_TRUE(drawn <= WIDTH);
    TEST_ASSERT_EQUAL(125, drawn); // 20 glyphs + the ellipsis
    // The ellipsis is three dots on the baseline row of the last cell.
    const int cell = 20 * 6;
    TEST_ASSERT_TRUE(f.pixel(cell + 0, 8 + 6));
    TEST_ASSERT_TRUE(f.pixel(cell + 2, 8 + 6));
    TEST_ASSERT_TRUE(f.pixel(cell + 4, 8 + 6));
    TEST_ASSERT_EQUAL(0, litInRect(f, cell, 8, 5, 6));
    TEST_ASSERT_TRUE(rightmost(f, 8, 8) < WIDTH);
    // A narrower box cuts sooner.
    f.clear();
    TEST_ASSERT_EQUAL(29, f.drawText(0, 0, "Observatory", 30)); // 4 glyphs + ellipsis
    TEST_ASSERT_EQUAL(0, litInRect(f, 30, 0, 98, 8));
}

void test_dirty_pages_only_where_changed(void)
{
    Frame f(buffer);
    f.clear();
    f.markAllDirty();
    TEST_ASSERT_EQUAL_HEX8(0xFF, f.dirtyMask());
    for (int p = 0; p < PAGES; ++p)
        f.markClean(p);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.dirtyMask());

    // First render: the pages that hold something differ from the blank screen.
    f.clear();
    f.drawText(0, 0, "SQM");   // page 0
    f.drawText(0, 24, "Temp"); // page 3
    f.commit();
    TEST_ASSERT_EQUAL_HEX8(0x09, f.dirtyMask());
    f.markClean(0);
    f.markClean(3);

    // The same picture again: nothing to send.
    f.clear();
    f.drawText(0, 0, "SQM");
    f.drawText(0, 24, "Temp");
    f.commit();
    TEST_ASSERT_EQUAL_HEX8(0x00, f.dirtyMask());

    // One value changes: only its page.
    f.clear();
    f.drawText(0, 0, "SQM");
    f.drawText(0, 24, "Tamp");
    f.commit();
    TEST_ASSERT_EQUAL_HEX8(0x08, f.dirtyMask());
    TEST_ASSERT_EQUAL(3, f.nextDirtyPage());
    f.markClean(3);
    TEST_ASSERT_EQUAL(-1, f.nextDirtyPage());

    // A pixel shift moves text across a page boundary: both pages change.
    f.clear();
    f.drawText(0, 4, "SQM");
    f.drawText(0, 24, "Tamp");
    f.commit();
    TEST_ASSERT_EQUAL_HEX8(0x03, f.dirtyMask());
    TEST_ASSERT_EQUAL_PTR(buffer + WIDTH, f.page(1));
}

void test_pixel_shift_offsets(void)
{
    const int expected[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (uint8_t i = 0; i < 4; ++i)
    {
        const Shift s = shiftOffset(i);
        TEST_ASSERT_EQUAL(expected[i][0], s.dx);
        TEST_ASSERT_EQUAL(expected[i][1], s.dy);
    }
    // Wraps.
    TEST_ASSERT_EQUAL(1, shiftOffset(5).dx);
    TEST_ASSERT_EQUAL(0, shiftOffset(5).dy);
}

void test_inverse_text(void)
{
    Frame f(buffer);
    f.clear();
    f.fillRect(0, 0, 20, 9, true);
    TEST_ASSERT_EQUAL(180, litInRect(f, 0, 0, 20, 9));
    f.drawText(1, 1, "I", WIDTH, false);
    TEST_ASSERT_FALSE(f.pixel(3, 1)); // the stem is cut out
    TEST_ASSERT_TRUE(f.pixel(0, 0));
}

void run_frame_tests(void)
{
    RUN_TEST(test_frame_clear_and_pixels);
    RUN_TEST(test_small_text_advance_and_glyphs);
    RUN_TEST(test_large_digits);
    RUN_TEST(test_text_wider_than_line_ends_in_ellipsis);
    RUN_TEST(test_dirty_pages_only_where_changed);
    RUN_TEST(test_pixel_shift_offsets);
    RUN_TEST(test_inverse_text);
}
