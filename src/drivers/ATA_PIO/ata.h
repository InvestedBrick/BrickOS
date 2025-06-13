
#ifndef INCLUDE_ATA_H
#define INCLUDE_ATA_H

/**
 * Primary bus I/O ports:
 * 0x1f0 - 0x1f7
 * 
 * Secondary bus I/O ports:
 * 0x170 - 0x177
 * 
 * Primary alternate status port: 0x3f6
 * Secondary alternate status port: 0x376
 * 
 * Standard irq first bus: 14
 * Standard irq second bus: 15
 * 
 * Driver should block commands as long as one is already being processed
 * clear bsy and drq after each command
 * 
 * Setup drive: send 0xa0 to drive select port for drive 0 or 0xb0 for drive 1
 *               Set sectorcount, LBAlo, LBAmin and LBAhi ports to 0 (sector count 0 => 256 sectors)
 *               Read status port: if 0 -> does not exist
 *                                 else -> poll status port until bit 7(BSY) clears
 *               Check LBAmid and LBAhi to see if non-zero, else the drive is not ATA
 *               Poll one of status ports until bit 3(DRQ) sets or bit 0(ERR) sets
 *               if !ERR : Feel free to read the IDENTIFY data from data port (256 ushorts)
 * Info from the IDENTIFY data:
 *                 uint16_t 60 & 60 as uint32_t: total number of 28 bit addressable sectors
 *                 uint16_t 100 - 103 as uint64_t: total number of 48 bit addressable sectors
 * 
 * if no disk connected to bus -> reads 0xff
 * 
 * commands: Drive diagnostics 0x90
 *           IDENTIFY 0xec
 * 
 * port_offsets: 0  R/W Data register
 *               1  R   ERROR register
 *               1  W   Features register
 *               2  R/W Sector count register
 *               3  R/W LBA low
 *               4  R/W LBA mid
 *               5  R/W LBA high
 *               6  R   Drive / Head register
 *               7  R   Status register
 *               7  W   Command register
 */

#define ATA_PRIMARY_BUS_IO 0x1f0
#define ATA_SECONDARY_BUS_IO 0x170

#define ATA_PRIMARY_ALTERNATE_STATUS_PORT 0x3f6
#define ATA_SECONDARY_ALTERNATE_STATUS_PORT 0x376

#define ATA_DATA_PORT(base)             (base)
#define ATA_ERROR_PORT(base)            (base + 1)
#define ATA_FEATURE_PORT(base)          (base + 1)
#define ATA_SECTOR_COUNT_PORT(base)     (base + 2)
#define ATA_LBA_LOW_PORT(base)          (base + 3)
#define ATA_LBA_MID_PORT(base)          (base + 4)
#define ATA_LBA_HIGH_PORT(base)         (base + 5)
#define ATA_DRIVE_SELECT_PORT(base)     (base + 6)
#define ATA_STATUS_PORT(base)           (base + 7)
#define ATA_COMMAND_PORT(base)          (base + 7)

#define ATA_COMMAND_IDENTIFY 0xec
#define ATA_COMMAND_DRIVE_DIAGNOSTICS 0x90
#define ATA_COMMAND_WRITE_SECTORS 0x30
#define ATA_COMMAND_READ_SECTORS 0x20
#define ATA_COMMAND_CACHE_FLUSH 0xe7

#define ATA_INIT_DRIVE_ONE 0xa0
#define ATA_INIT_DRIVE_TWO 0xb0

#define ATA_SELECT_DRIVE_ONE 0xe0
#define ATA_SELECT_DRIVE_TWO 0xf0

#define ATA_SECTOR_SIZE 512

/**
 * init_disk_driver:
 * Initializes the Disk driver
 */
void init_disk_driver();

/**
 * read_sectors:
 * Reads a number of sectors from a specified ATA bus to a buffer
 * @param bus The ATA bus
 * @param n_sectors The number of sectors to read
 * @param buf The buffer to read the sectors into
 * @param lba The address from where to start reading
 */
void read_sectors(unsigned short bus,unsigned char n_sectors, unsigned char* buf, unsigned int lba);

/**
 * write_sectors:
 * Writes a number of sectors into a specified ATA bus
 * @param bus The ATA bus
 * @param n_sectors The number of sectors to write
 * @param buf The buffer which to write into the sectors
 * @param lba The address from where to start writing
 */
void write_sectors(unsigned short bus, unsigned char n_sectors, unsigned char* buf,unsigned int lba);


#endif