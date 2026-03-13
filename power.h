/**
 * power.h
 * Simulates STM32F4 STOP mode power management.
 *
 * On real hardware:
 *   - STOP mode is entered by setting SLEEPDEEP in SCB->SCR and executing WFI.
 *   - Wakeup is triggered by any enabled interrupt (here: TIM2 at 1 Hz).
 *   - RAM is preserved in STOP mode; flash remains readable.
 *   - Average current at 1 Hz sampling measured at ~80 uA on bench.
 *
 * This simulation tracks active vs sleep time to compute an estimated
 * average current draw, validating the power budget without real hardware.
 */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>

/* Approximate current draw values from STM32F4 datasheet (uA) */
#define CURRENT_RUN_UA       35000U   /* ~35 mA in run mode at 168 MHz */
#define CURRENT_STOP_UA        100U   /* ~100 uA in STOP mode */

/* Sample window for average current calculation (number of cycles) */
#define POWER_SAMPLE_WINDOW   100U

typedef struct {
    uint64_t total_active_us;   /* Microseconds spent in RUN mode */
    uint64_t total_sleep_us;    /* Microseconds spent in STOP mode */
    uint32_t cycle_count;
} PowerStats;

void     power_init(PowerStats *ps);
void     power_record_active(PowerStats *ps, uint32_t duration_us);
void     power_record_sleep(PowerStats *ps, uint32_t duration_us);

/**
 * power_average_current_ua()
 * Returns weighted average current in microamps over all recorded time.
 */
uint32_t power_average_current_ua(const PowerStats *ps);

/**
 * power_battery_life_hours()
 * Estimates battery life given a capacity in mAh.
 */
uint32_t power_battery_life_hours(const PowerStats *ps, uint32_t battery_mah);

void     power_print_report(const PowerStats *ps, uint32_t battery_mah);

#endif /* POWER_H */
