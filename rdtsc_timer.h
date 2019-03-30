/**
 * A library to obtain high precision timers through
 * the serialized time stamp counter in Intel and AMD CPUs.
 * 
 * Author: Yash Gupta <yash_gupta12@live.com>
 * Copyright: This code is made public using the MIT License.
 */

// Feature set macros (Hopefully these don't clash).
#define _GNU_SOURCE

#if !defined(__amd64) && !defined(__i386)
#error "Architecture not supported"
#endif

// TODO: Get rid of:
#if !__has_include(<x86intrin.h>)
#error "x86 intrinsics not available"
#endif

#ifndef RDTSC_TIMER_H
#define RDTSC_TIMER_H

#define __cpuid(level, a, b, c, d)               \
    __asm__("cpuid\n\t"                          \
            : "=a"(a), "=b"(b), "=c"(c), "=d"(d) \
            : "0"(level))

#ifndef __KERNEL__
#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>
#else
#error "Kernel not supported yet"
#endif

enum {
    RDTSC_TIMER_READY = 0,
    RDTSC_TIMER_ERR_CPU_FREQ,
    RDTSC_TIMER_ERR_RDTSCP_SUPPORT,
    RDTSC_TIMER_ERR_MEASUREMENT,
};

// Globals.
static double __cpu_freq;
static unsigned long __instruction_overhead;
volatile static unsigned int __timer_status;
volatile static unsigned int __timer_error;

/**
 * Acquire the nominal frequency at which the
 * CPU operates.
 * 
 * Returns in Hz, or 0 for error.
 */
static unsigned long
__timer_processor_frequency()
{
#ifdef __APPLE__
    unsigned long freq;
    size_t len = sizeof(freq);
    sysctlbyname("hw.cpufrequency_max", &freq, &len, NULL, 0);
    return freq;

#elif __linux__
    // This part is dumb, but far as I know, this is the only
    // way to obtain CPU frequency in Linux since the OS does
    // not contain the 'hw' class of sysctls.

    FILE *cpu_file;
    char buf[64];
    double freq = 0.0;
    int ret;

    cpu_file = fopen("/proc/cpuinfo", "r");
    if (!cpu_file) {
        return 0;
    }

    while ((fgets(buf, 64, cpu_file)) != NULL) {
        ret = sscanf(buf, "cpu MHz : %lf", &freq);
        if (ret == 1) {
            break;
        }
    }
    fclose(cpu_file);

    // Convert from MHz to Hz.
    return (freq * 1000000);
#else
    return 0;
#endif
}

/**
 * Calculate standard deviation of an array set.
 */
static double
__timer_calculate_dev(unsigned long *set, unsigned long set_length, double mean)
{
    unsigned long i;
    double mean_diff;
    double variance, dev;

    variance = 0.0;
    for (i = 0; i < set_length; i++) {
        mean_diff = set[i] - mean;
        variance += (mean_diff * mean_diff);
    }
    variance /= set_length;

    return sqrt(variance);
}

/**
 * Calculate the margin of error on 95% confidence
 * interval.
 */
static double
__timer_calculate_error(unsigned long *set, unsigned long set_length, double mean)
{
    const double z_coefficient = 1.96;
    double dev;

    dev = __timer_calculate_dev(set, set_length, mean);
    return z_coefficient * (dev / sqrt(set_length));
}

/**
 * Calculate average of an array set.
 */
static double
__timer_calculate_mean(unsigned long *set, unsigned long set_length)
{
    unsigned long i;
    double average;

    average = 0.0;
    for (i = 0; i < set_length; i++) {
        average += set[i];
    }
    average /= set_length;

    return average;
}

/**
 * Calibrate the overhead of using the timer instruction
 * and other things.
 * 
 * TODO: Move 'timing' array to dynamic allocation to
 * allow for use on a kernel stack.
 */
static void
__timer_calibrate(void)
{
    const unsigned long repeat_factor = 1000000;
    unsigned long i;
    unsigned long start, end, timing[repeat_factor];
    unsigned int temp;
    int ci_temp[4];
    double mean;
    double error;

    // Warm CPU cache by calling instruction sequence three times.
    // 1.
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);
    start = __rdtsc();
    end = __rdtscp(&temp);
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);

    // 2.
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);
    start = __rdtsc();
    end = __rdtscp(&temp);
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);

    // 3.
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);
    start = __rdtsc();
    end = __rdtscp(&temp);
    __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);

    for (i = 0; i < repeat_factor; i++) {
        __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);
        start = __rdtsc();
        end = __rdtscp(&temp);
        __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);
        timing[i] = end - start;
    }

    mean = __timer_calculate_mean(timing, repeat_factor);
    error = __timer_calculate_error(timing, repeat_factor, mean);

    __instruction_overhead = mean;
    if (error <= (mean / 100.0)) {
        __timer_error = 1;
    } else if (error <= (mean / 50.0)) {
        __timer_error = 2;
    } else if (error <= (mean / 33.34)) {
        __timer_error = 3;
    } else {
        __timer_status = RDTSC_TIMER_ERR_MEASUREMENT;
    }
}

