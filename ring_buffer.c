/**
 * ring_buffer.c
 *
 * Power-of-2 capacity allows head/tail to grow monotonically and wrap
 * naturally via bitmask.  This avoids the branch in a modulo-based wrap
 * and is safe against the ABA problem when head/tail are 32-bit counters
 * (they will take ~4 billion writes to overflow on a 64-byte buffer).
 *
 * On an actual STM32F4 the rb_write ISR path would disable no interrupts —
 * the single-producer / single-consumer contract makes it lock-free.
 * rb_read disables interrupts only long enough to snapshot head and tail
 * as a consistent pair.
 */

#include "ring_buffer.h"
#include <string.h>

/* On real hardware these macros would map to PRIMASK manipulation.
 * In the host simulation build they are no-ops. */
#ifndef DISABLE_IRQS
  #define DISABLE_IRQS()   ((void)0)
  #define ENABLE_IRQS()    ((void)0)
#endif

#define RB_MASK (RB_CAPACITY - 1U)

void rb_init(RingBuffer *rb)
{
    memset((void *)rb->buf, 0, sizeof(rb->buf));
    rb->head = 0U;
    rb->tail = 0U;
}

bool rb_write(RingBuffer *rb, uint8_t byte)
{
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;

    if ((head - tail) >= RB_CAPACITY) {
        /* Buffer full — caller must handle dropped byte */
        return false;
    }

    rb->buf[head & RB_MASK] = byte;

    /* Memory barrier: ensure buf write completes before head advances.
     * On ARM this would be __DMB(). Omitted in simulation build. */
    rb->head = head + 1U;
    return true;
}

bool rb_read(RingBuffer *rb, uint8_t *out)
{
    DISABLE_IRQS();
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    ENABLE_IRQS();

    if (head == tail) {
        return false;   /* Empty */
    }

    *out = rb->buf[tail & RB_MASK];
    rb->tail = tail + 1U;
    return true;
}

uint32_t rb_available(const RingBuffer *rb)
{
    return rb->head - rb->tail;
}

bool rb_is_full(const RingBuffer *rb)
{
    return (rb->head - rb->tail) >= RB_CAPACITY;
}
