# UART Sensor Node — STM32F4 Bare-Metal Firmware

A bare-metal C firmware project for the STM32F4 microcontroller that reads environmental sensor data over UART, buffers it in a lock-free ring buffer, and flushes to flash storage with wear-levelling — targeting a ~10-year battery life on a single AA cell.

This repository includes a full host-side simulation build so every module can be compiled and tested on any Linux or macOS machine without hardware.

---

## Project motivation

Most embedded tutorials stop at "blink an LED." This project was built to understand the actual constraints that matter in a long-life field sensor:

- **Power budget**: how do you achieve under 100 µA average draw on a device that samples at 1 Hz?
- **Memory safety**: how do you write a ring buffer that is correct across the wrap boundary — not just in the happy path?
- **Flash longevity**: what happens to a device that logs every 10 minutes for 15 years if you always write to the same sector?
- **Automated validation**: how do you verify firmware correctness without spending 30 minutes with a debugger every time you change something?

---

## Architecture

```
Sensor (UART) ──► UART ISR ──► RingBuffer ──► Main loop ──► FlashStore
                                                │
                                          PowerStats
                                          (sleep/active tracking)
```

**`ring_buffer`** — Lock-free single-producer / single-consumer circular buffer. ISR writes (producer), main loop reads (consumer). Power-of-2 capacity enables bitmask wrap — no branch, no modulo.

**`flash_store`** — Write-ahead flash logger with sector rotation. Tracks write counts per sector and rotates to the least-written sector when the threshold is reached.

**`power`** — Records active and sleep durations per cycle and computes weighted average current draw. Estimates battery life given a capacity in mAh.

---

## Power budget

| State       | Current    | Time per 1 Hz cycle |
|-------------|------------|---------------------|
| RUN mode    | ~35 mA     | ~5 ms               |
| STOP mode   | ~100 µA    | ~995 ms             |
| **Average** | **~278 µA**| —                   |

On real STM32F4 hardware the STOP mode current is lower (~2–5 µA with RTC only), which brings average draw down toward 80 µA. The simulation uses the datasheet worst-case STOP value of 100 µA.

Battery life estimate (3000 mAh AA):
- At 80 µA average: **~4.3 years**
- Achievable with larger battery pack or lower sample rate: **10+ years**

---

## Getting started

**Requirements:** GCC, Make, Python 3.8+

```bash
# Clone
git clone https://github.com/aadit-gupta/uart-sensor-node
cd uart-sensor-node

# Build and run the firmware simulation
make run

# Run ring buffer unit tests
make test

# Run Python hardware validation script (simulation mode)
make validate
```

---

## Ring buffer — the off-by-one bugs

During development, two off-by-one bugs were found in the wrap logic. Both were caught by the automated Python validation script before they could corrupt data silently in a field device.

The bugs: when the write pointer reached `RB_CAPACITY - 1`, one code path wrapped to slot 1 instead of slot 0. Over 64 write cycles, the read and write pointers diverged and stale data was returned.

The test that catches this is `test_wrap_around` in `tests/test_ring_buffer.c`. The Python script in `scripts/validate_firmware.py` can inject the buggy wrap behaviour (`--inject-bug`) to demonstrate detection.

---

## Flash wear-levelling

| Parameter            | Value  |
|----------------------|--------|
| Sectors              | 8      |
| Bytes per sector     | 4096   |
| Wear threshold       | 1000 writes |

When a sector reaches the write threshold, `flash_write()` scans all sectors for the one with the lowest write count and rotates to it. This distributes writes evenly across all sectors.

Without wear-levelling, a device logging every 10 minutes would write to the same sector ~52,560 times per year. At 10 years that is 525,600 cycles — well past typical NOR flash endurance of 100,000 cycles.

---

## On real hardware

The firmware targets STM32F4 (Cortex-M4, 168 MHz). To build for the target:

1. Install ARM GCC toolchain: `arm-none-eabi-gcc`
2. Update `Makefile` with `CC = arm-none-eabi-gcc` and appropriate `-mcpu` flags
3. Add STM32F4 CMSIS headers and startup files
4. Replace simulation `DISABLE_IRQS` / `ENABLE_IRQS` macros with `__disable_irq()` / `__enable_irq()`
5. Replace `power_record_*` calls with actual `SCB->SCR` and `WFI` instructions

---

## File structure

```
uart-sensor-node/
├── include/
│   ├── ring_buffer.h
│   ├── flash_store.h
│   └── power.h
├── src/
│   ├── ring_buffer.c
│   ├── flash_store.c
│   ├── power.c
│   └── main.c
├── tests/
│   └── test_ring_buffer.c
├── scripts/
│   └── validate_firmware.py
├── Makefile
└── README.md
```
