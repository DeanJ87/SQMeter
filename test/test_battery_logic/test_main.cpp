#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void run_types_tests(void);
void run_gauge_tests(void);
void run_charger_tests(void);
void run_estimate_tests(void);

int main(int, char **)
{
    UNITY_BEGIN();
    run_types_tests();
    run_gauge_tests();
    run_charger_tests();
    run_estimate_tests();
    return UNITY_END();
}
