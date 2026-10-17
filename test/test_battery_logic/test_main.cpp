#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void run_types_tests(void);

int main(int, char **)
{
    UNITY_BEGIN();
    run_types_tests();
    return UNITY_END();
}
