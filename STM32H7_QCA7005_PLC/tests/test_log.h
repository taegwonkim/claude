/* tests/test_log.h - log hook used by the library while testing */
#ifndef TEST_LOG_H
#define TEST_LOG_H
int test_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
