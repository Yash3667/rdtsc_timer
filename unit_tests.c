/**
 * Author: Yash Gupta <yash_gupta12@live.com>
 */
#include "rdtsc_timer.h"
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
    unsigned long start, end;

    if (__timer_status) {
        fprintf(stderr, "Timer not ready: %d\n", __timer_status);
        return -1;
    }

    printf("(Function)  Sleep [1us]: %.9lf seconds\n", TIME_FUNCTION(usleep, 1));
    printf("(Function)  Sleep [1ms]: %.9lf seconds\n", TIME_FUNCTION(usleep, 1000));
    printf("(Function)  Sleep [1s] : %.9lf seconds\n\n", TIME_FUNCTION(usleep, 1000000));

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