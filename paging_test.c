/**
 * Cache line size on my CPU is 64 bytes.
 * 
 * Author: Yash Gupta <yash_gupta12@live.com>
 */
#include "rdtsc_timer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Page size.
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// Pages to allocate for testing.
#define NUM_PAGES 3

// 16MB.
#define CACHE_SIZE_BYTES (4 * (PAGE_SIZE * 1024))
#define CACHE_SIZE_WORDS (CACHE_SIZE_BYTES / 4)

/**
 * Fill all three levels of caches with trash.
 */
static inline void
trash_cache()
{
    int32_t *mem;
    int i;
    volatile int val;

    mem = malloc(CACHE_SIZE_BYTES);
    assert(mem);

    srand(time(0));
    for (i = 0; i < CACHE_SIZE_WORDS; i++) {
        mem[i] = rand();
    }

    // 'val' is volatile, so the previous writes won't
    // be optimized out.
    for (i = 0; i < CACHE_SIZE_WORDS; i++) {
        val = mem[i];
    }

    free(mem);
}

int
main(void)
{
    void *mem;
    uint64_t *ptr;
    volatile uint64_t val;
    unsigned long start, end;
    int ret;

    if (rdtsc_timer_status()) {
        fprintf(stderr, "Timer error: %d\n", rdtsc_timer_status());
        return -1;
    }

    printf("Timer Precision: %lf ns\n", rdtsc_timer_precision());
    printf("Timer Error: Below %u%%\n", rdtsc_timer_error());
    printf("Legend: (Not)Aligned, (Not)Cached, (N)PageFault\n\n");

    ret = posix_memalign(&mem, PAGE_SIZE, NUM_PAGES * PAGE_SIZE);
    assert(!ret);
    val = 0;

    // Aligned, not cached, potential page fault.
    ptr = mem + PAGE_SIZE;
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[A, NC, F]  : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    // Aligned, cached, no page fault.
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[A, C, NF]  : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    // Aligned, not cached, no page fault.
    trash_cache();
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[A, NC, NF] : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    // Not aligned, not cached, page fault.
    ptr = mem + PAGE_SIZE + (PAGE_SIZE - 4);
    trash_cache();
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[NA, NC, F] : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    // Not aligned, cached, no page fault.
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[NA, C, NF] : %.9lf seconds\n", rdtsc_timer_diff(start, end));

    // Not aligned, not cached, no page fault.
    trash_cache();
    start = RDTSC_TIMER_START();
    val = *ptr;
    end = RDTSC_TIMER_END();
    printf("[NA, NC, NF]: %.9lf seconds\n", rdtsc_timer_diff(start, end));

    free(mem);
    return 0;
}