/**
 * Pin thread to the CPU core it is executing on.
 * CPU pinning is not supported on: MacOS.
 */
#ifdef __linux__
static cpu_set_t __default_cpu_mask;

static void
__timer_set_affinity(void)
{
    unsigned int cpu_core;
    cpu_set_t mask;

    // Determine current execution core.
    __rdtscp(&cpu_core);

    CPU_ZERO(&mask);
    CPU_SET(cpu_core, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static void
__timer_reset_affinity(void)
{
    sched_setaffinity(0, sizeof(cpu_set_t), &__default_cpu_mask);
}
#elif __APPLE__
#define __timer_set_affinity()
#define __timer_reset_affinity()
#endif

/**
 * Time the calling of a single function. This pins the
 * calling thread to the current CPU core it is executing on.
 */
#define RDTSC_TIMER_FUNCTION(func, ...)                                          \
    ({                                                                           \
        double ret;                                                              \
        unsigned long start, end;                                                \
        unsigned int temp;                                                       \
        int ci_temp[4];                                                          \
        if (__timer_status) {                                                    \
            ret = -1.0;                                                          \
        } else {                                                                 \
            __timer_set_affinity();                                              \
            __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);          \
            start = __rdtsc();                                                   \
            func(__VA_ARGS__);                                                   \
            end = __rdtscp(&temp);                                               \
            __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]);          \
            __timer_reset_affinity();                                            \
            ret = ((double)(end - start) - __instruction_overhead) / __cpu_freq; \
        }                                                                        \
        ret;                                                                     \
    })

/**
 * These macros are used to time stamp an arbitrary
 * code block. Since these can include multiple threads,
 * no threads are pinned to cores.
 * 
 * NOTE: No status checks are made, if the CPU does not support
 * the instruction, these may result in a segmentation fault.
 */
#define RDTSC_TIMER_START()                                         \
    ({                                                              \
        int ci_temp[4];                                             \
        __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]); \
        __rdtsc();                                                  \
    })

#define RDTSC_TIMER_END()                                           \
    ({                                                              \
        unsigned long ret;                                          \
        unsigned int temp;                                          \
        int ci_temp[4];                                             \
        ret = __rdtscp(&temp);                                      \
        __cpuid(0, ci_temp[0], ci_temp[1], ci_temp[2], ci_temp[3]); \
        ret;                                                        \
    })

/**
 * Use this function when timestamps are acquired through
 * 'RDTSC_TIMER_START/END' macros.
 * 
 * A value of zero means less than 1ns.
 */
static inline double
rdtsc_timer_diff(unsigned long start, unsigned long end)
{
    double sec;

    if (__timer_status) {
        return -1.0;
    }

    sec = (double)(end - start) - __instruction_overhead;
    if (sec < 0) {
        sec = 0;
    }

    return sec / __cpu_freq;
}

/**
 * Call by itself to set up the timing
 * environment.
 */
__attribute__((constructor)) static void
__timer_constructor(void)
{
    unsigned int cpu_core;
    int cpu_info[4];

    __cpu_freq = (double)__timer_processor_frequency();
    if (!__cpu_freq) {
        __timer_status = RDTSC_TIMER_ERR_CPU_FREQ;
        return;
    }

    // Determine rdtscp support.
    __cpuid(0x80000001, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
    if (!(cpu_info[3] & (1 << 27))) {
        __timer_status = RDTSC_TIMER_ERR_RDTSCP_SUPPORT;
        return;
    }

    // Calibrate overhead.
    __timer_calibrate();

    // Calculate and maintain the default CPU affinity mask.
#ifdef __linux__
    sched_getaffinity(0, sizeof(cpu_set_t), &__default_cpu_mask);
#endif

    __timer_status = RDTSC_TIMER_READY;
}

/**
 * Acquire the timer status.
 */
static inline unsigned int
rdtsc_timer_status()
{
    return __timer_status;
}

/**
 * Acquire the timer precision in ns.
 */
static inline double
rdtsc_timer_precision()
{
    return 1.0 / (__cpu_freq / 1000000000.0);
}

/**
 * Acquire the error of margin in timer
 * measurement.
 * 
 * 1: Below 1%.
 * 2: Below 2%.
 * 3: Below 3%.
 */
static inline unsigned int
rdtsc_timer_error()
{
    return __timer_error;
}

#endif /* RDTSC_TIMER_H */
