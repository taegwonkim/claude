/* tests/test_main.c - host unit tests for the PLC library (no hardware) */
#include <stdarg.h>
#include <string.h>
#include "test_common.h"

int g_verbose;
int g_failures;

void test_frame_crypto(void);
void test_qca7k_spi(void);
void test_slac_sim(void);

int test_log(const char *fmt, ...)
{
    va_list ap;
    int r = 0;
    if (!g_verbose)
        return 0;
    va_start(ap, fmt);
    printf("    ");
    r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "-v"))
        g_verbose = 1;

    test_frame_crypto();
    test_qca7k_spi();
    test_slac_sim();

    if (g_failures) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall tests passed\n");
    return 0;
}
