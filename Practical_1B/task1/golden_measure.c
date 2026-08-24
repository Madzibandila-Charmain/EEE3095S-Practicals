/*
 * Task 1: The Golden Measure on the PC
 *
 * Integer square root of a 32-bit unsigned input x: the largest integer
 * whose square does not exceed x.  Golden version uses double precision
 * arithmetic and the standard library square root.
 *
 * Prediction (written before running):
 *   On a ~3 GHz x86-64 CPU, sqrt + floor + conversion is roughly
 *   20..40 instructions.  Estimate: 40 instructions * 0.33 ns/instr
 *   ~= 13 ns per call.  One call alone cannot be timed reliably because
 *   clock_gettime resolution is ~10..50 ns and scheduling noise is larger
 *   than the call itself, so we loop and divide.
 *
 * Build:   gcc -O2 -o golden_measure golden_measure.c -lm
 * Run:     ./golden_measure
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>

static const uint32_t inputs[10] = {
    0, 1, 15, 16, 4095, 65535,
    123456789, 987654321, 4294836225u, 4294967295u
};

static uint32_t golden_isqrt(uint32_t x)
{
    return (uint32_t)floor(sqrt((double)x));
}

/* Returns current time in microseconds */
static double timestamp_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;

}

/* Hand check: r^2 <= x < (r+1)^2, written out in full. */
static int hand_check(uint32_t x, uint32_t r)
{
    uint64_t r_squared = (uint64_t)r * r;
    uint64_t next_squared = (uint64_t)(r + 1) * (r + 1);

    return (r_squared <= x) && (x < next_squared);
}

/*Times all calls and returns average time per call in ns */
static double time_n_calls(long reps)
{
    volatile uint32_t result = 0; //Volatile to protect the variable from the CPU

    uint32_t x = 987654321u;

    double start = timestamp_us();   //Stores the time at the start of the call

    for (long i = 0; i < reps; i++) {
        result = golden_isqrt(x);
    }

    double end = timestamp_us();     //Stores the time after reps calls.

    //Prevents the compiler from treating the result as unused(2^32 - 1)
    if (result == 0xFFFFFFFFu) {
        print("Unexpected result\n");
    }

    double elapsed_us = end - start;

    return (elapsed_us * 1000.0) / (double)reps;
}

int main(void)
{

}