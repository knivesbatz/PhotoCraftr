/* Native test runner: each suite is optional (weak) so modules can land independently. */
#include "testutil.h"

int g_test_failures, g_test_checks;

int test_core(void) __attribute__((weak));
int test_io(void) __attribute__((weak));
int test_quant(void) __attribute__((weak));
int test_updater(void) __attribute__((weak));

int main(void)
{
    struct { const char *name; int (*fn)(void); } suites[] = {
        { "core", test_core }, { "quant", test_quant }, { "io", test_io }, { "updater", test_updater },
    };
    unsigned i;
    for (i = 0; i < sizeof suites / sizeof suites[0]; i++) {
        int before = g_test_failures;
        if (!suites[i].fn) { printf("[skip] %s (not linked)\n", suites[i].name); continue; }
        suites[i].fn();
        printf("[%s] %s\n", g_test_failures == before ? " ok " : "FAIL", suites[i].name);
    }
    printf("%d checks, %d failures\n", g_test_checks, g_test_failures);
    return g_test_failures ? 1 : 0;
}
