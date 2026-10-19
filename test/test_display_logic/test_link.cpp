#include <unity.h>

#include "DisplayController.h"

using namespace SQM::Display;

void test_link_probes_only_while_enabled(void)
{
    Link link;
    TEST_ASSERT_FALSE(link.shouldProbe(false, 0));
    TEST_ASSERT_FALSE(link.shouldProbe(false, 100000));
    TEST_ASSERT_FALSE(link.detected());
    TEST_ASSERT_FALSE(link.needsInit());
    // Enabled: probe at once.
    TEST_ASSERT_TRUE(link.shouldProbe(true, 1000));
}

void test_link_reprobes_every_five_seconds(void)
{
    Link link;
    TEST_ASSERT_TRUE(link.shouldProbe(true, 1000));
    link.probeResult(false, 1000);
    TEST_ASSERT_FALSE(link.detected());
    TEST_ASSERT_FALSE(link.shouldProbe(true, 1001));
    TEST_ASSERT_FALSE(link.shouldProbe(true, 5999));
    TEST_ASSERT_TRUE(link.shouldProbe(true, 6000));
    link.probeResult(true, 6000);
    TEST_ASSERT_TRUE(link.detected());
    TEST_ASSERT_TRUE(link.needsInit());
    // Detected: no more probes.
    TEST_ASSERT_FALSE(link.shouldProbe(true, 20000));
    link.initDone();
    TEST_ASSERT_FALSE(link.needsInit());
}

void test_link_three_write_failures_lose_the_display(void)
{
    Link link;
    link.probeResult(true, 0);
    link.initDone();
    link.writeResult(false, 100);
    link.writeResult(false, 200);
    TEST_ASSERT_TRUE(link.detected());
    link.writeResult(true, 300); // a good write resets the count
    link.writeResult(false, 400);
    link.writeResult(false, 500);
    TEST_ASSERT_TRUE(link.detected());
    link.writeResult(false, 600);
    TEST_ASSERT_FALSE(link.detected());
    TEST_ASSERT_FALSE(link.needsInit());
    // Probing resumes five seconds later; re-detection needs the init again.
    TEST_ASSERT_FALSE(link.shouldProbe(true, 5599));
    TEST_ASSERT_TRUE(link.shouldProbe(true, 5600));
    link.probeResult(true, 5600);
    TEST_ASSERT_TRUE(link.detected());
    TEST_ASSERT_TRUE(link.needsInit());
}

void test_link_disable_forgets_the_display(void)
{
    Link link;
    link.probeResult(true, 0);
    TEST_ASSERT_TRUE(link.detected());
    link.disable();
    TEST_ASSERT_FALSE(link.detected());
    TEST_ASSERT_FALSE(link.needsInit());
    TEST_ASSERT_FALSE(link.shouldProbe(false, 10));
    // Enabled again: probe at once.
    TEST_ASSERT_TRUE(link.shouldProbe(true, 20));
}

void test_link_millis_wrap(void)
{
    Link link;
    link.probeResult(false, 0xFFFFF000U);
    TEST_ASSERT_FALSE(link.shouldProbe(true, 0x00000010U));
    TEST_ASSERT_TRUE(link.shouldProbe(true, 0x00000FA0U)); // 5,024 ms later
}

void test_button_debounce_one_press_per_press(void)
{
    Button button;
    // Released (pulled up = high).
    TEST_ASSERT_FALSE(button.update(true, 0));
    TEST_ASSERT_FALSE(button.update(true, 10));
    // Bounce: low/high/low inside 30 ms gives nothing.
    TEST_ASSERT_FALSE(button.update(false, 20));
    TEST_ASSERT_FALSE(button.update(true, 25));
    TEST_ASSERT_FALSE(button.update(false, 30));
    TEST_ASSERT_FALSE(button.update(false, 50));
    // Low for 30 ms: one press.
    TEST_ASSERT_TRUE(button.update(false, 60));
    TEST_ASSERT_FALSE(button.update(false, 70));
    TEST_ASSERT_FALSE(button.update(false, 500));
    // Release, then a second press.
    TEST_ASSERT_FALSE(button.update(true, 510));
    TEST_ASSERT_FALSE(button.update(true, 545));
    TEST_ASSERT_FALSE(button.update(false, 600));
    TEST_ASSERT_TRUE(button.update(false, 630));
}

void test_button_held_at_boot_is_not_a_press(void)
{
    Button button;
    TEST_ASSERT_FALSE(button.update(false, 0));
    TEST_ASSERT_FALSE(button.update(false, 100));
    TEST_ASSERT_FALSE(button.update(true, 200));
    TEST_ASSERT_FALSE(button.update(true, 240));
    TEST_ASSERT_FALSE(button.update(false, 300));
    TEST_ASSERT_TRUE(button.update(false, 330));
}

void run_link_tests(void)
{
    RUN_TEST(test_link_probes_only_while_enabled);
    RUN_TEST(test_link_reprobes_every_five_seconds);
    RUN_TEST(test_link_three_write_failures_lose_the_display);
    RUN_TEST(test_link_disable_forgets_the_display);
    RUN_TEST(test_link_millis_wrap);
    RUN_TEST(test_button_debounce_one_press_per_press);
    RUN_TEST(test_button_held_at_boot_is_not_a_press);
}
