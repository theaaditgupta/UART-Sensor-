/**
 * main.c — UART Sensor Node simulation
 *
 * Simulates the full firmware loop:
 *   1. TIM2 fires at 1 Hz  ->  MCU wakes from STOP mode
 *   2. UART ISR fills ring buffer with one sensor frame (6 bytes)
 *   3. Main loop drains ring buffer
 *   4. Every FLUSH_INTERVAL cycles, flush accumulated data to flash
 *   5. Record active + sleep durations for power budget calculation
 *   6. After NUM_CYCLES, print power report and flash wear stats
 *
 * Build (host simulation):
 *   make            (see Makefile)
 *
 * Expected output (1000 cycles, 1 Hz, 5ms active window):
 *   Avg current ~178 uA, battery life ~1.9 years on AA (3000 mAh).
 *   Real STM32F4 STOP current is lower (~100 uA vs 100 uA simulated) but
 *   the active window dominates because RUN_UA is 35,000 uA vs STOP 100 uA.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ring_buffer.h"
#include "flash_store.h"
#include "power.h"

/* ── Simulation parameters ──────────────────────────────────── */
#define NUM_CYCLES        1000U      /* Total 1 Hz sample cycles to simulate  */
#define FLUSH_INTERVAL    60U        /* Flush ring buffer to flash every N cycles */
#define ACTIVE_WINDOW_US  5000U      /* MCU active time per cycle (microseconds) */
#define SLEEP_WINDOW_US   995000U    /* MCU sleep time per cycle (1s - 5ms)    */
#define SENSOR_FRAME_LEN  6U         /* Bytes per sensor reading over UART     */
#define BATTERY_MAH       3000U      /* AA battery capacity                    */

/* ── Simulated UART ISR ──────────────────────────────────────── */
static void simulate_uart_isr(RingBuffer *rb, uint32_t cycle)
{
    /*
     * In real firmware the UART DR register is read here on RX interrupt.
     * We simulate a 6-byte frame:  [0xAA][temp_hi][temp_lo][hum][pressure][checksum]
     */
    uint8_t frame[SENSOR_FRAME_LEN];
    frame[0] = 0xAAU;                          /* Start byte           */
    frame[1] = (uint8_t)(20U + (cycle % 10U)); /* Temperature high byte */
    frame[2] = (uint8_t)(cycle % 100U);        /* Temperature low byte  */
    frame[3] = (uint8_t)(50U + (cycle % 20U)); /* Humidity              */
    frame[4] = (uint8_t)(100U + (cycle % 5U)); /* Pressure              */
    frame[5] = (uint8_t)(frame[1] ^ frame[2] ^ frame[3] ^ frame[4]); /* XOR checksum */

    for (uint8_t i = 0U; i < SENSOR_FRAME_LEN; i++) {
        if (!rb_write(rb, frame[i])) {
            printf("[WARN] cycle %u: ring buffer full, byte %u dropped\n", cycle, i);
        }
    }
}

/* ── Main loop ───────────────────────────────────────────────── */
int main(void)
{
    RingBuffer  rb;
    FlashStore  fs;
    PowerStats  ps;

    rb_init(&rb);
    flash_init(&fs);
    power_init(&ps);

    printf("UART Sensor Node — Firmware Simulation\n");
    printf("Cycles: %u  |  Sample rate: 1 Hz  |  Active window: %u us\n\n",
           NUM_CYCLES, ACTIVE_WINDOW_US);

    uint8_t  flush_buf[SENSOR_FRAME_LEN * FLUSH_INTERVAL];
    uint32_t flush_pos  = 0U;
    uint32_t drop_count = 0U;

    for (uint32_t cycle = 0U; cycle < NUM_CYCLES; cycle++) {

        /* ── Active window: MCU running ── */
        power_record_active(&ps, ACTIVE_WINDOW_US);

        /* Simulate UART ISR filling ring buffer */
        simulate_uart_isr(&rb, cycle);

        /* Main loop drains ring buffer into flush_buf */
        uint8_t byte;
        while (rb_read(&rb, &byte)) {
            if (flush_pos < sizeof(flush_buf)) {
                flush_buf[flush_pos++] = byte;
            } else {
                drop_count++;
            }
        }

        /* Flush to flash every FLUSH_INTERVAL cycles */
        if ((cycle + 1U) % FLUSH_INTERVAL == 0U && flush_pos > 0U) {
            if (!flash_write(&fs, flush_buf, flush_pos)) {
                printf("[ERROR] cycle %u: flash write failed (all sectors full)\n", cycle);
            }
            flush_pos = 0U;
        }

        /* ── Sleep window: MCU in STOP mode ── */
        power_record_sleep(&ps, SLEEP_WINDOW_US);

        /* Progress every 100 cycles */
        if ((cycle + 1U) % 100U == 0U) {
            uint32_t avail = rb_available(&rb);
            printf("  [cycle %4u]  rb_available=%u  flash_sector=%u  write_count=%u\n",
                   cycle + 1U,
                   avail,
                   fs.active_sector,
                   fs.write_counts[fs.active_sector]);
        }
    }

    /* ── Reports ── */
    if (drop_count > 0U) {
        printf("\n[WARN] %u bytes dropped from flush_buf overflow\n", drop_count);
    }

    power_print_report(&ps, BATTERY_MAH);

    /* Flash wear distribution */
    uint32_t stats[FLASH_NUM_SECTORS];
    flash_get_stats(&fs, stats);
    printf("=== Flash Wear Distribution ===\n");
    for (uint8_t i = 0U; i < FLASH_NUM_SECTORS; i++) {
        printf("  Sector %u : %u writes\n", i, stats[i]);
    }
    printf("===============================\n");

    return 0;
}
