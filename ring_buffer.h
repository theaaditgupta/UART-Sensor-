/**
 * ring_buffer.h
 * Fixed-size circular buffer for UART sensor data.
 *
 * Thread-safety note: rb_write() is safe to call from an ISR.
 * rb_read() must only be called from the main loop (not from ISR context).
 * Access to shared state is protected by disabling interrupts around
 * multi-byte reads.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RB_CAPACITY 64U   /* Must be a power of 2 */

/**
 * Compile-time assertion: capacity must be power of 2 so wrap
 * can be done with a bitmask instead of a modulo operation.
 */
_Static_assert((RB_CAPACITY & (RB_CAPACITY - 1U)) == 0U,
               "RB_CAPACITY must be a power of 2");

typedef struct {
    volatile uint8_t  buf[RB_CAPACITY];
    volatile uint32_t head;   /* Next write position (ISR writes here) */
    volatile uint32_t tail;   /* Next read position  (main loop reads here) */
} RingBuffer;

/**
 * rb_init()
 * Initialise all fields to zero. Call once before use.
 */
void rb_init(RingBuffer *rb);

/**
 * rb_write()
 * Write one byte. Safe to call from ISR.
 * Returns true on success, false if buffer is full (byte dropped).
 */
bool rb_write(RingBuffer *rb, uint8_t byte);

/**
 * rb_read()
 * Read one byte into *out. Call from main loop only.
 * Returns true on success, false if buffer is empty.
 */
bool rb_read(RingBuffer *rb, uint8_t *out);

/**
 * rb_available()
 * Number of bytes currently in the buffer.
 */
uint32_t rb_available(const RingBuffer *rb);

/**
 * rb_is_full()
 */
bool rb_is_full(const RingBuffer *rb);

#endif /* RING_BUFFER_H */
