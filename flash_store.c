/**
 * flash_store.c
 */

#include "flash_store.h"
#include <string.h>
#include <stdint.h>

void flash_init(FlashStore *fs)
{
    memset(fs, 0, sizeof(FlashStore));
    fs->active_sector = 0U;   /* Sector 0 is reserved for metadata in real hw;
                                  here we use index 0 as first data sector for
                                  simplicity in the simulation. */
}

/**
 * find_least_written_sector()
 * Returns the index of the sector with the lowest write count,
 * excluding the current active sector.
 */
static uint8_t find_least_written_sector(const FlashStore *fs)
{
    uint8_t  best_idx   = (fs->active_sector + 1U) % FLASH_NUM_SECTORS;
    uint32_t best_count = fs->write_counts[best_idx];

    for (uint8_t i = 0U; i < FLASH_NUM_SECTORS; i++) {
        if (i == fs->active_sector) continue;
        if (fs->write_counts[i] < best_count) {
            best_count = fs->write_counts[i];
            best_idx   = i;
        }
    }
    return best_idx;
}

bool flash_write(FlashStore *fs, const uint8_t *buf, size_t len)
{
    /* Rotate if wear threshold hit or sector is full */
    FlashSector *sec = &fs->sectors[fs->active_sector];
    bool needs_rotate =
        (fs->write_counts[fs->active_sector] >= FLASH_WEAR_THRESHOLD) ||
        (sec->write_pos + len > FLASH_SECTOR_BYTES);

    if (needs_rotate) {
        uint8_t next = find_least_written_sector(fs);

        /* Check that the target sector actually has space */
        if (fs->sectors[next].write_pos + len > FLASH_SECTOR_BYTES) {
            /* All sectors full */
            return false;
        }
        fs->active_sector = next;
        sec = &fs->sectors[fs->active_sector];
    }

    memcpy(&sec->data[sec->write_pos], buf, len);
    sec->write_pos += (uint32_t)len;
    fs->write_counts[fs->active_sector]++;
    return true;
}

void flash_get_stats(const FlashStore *fs, uint32_t out[FLASH_NUM_SECTORS])
{
    for (uint8_t i = 0U; i < FLASH_NUM_SECTORS; i++) {
        out[i] = fs->write_counts[i];
    }
}
