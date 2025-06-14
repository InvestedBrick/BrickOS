
#ifndef INCLUDE_FILESYSTEM_H
#define INCLUDE_FILESYSTEM_H

#define NUM_DATA_SECTORS_PER_FILE 10

#define RESERVED_BITMAP_SECTORS 17 // (16 * 8) normal sector bitmaps + 1 sector for header + 2 big bitmaps
// we can create up to 800 files / directories. Will be enough for now
#define RESERVED_INODE_SECTORS 100
#define RESERVED_ROOT_SECTOR 1
#define RESERVED_SECTORS (RESERVED_BITMAP_SECTORS + RESERVED_ROOT_SECTOR + RESERVED_INODE_SECTORS) 
#define BITMAP_SECTOR_START 101
/**
 * Sectors: 0         => root sector
 *          1 - 100   => inodes
 *          101 - 117 => free sector bitmap 
 */

#define TOTAL_SECTORS 524288
#define BIG_SECTOR_SECTOR_COUNT 512
#define TOTAL_BIG_SECTORS (TOTAL_SECTORS / BIG_SECTOR_SECTOR_COUNT) //1024
#define BIG_SECTOR_BITMAP_SIZE (TOTAL_BIG_SECTORS / 8) // 128
#define SECTOR_BITMAP_SIZE (BIG_SECTOR_SECTOR_COUNT / 8) // 64
#define SECTOR_BITMAPS_PER_SECTOR (ATA_SECTOR_SIZE / SECTOR_BITMAP_SIZE) // 8

#define BIT_SET(bitmap, idx)   (bitmap[(idx)/8] |= (1 << ((idx)%8)))
#define BIT_CLEAR(bitmap, idx) (bitmap[(idx)/8] &= ~(1 << ((idx)%8)))
#define BIT_CHECK(bitmap, idx) (bitmap[(idx)/8] & (1 << ((idx)%8)))


typedef struct{
    unsigned char big_sector_used_map[TOTAL_BIG_SECTORS / 8]; // 128 bytes as 1024-bit bitmap
    unsigned char big_sector_full_map[TOTAL_BIG_SECTORS / 8];

    unsigned char* sector_bitmaps[TOTAL_BIG_SECTORS];   // pointers to 64-byte (512-bit) normal sector bitmaps
}sectors_headerdata_t;

#define FS_DATA_START_LBA ((RESERVED_SECTORS) * 512)
#define FS_SECTOR_0_INIT_FLAG 0xff

#define FS_TYPE_FILE 1
#define FS_TYPE_DIR 2
extern unsigned char first_time_fs_init;

// Padding so that the size is 64 and they are sector aligned
typedef struct {
    unsigned int id;
    unsigned int size;
    unsigned char type;
    unsigned char unused_flag_one;
    unsigned char unused_flag_two;
    unsigned char unused_flag_three;
    unsigned int indirect_sector;
    unsigned int data_sectors[NUM_DATA_SECTORS_PER_FILE];

}__attribute__((packed)) inode_t;

typedef struct {
    unsigned int parent_id;
    unsigned int id;
    unsigned char length;
    unsigned char* name;
}inode_name_pair_t;

// This should be 8
#define FS_INODES_PER_SECTOR (ATA_SECTOR_SIZE / sizeof(inode_t)) 

/**
 * init_filesystem:
 * Initializes the filesystem
 * 
 */
void init_filesystem();
/**
 * get_name_by_inode_id:
 *  
 * @param id The id of the inode
 * @return A pointer to a inode_name_pair_t struct where the name is contained
 */
inode_name_pair_t* get_name_by_inode_id(unsigned int id);

/**
 * get_inode_id_by_name:
 * @param parent_id The id of the parent directory
 * @param name The name of the direcotry/file
 * 
 */
unsigned int get_inode_id_by_name(unsigned int parent_id, const char* name);

/**
 * read_bitmaps:
 * Reads the stored bitmaps from disk and loads them into a sectors_headerdata_t struct
 */
void read_bitmaps();

/**
 * write_bitmaps: 
 * Writes the bitmaps from the sectors_headerdata_t struct into the disk
 */
void write_bitmaps();

/**
 * free_sector: 
 * Frees an allocated sector of the disk
 * @param sector_index The sector index of the sector
 */
void free_sector(unsigned int sector_index);

/**
 * allocate_sector: 
 * Allocates a sector from the disk
 * @return The sector index from the allocated sector
 */
unsigned int allocate_sector();
#endif