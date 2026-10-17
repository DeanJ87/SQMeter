#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void run_display_types_tests(void);

int main(int, char **)
{
    UNITY_BEGIN();
    run_display_types_tests();
    return UNITY_END();
}
