/**
 * flash_store.h
 * Minimal wear-levelling layer for logging sensor readings to flash.
 *
 * The flash is divided into FLASH_NUM_SECTORS equal-sized sectors.
 * A metadata sector (sector 0) holds a write-count table — one uint32_t
 * per data sector.  Before every flush, the write count for the current
 * sector is checked.  When it reaches FLASH_WEAR_THRESHOLD the writer
 * rotates to the next sector with the lowest write count.
 *
 * This is a simplified round-robin implementation suitable for explaining
 * the concept.  A production implementation would use a more sophisticated
 * bad-block map and handle metadata sector endurance separately.
 */

#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FLASH_NUM_SECTORS      8U
#define FLASH_SECTOR_BYTES     4096U
#define FLASH_WEAR_THRESHOLD   1000U   /* Rotate sector after this many writes */

typedef struct {
    uint8_t  data[FLASH_SECTOR_BYTES];
    uint32_t write_pos;                /* Byte offset within current sector */
} FlashSector;

typedef struct {
    FlashSector  sectors[FLASH_NUM_SECTORS];
    uint32_t     write_counts[FLASH_NUM_SECTORS];
    uint8_t      active_sector;
} FlashStore;

/**
 * flash_init()
 * Zero-initialise all sectors and write counts.
 */
void flash_init(FlashStore *fs);

/**
 * flash_write()
 * Append len bytes from buf to the active sector.
 * Rotates to the least-written sector when FLASH_WEAR_THRESHOLD is reached
 * or when the current sector is full.
 * Returns false if all sectors are full.
 */
bool flash_write(FlashStore *fs, const uint8_t *buf, size_t len);

/**
 * flash_get_stats()
 * Fill out[FLASH_NUM_SECTORS] with write counts for each sector.
 * Useful for validating wear distribution in tests.
 */
void flash_get_stats(const FlashStore *fs, uint32_t out[FLASH_NUM_SECTORS]);

#endif /* FLASH_STORE_H */
