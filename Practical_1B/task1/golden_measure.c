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
 * Madzibandila Charmaine Answers For Task 1
 *
 * Prediction : My CPU frequecy = 2.4GHz
 * The clock period is Tcycle = 1/2.4 * 10^(9) ​≈ 0.417 ns
 * Estimate: 40 instructions for the square-root calculation
 * Tpredicted​=40(0.417)
 * Predicted time per call: approximately 16.7 ns.
 * Reason: Just an estimate because processors can execute instructions in parallel, 
 *         and individual instructions do not necessarily correspond directly to one clock cycle.
 *
 * Why we Don't time only one call
 * Because clock_gettime() has measurement overhead and the timer resolution and 
 * measurement noise can be comparable to the function execution time.
 * Because Operating-system scheduling can introduce delays and background processes can
 * interrupt the program.
 * Repeating the function many times makes the total measurement interval larger, 
 * and dividing by the number of repetitions gives a more reliable average time per call.
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
    return (uint32_t)(sqrt((double)x));
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
    volatile uint32_t result = 0; // Volatile prevents the compiler from treating the result as unused.
    volatile uint32_t x = 987654321u;

    double start = timestamp_us();   //Stores the time at the start of the call

    for (long i = 0; i < reps; i++) {

        result = golden_isqrt(x);
    }

    double end = timestamp_us();     //Stores the time after reps calls.

    //Prevents the compiler from treating the result as unused(2^32 - 1)
    if (result == 0xFFFFFFFFu){
        printf("Unexpected result\n");
    }

    double elapsed_us = end - start;

    return (elapsed_us * 1000.0) / (double)reps;
}

int main(void)
{

    /* Golden ouput result table */
    printf("%-16s %s\n", "Input", "Golden output");
    printf("--------------------------------\n");

    for (int i = 0; i < 10; i++) {

        uint32_t result = golden_isqrt(inputs[i]);

        printf("%-16" PRIu32 " %" PRIu32 "\n", inputs[i], result);
    }

    /* Hand Verfication*/
    uint32_t check_x = 65535;
    uint32_t check_r = golden_isqrt(check_x);

    uint64_t r_squared = (uint64_t)check_r * check_r;
    uint64_t next_squared = (uint64_t)(check_r + 1) * (check_r + 1);

    printf("\nHand check\n");
    printf("--------------------------------\n");

    printf("x = %" PRIu32 "\n", check_x);
    printf("r = %" PRIu32 "\n", check_r);

    printf("%" PRIu32 "^2 = %" PRIu64 " <= %" PRIu32 "\n", check_r, r_squared, check_x);
    printf("%" PRIu32 "^2 = %" PRIu64 " > %" PRIu32 "\n", check_r + 1, next_squared, check_x);

    if(hand_check(check_x, check_r)){

        printf("Hand check: PASS\n");
    }
    else {

        printf("Hand check: FAIL\n");
    }

    /* Timing */
    long reps1 = 1000000;
    long reps2 = 10000000;

    printf("\nTiming\n");
    printf("--------------------------------\n");

    double time1 = time_n_calls(reps1);
    double time2 = time_n_calls(reps2);

    printf("Run 1 repetitions: %1d\n", reps1);
    printf("Run 1 time/call:  %.3f ns\n", time1);

    printf("\nRun 2 repetitions: %ld\n", reps2);
    printf("Run 2 time/call:   %.3f ns\n", time2);

    double mean = (time1 + time2) / 2.0;
    double spread = fabs(time1 - time2);

    printf("\nMean time/call:     %.3f ns\n", mean);
    printf("Spread:             %.3f ns\n", spread);

    return 0;

}