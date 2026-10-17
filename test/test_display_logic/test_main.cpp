#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void run_display_types_tests(void);
void run_frame_tests(void);
void run_link_tests(void);

int main(int, char **)
{
    UNITY_BEGIN();
    run_display_types_tests();
    run_frame_tests();
    run_link_tests();
    return UNITY_END();
}
