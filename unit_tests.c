/**
 * Author: Yash Gupta <yash_gupta12@live.com>
 */
#include "rdtsc_timer.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define INIT_TIME(x) _initTime(x)
#define GET_TIME(x) _getTime(x)

static inline void
_initTime(struct timespec *start)
{
    clock_gettime(CLOCK_MONOTONIC_RAW, start);
}

static inline double
_getTime(struct timespec start)
{
    double time_passed;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    time_passed = (int64_t)1000000000L * (int64_t)(now.tv_sec - start.tv_sec);
    time_passed += (int64_t)(now.tv_nsec - start.tv_nsec);

    return time_passed / 1000000000.0;
}

static void
sum_test(long n)
{
    long i;
    for (i = 0; i < n; i++) {
    }
}

int
main(void)
{
    unsigned long start, end;
    struct timespec timespec_stamp;

    if (__timer_status) {
        fprintf(stderr, "Timer not ready: %d\n", __timer_status);
        return -1;
    }

    printf("Timer Precision: %lf ns\n", rdtsc_timer_precision());
    printf("Timer Error: Below %u%%\n\n", rdtsc_timer_error());

    // Warm cache and CPU.
    sum_test(1000);

    // rdtsc_timer RDTSC_TIMER_FUNCTION.
    printf("(rdtsc_timer_function) [1] : %.9lf seconds\n", RDTSC_TIMER_FUNCTION(sum_test, 1));
    printf("(rdtsc_timer_function) [1K]: %.9lf seconds\n", RDTSC_TIMER_FUNCTION(sum_test, 1 << 10));
    printf("(rdtsc_timer_function) [1M]: %.9lf seconds\n", RDTSC_TIMER_FUNCTION(sum_test, 1 << 20));
    printf("(rdtsc_timer_function) [1B]: %.9lf seconds\n\n", RDTSC_TIMER_FUNCTION(sum_test, 1 << 30));

    // rdtsc_timer RDTSC_TIMER_START.
    start = RDTSC_TIMER_START();
    sum_test(1);
    end = RDTSC_TIMER_END();
    printf("(rdtsc_timer_stamp) [1] : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    start = RDTSC_TIMER_START();
    sum_test(1 << 10);
    end = RDTSC_TIMER_END();
    printf("(rdtsc_timer_stamp) [1K]: %.9lf seconds\n", rdtsc_timer_diff(start, end));

    start = RDTSC_TIMER_START();
    sum_test(1 << 20);
    end = RDTSC_TIMER_END();
    printf("(rdtsc_timer_stamp) [1M]: %.9lf seconds\n", rdtsc_timer_diff(start, end));

    start = RDTSC_TIMER_START();
    sum_test(1 << 30);
    end = RDTSC_TIMER_END();
    printf("(rdtsc_timer_stamp) [1B]: %.9lf seconds\n\n", rdtsc_timer_diff(start, end));

    // clock_gettime.
    INIT_TIME(&timespec_stamp);
    sum_test(1);
    printf("(get_time) [1] : %.9lf seconds\n", GET_TIME(timespec_stamp));

    INIT_TIME(&timespec_stamp);
    sum_test(1 << 10);
    printf("(get_time) [1K]: %.9lf seconds\n", GET_TIME(timespec_stamp));

    INIT_TIME(&timespec_stamp);
    sum_test(1 << 20);
    printf("(get_time) [1M]: %.9lf seconds\n", GET_TIME(timespec_stamp));

    INIT_TIME(&timespec_stamp);
    sum_test(1 << 30);
    printf("(get_time) [1B]: %.9lf seconds\n", GET_TIME(timespec_stamp));

    return 0;
}