
#ifndef INCLUDE_FILESYSTEM_H
#define INCLUDE_FILESYSTEM_H

#include "../util.h"
// must be set so that the inode struct is exactly 64 bytes wide
#define NUM_DATA_SECTORS_PER_FILE 12

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
    unsigned char inode_sector_map[RESERVED_INODE_SECTORS]; // (100 sectors * 8 inodes/sector) / 8 bits/byte
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

extern inode_t* active_dir;

typedef struct {
    unsigned int parent_id;
    unsigned int id;
    unsigned char length;
    unsigned char* name;
}inode_name_pair_t;


// This should be 8
#define FS_INODES_PER_SECTOR (ATA_SECTOR_SIZE / sizeof(inode_t)) 

// make sure that the root dir id is unique and cannot be overwritten by allocated inodes
#define FS_ROOT_DIR_ID ((RESERVED_INODE_SECTORS * FS_INODES_PER_SECTOR ) + 1)
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
 * @param name The name of the directory/file
 * 
 */
unsigned int get_inode_id_by_name(unsigned int parent_id, unsigned char* name);

/**
 * get_inode_by_id: 
 * Returns a pointer to an inode with the given id
 * @param id The id of the inode
 * @return A pointer to the inode
 */
inode_t* get_inode_by_id(unsigned int id);
/**
 * get_active_dir: 
 * Returns the name of the current active directory
 * IMPORTANT: Do NOT free the char* associated with the returned string, it belongs to inode_name_pairs. This isn't Rust, get over it
 * @return The name of the directory
 */
string_t get_active_dir();

/**
 * read_bitmaps:
 * Reads the stored bitmaps from disk and loads them into a sectors_headerdata_t struct
 */
void read_bitmaps_from_disk();

/**
 * write_bitmaps: 
 * Writes the bitmaps from the sectors_headerdata_t struct into the disk
 */
void write_bitmaps_to_disk();

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

/**
 * get_all_names_in_dir:
 * Returns an array of strings which contain all the names of the directory entries
 * 
 * IMPORTANT: You need to free the returned strings, the string struct and the returned string array pointer (See free_string_arr() in util.h)
 * @param dir The directory of which to get the entires
 * @param add_slash A boolean value deciding if directories shuld get an added '/' when being returned
 * @return Array of strings which contain the data
 * 
 */
string_array_t* get_all_names_in_dir(inode_t* dir,unsigned char add_slash);

/**
 * create_directory:
 * Creates a directory in a given parent directory
 * 
 * @param parent_dir The parent directory inode
 * @param name The name of the new directory
 * @param name_length The length if the name
 */
void create_file(inode_t* parent_dir, unsigned char* name, unsigned char name_length,unsigned char file);

/**
 * write_to_disk: 
 * Writes the disk metadata to the disk
 */
void write_to_disk();


/** 
 * get_parent_inode: 
 * Returns a pointer to the parent inode of a child
 * @param child The child inode
 * @return The parent inode
*/
inode_t* get_parent_inode(inode_t* child);

/**
 * get_inode_by_full_file_path: 
 * Returns a pointer to the inode which indexes the provided file 
 * @param path The file path
 * @return The inode pointer
 */
inode_t* get_inode_by_full_file_path(unsigned char* path);

/**
 * dir_contains_name: 
 * Returns whether a directory contains a certain file
 * @param dir The directory
 * @param name The name of the file
 * 
 * @return 1 if the directory contains the name
 * 
 *         0 of the directory does not contain the name
 */
unsigned char dir_contains_name(inode_t* dir,unsigned char* name);
#endif