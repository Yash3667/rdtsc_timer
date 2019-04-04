/**
 * A library to obtain high precision timers through
 * the time stamp counter in Intel and AMD CPUs.
 * 
 * Author: Yash Gupta <yash_gupta12@live.com>
 * Copyright: This code is made public using the MIT License.
 */

// Feature set macros (Hopefully these don't clash).
#define _GNU_SOURCE

#if !defined(__amd64) && !defined(__i386)
#error "Architecture not supported"
#endif

#if !__has_include(<x86intrin.h>)
#error "x86 intrinsics not available"
#endif

#ifndef RDTSC_TIMER_H
#define RDTSC_TIMER_H

#if __has_include(<cpuid.h>)
#include <cpuid.h>
#else
#define __cpuid(level, a, b, c, d)               \
    __asm__("cpuid\n\t"                          \
            : "=a"(a), "=b"(b), "=c"(c), "=d"(d) \
            : "0"(level))
#endif

#ifndef __KERNEL__
#include <fcntl.h>
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

typedef enum timer_status_bits {
    TIMER_READY = 0,
    TIMER_ERR_CPU_FREQ,
    TIMER_ERR_RDTSCP_SUPPORT,
} timer_status_bits;

/**
 * Acquire the nominal frequency at which the
 * CPU operates.
 * 
 * Returns in MHz, or -1 for error.
 */
static unsigned long
__timer_processor_frequency()
{
    int cpu_info[4];

    __cpuid(0, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
    if (cpu_info[0] >= 0x16) {
        __cpuid(0x16, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
        return cpu_info[1];
    }

    // Only newer processors support the 0x16 function of
    // 'cpuid'. If running on an older processor, OS help is
    // required to figure out the nominal CPU frequency.

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

// Globals.
static int __rdtscp_support;
static double __cpu_freq;
static unsigned long __instruction_overhead;
static unsigned long __other_overhead;
static timer_status_bits __timer_status;

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
        __timer_status = TIMER_ERR_CPU_FREQ;
        return;
    }

    // Determine rdtscp support.
    __cpuid(0x80000001, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
    if (cpu_info[3] & (1 << 27)) {
        __rdtscp_support = 1;
    } else {
        // TODO: Support cpuid + rdtsc.
        __timer_status = TIMER_ERR_RDTSCP_SUPPORT;
        return;
    }

    // TODO: Calibrate overhead.
    //  -> Calculate cost of calling rdtscp (__instruction_overhead).
    //  -> If rdtscp support is not present, then calculate cost of using cpuid for serialization
    //     (__instruction_overhead).
    //  -> When using 'TIME_STAMP', overhead for making an 'if' comparison and setting a variable
    //     value need to be recorded in '__other_overhead'.

    // Calculate and maintain the default CPU affinity mask.
#ifdef __linux__
    sched_getaffinity(0, sizeof(cpu_set_t), &__default_cpu_mask);
#endif

    __timer_status = TIMER_READY;
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
#define TIME_FUNCTION(func, ...)                                                     \
    ({                                                                               \
        double ret;                                                                  \
        unsigned long start, end;                                                    \
        unsigned int temp;                                                           \
        if (__timer_status) {                                                        \
            ret = -1.0;                                                              \
        } else {                                                                     \
            if (__rdtscp_support) {                                                  \
                __timer_set_affinity();                                              \
                start = __rdtscp(&temp);                                             \
                func(__VA_ARGS__);                                                   \
                end = __rdtscp(&temp);                                               \
                __timer_reset_affinity();                                            \
                ret = ((double)(end - start) - __instruction_overhead) / __cpu_freq; \
            } else {                                                                 \
                ret = -1.0;                                                          \
            }                                                                        \
        }                                                                            \
        ret;                                                                         \
    })

/**
 * This macro is used when you may want to time an arbitrary
 * code block. Since this can include multiple threads, no 
 * threads are pinned to a single core.
 */
#define TIME_STAMP()               \
    ({                             \
        unsigned int temp;         \
        unsigned long ret;         \
        if (__rdtscp_support) {    \
            ret = __rdtscp(&temp); \
        } else {                   \
            ret = 0.0;             \
        }                          \
        ret;                       \
    })

/**
 * Use this function when timestamps are acquired through
 * 'TIME_STAMP' macro.
 */
static inline double
rdtsc_timer_diff(unsigned long start, unsigned long end)
{
    double sec;

    if (__timer_status) {
        return -1.0;
    }

    sec = (double)(end - start) - (__instruction_overhead + __other_overhead);
    return sec / __cpu_freq;
}

#endif /* RDTSC_TIMER_H */