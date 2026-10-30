/* Minimal test harness shared by the tests/ suites (native build only). */
#ifndef TESTUTIL_H
#define TESTUTIL_H
#include <stdio.h>
extern int g_test_failures, g_test_checks;
#define CHECK(cond) do { g_test_checks++; if (!(cond)) { g_test_failures++; \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); g_test_checks++; if (_a != _b) { g_test_failures++; \
    fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%ld vs %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#define CHECK_NEAR(a, b, tol) do { long _a = (long)(a), _b = (long)(b); g_test_checks++; if (_a - _b > (tol) || _b - _a > (tol)) { \
    g_test_failures++; fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s ~ %s (%ld vs %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
#endif
