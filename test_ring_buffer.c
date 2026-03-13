/**
 * test_ring_buffer.c
 *
 * Unit tests for the ring buffer.  Covers:
 *   - Basic write/read roundtrip
 *   - Wrap-around (the off-by-one bugs that were found in development)
 *   - Full-buffer rejection
 *   - Empty-buffer read returning false
 *   - Available count tracking
 *   - Sequential fill-drain cycles (simulates multi-period operation)
 *
 * Build and run via: make test
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "ring_buffer.h"

static uint32_t tests_run    = 0U;
static uint32_t tests_passed = 0U;

#define ASSERT_TRUE(expr) do { \
    tests_run++; \
    if (!(expr)) { \
        printf("  FAIL [%s:%d]  %s\n", __FILE__, __LINE__, #expr); \
    } else { \
        tests_passed++; \
    } \
} while (0)

/* ── Test: basic write and read ── */
static void test_basic_roundtrip(void)
{
    printf("test_basic_roundtrip\n");
    RingBuffer rb;
    rb_init(&rb);

    ASSERT_TRUE(rb_write(&rb, 0x42U) == true);
    uint8_t out = 0U;
    ASSERT_TRUE(rb_read(&rb, &out) == true);
    ASSERT_TRUE(out == 0x42U);
}

/* ── Test: empty buffer returns false ── */
static void test_empty_read(void)
{
    printf("test_empty_read\n");
    RingBuffer rb;
    rb_init(&rb);

    uint8_t out = 0U;
    ASSERT_TRUE(rb_read(&rb, &out) == false);
}

/* ── Test: fill to capacity ── */
static void test_fill_to_capacity(void)
{
    printf("test_fill_to_capacity\n");
    RingBuffer rb;
    rb_init(&rb);

    for (uint32_t i = 0U; i < RB_CAPACITY; i++) {
        ASSERT_TRUE(rb_write(&rb, (uint8_t)(i & 0xFFU)) == true);
    }
    ASSERT_TRUE(rb_is_full(&rb) == true);
    /* One more write should be rejected */
    ASSERT_TRUE(rb_write(&rb, 0xFFU) == false);
}

/* ── Test: wrap-around correctness ──
 * This is the class of bug found during development.
 * Write 64 bytes, read 32, write 32 more — the second write set wraps
 * around the end of the buffer.  All bytes must come out in order.
 */
static void test_wrap_around(void)
{
    printf("test_wrap_around\n");
    RingBuffer rb;
    rb_init(&rb);

    /* Fill buffer */
    for (uint8_t i = 0U; i < RB_CAPACITY; i++) {
        rb_write(&rb, i);
    }

    /* Drain half */
    for (uint8_t i = 0U; i < RB_CAPACITY / 2U; i++) {
        uint8_t out;
        rb_read(&rb, &out);
        ASSERT_TRUE(out == i);
    }

    /* Write another half — these wrap around the physical array end */
    for (uint8_t i = 0U; i < RB_CAPACITY / 2U; i++) {
        ASSERT_TRUE(rb_write(&rb, (uint8_t)(0x80U + i)) == true);
    }

    /* Read the original second half */
    for (uint8_t i = RB_CAPACITY / 2U; i < RB_CAPACITY; i++) {
        uint8_t out;
        ASSERT_TRUE(rb_read(&rb, &out) == true);
        ASSERT_TRUE(out == i);
    }

    /* Read the wrapped-around second batch */
    for (uint8_t i = 0U; i < RB_CAPACITY / 2U; i++) {
        uint8_t out;
        ASSERT_TRUE(rb_read(&rb, &out) == true);
        ASSERT_TRUE(out == (uint8_t)(0x80U + i));
    }

    /* Buffer should now be empty */
    uint8_t dummy;
    ASSERT_TRUE(rb_read(&rb, &dummy) == false);
}

/* ── Test: available count ── */
static void test_available_count(void)
{
    printf("test_available_count\n");
    RingBuffer rb;
    rb_init(&rb);

    ASSERT_TRUE(rb_available(&rb) == 0U);
    rb_write(&rb, 1U);
    rb_write(&rb, 2U);
    rb_write(&rb, 3U);
    ASSERT_TRUE(rb_available(&rb) == 3U);

    uint8_t out;
    rb_read(&rb, &out);
    ASSERT_TRUE(rb_available(&rb) == 2U);
}

/* ── Test: multiple fill-drain cycles ──
 * Simulates 500 sensor frames of 6 bytes each being written and drained.
 * Verifies no data corruption across many cycles — catches the monotonic
 * head/tail overflow edge case if it exists.
 */
static void test_multi_cycle(void)
{
    printf("test_multi_cycle\n");
    RingBuffer rb;
    rb_init(&rb);

    uint32_t write_seq = 0U;
    uint32_t read_seq  = 0U;

    for (uint32_t cycle = 0U; cycle < 500U; cycle++) {
        /* Write 6-byte frame */
        for (uint8_t b = 0U; b < 6U; b++) {
            uint8_t val = (uint8_t)((write_seq + b) & 0xFFU);
            ASSERT_TRUE(rb_write(&rb, val) == true);
        }
        write_seq += 6U;

        /* Read it back immediately */
        for (uint8_t b = 0U; b < 6U; b++) {
            uint8_t out;
            ASSERT_TRUE(rb_read(&rb, &out) == true);
            ASSERT_TRUE(out == (uint8_t)((read_seq + b) & 0xFFU));
        }
        read_seq += 6U;
    }
}

int main(void)
{
    printf("\n=== Ring Buffer Tests ===\n\n");

    test_basic_roundtrip();
    test_empty_read();
    test_fill_to_capacity();
    test_wrap_around();
    test_available_count();
    test_multi_cycle();

    printf("\n%u/%u tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("ALL TESTS PASSED\n\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED\n\n");
        return 1;
    }
}
