/**
 * power.c
 */

#include "power.h"
#include <stdio.h>
#include <string.h>

void power_init(PowerStats *ps)
{
    memset(ps, 0, sizeof(PowerStats));
}

void power_record_active(PowerStats *ps, uint32_t duration_us)
{
    ps->total_active_us += duration_us;
    ps->cycle_count++;
}

void power_record_sleep(PowerStats *ps, uint32_t duration_us)
{
    ps->total_sleep_us += duration_us;
}

uint32_t power_average_current_ua(const PowerStats *ps)
{
    uint64_t total_us = ps->total_active_us + ps->total_sleep_us;
    if (total_us == 0U) return 0U;

    /* Weighted average: (run_current * active_time + sleep_current * sleep_time) / total */
    uint64_t numerator = ((uint64_t)CURRENT_RUN_UA  * ps->total_active_us)
                       + ((uint64_t)CURRENT_STOP_UA * ps->total_sleep_us);
    return (uint32_t)(numerator / total_us);
}

uint32_t power_battery_life_hours(const PowerStats *ps, uint32_t battery_mah)
{
    uint32_t avg_ua = power_average_current_ua(ps);
    if (avg_ua == 0U) return 0U;
    /* battery_mah * 1000 converts mAh -> uAh; divide by avg_ua to get hours */
    return (battery_mah * 1000U) / avg_ua;
}

void power_print_report(const PowerStats *ps, uint32_t battery_mah)
{
    uint64_t total_us  = ps->total_active_us + ps->total_sleep_us;
    double   duty      = (total_us > 0)
                         ? (100.0 * (double)ps->total_active_us / (double)total_us)
                         : 0.0;
    uint32_t avg_ua    = power_average_current_ua(ps);
    uint32_t life_h    = power_battery_life_hours(ps, battery_mah);
    uint32_t life_days = life_h / 24U;
    uint32_t life_yrs  = life_days / 365U;

    printf("\n=== Power Budget Report ===\n");
    printf("  Cycles simulated  : %u\n",    ps->cycle_count);
    printf("  Active time       : %llu us\n", (unsigned long long)ps->total_active_us);
    printf("  Sleep time        : %llu us\n", (unsigned long long)ps->total_sleep_us);
    printf("  Duty cycle        : %.3f %%\n", duty);
    printf("  Avg current       : %u uA\n",  avg_ua);
    printf("  Battery (%u mAh) : ~%u hours / ~%u days / ~%u years\n",
           battery_mah, life_h, life_days, life_yrs);
    printf("===========================\n\n");
}
