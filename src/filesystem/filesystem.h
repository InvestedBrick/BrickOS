
#ifndef INCLUDE_FILESYSTEM_H
#define INCLUDE_FILESYSTEM_H

#define NUM_DATA_SECTORS 10

// we can create up to 800 files / directories. Will be enough for now
#define RESERVED_SECTORS 102
/**
 * Sectors: 0       => root sector
 *          1 - 100 => inodes
 *          101     => free sector bitmap 
 */
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
    unsigned int data_sectors[NUM_DATA_SECTORS];

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
#endif