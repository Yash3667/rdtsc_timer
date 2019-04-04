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

int
main(void)
{
    unsigned long start, end;
    struct timespec timespec_stamp;

    if (__timer_status) {
        fprintf(stderr, "Timer not ready: %d\n", __timer_status);
        return -1;
    }

    // clock_gettime.
    INIT_TIME(&timespec_stamp);
    usleep(1);
    printf("(get_time)  Sleep [1us]: %.9lf seconds\n", GET_TIME(timespec_stamp));

    INIT_TIME(&timespec_stamp);
    usleep(1000);
    printf("(get_time)  Sleep [1ms]: %.9lf seconds\n", GET_TIME(timespec_stamp));

    INIT_TIME(&timespec_stamp);
    usleep(1000000);
    printf("(get_time)  Sleep [1s] : %.9lf seconds\n\n", GET_TIME(timespec_stamp));

    // rdtsc_timer TIME_FUNCTION.
    printf("(Function)  Sleep [1us]: %.9lf seconds\n", TIME_FUNCTION(usleep, 1));
    printf("(Function)  Sleep [1ms]: %.9lf seconds\n", TIME_FUNCTION(usleep, 1000));
    printf("(Function)  Sleep [1s] : %.9lf seconds\n\n", TIME_FUNCTION(usleep, 1000000));

    // rdtsc_timer TIME_STAMP.
    start = TIME_STAMP();
    usleep(1);
    end = TIME_STAMP();
    printf("(Timestamp) Sleep [1us]: %.9lf seconds\n", rdtsc_timer_diff(start, end));

    start = TIME_STAMP();
    usleep(1000);
    end = TIME_STAMP();
    printf("(Timestamp) Sleep [1ms]: %.9lf seconds\n", rdtsc_timer_diff(start, end));

    start = TIME_STAMP();
    usleep(1000000);
    end = TIME_STAMP();
    printf("(Timestamp) Sleep [1s] : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    return 0;
}