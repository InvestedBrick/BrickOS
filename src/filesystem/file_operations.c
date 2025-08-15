#include "file_operations.h"
#include "filesystem.h"
#include "../util.h"
#include "../memory/kmalloc.h"
#include "../screen.h"
#include "../drivers/ATA_PIO/ata.h"
#include "fs_defines.h"

vfs_handlers_t fs_ops = {
    .open = fs_open,
    .close = fs_close,
    .write = fs_write,
    .read = fs_read
};

unsigned int get_sector_for_rw(inode_t* inode, unsigned int sector_idx, unsigned char is_write) {
    unsigned int sector;

    if (sector_idx >= NUM_DATA_SECTORS_PER_FILE + ATA_SECTOR_SIZE / sizeof(unsigned int))
        return 0; // Signal: Invalid sector index

    if (sector_idx >= NUM_DATA_SECTORS_PER_FILE) {
        // Indirect sector
        if (is_write && !inode->indirect_sector) {
            inode->indirect_sector = allocate_sector();
            memset(last_read_sector, 0, ATA_SECTOR_SIZE);
            write_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, inode->indirect_sector);
            last_read_sector_idx = inode->indirect_sector;
        }

        if (last_read_sector_idx != inode->indirect_sector) {
            read_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, inode->indirect_sector);
            last_read_sector_idx = inode->indirect_sector;
        }

        unsigned int indirect_idx = sector_idx - NUM_DATA_SECTORS_PER_FILE;
        unsigned int* indirect_entries = (unsigned int*)last_read_sector;
        sector = indirect_entries[indirect_idx];

        if (is_write && !sector) {
            sector = allocate_sector();
            indirect_entries[indirect_idx] = sector;
            write_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, inode->indirect_sector);
        }

    } else {
        if (is_write && !inode->data_sectors[sector_idx]) {
            inode->data_sectors[sector_idx] = allocate_sector();
        }
        sector = inode->data_sectors[sector_idx];
    }

    return sector;
}

generic_file_t* fs_open(unsigned char* filepath,unsigned char flags){
    
    inode_t* inode;
    if(dir_contains_name(active_dir,filepath)){
        inode = get_inode_by_id(get_inode_id_by_name(active_dir->id,filepath));
    }else{
        inode = get_inode_by_full_file_path(filepath);
    }
    if (!inode) {
        if (flags & FILE_FLAG_CREATE){
            if (flags & FILE_CREATE_DIR){
                if (create_file(active_dir,filepath,strlen(filepath),FS_TYPE_DIR,FS_FILE_PERM_NONE) < 0)
                    return nullptr;
            }else{
                if (create_file(active_dir,filepath,strlen(filepath),FS_TYPE_FILE,FS_FILE_PERM_READABLE | FS_FILE_PERM_WRITABLE) < 0)
                    return nullptr;
            }
            inode = get_inode_by_id(get_inode_id_by_name(active_dir->id,filepath));
        }else{
            return nullptr;
        }
    }

    if ((!(inode->perms & FS_FILE_PERM_READABLE)) && (flags & FILE_FLAG_READ)) return nullptr;
    if ((!(inode->perms & FS_FILE_PERM_WRITABLE)) && (flags & FILE_FLAG_WRITE)) return nullptr;

    open_file_t* open_file = (open_file_t*)kmalloc(sizeof(open_file_t));
    open_file->inode_id = inode->id;
    open_file->flags = flags;
    open_file->rw_pointer = 0;

    generic_file_t* gen_file = (generic_file_t*)kmalloc(sizeof(generic_file_t)); 
    gen_file->ops = &fs_ops;
    gen_file->generic_data = (void*)open_file;
    return gen_file;
}

int fs_close(generic_file_t* f){

    open_file_t* file = (open_file_t*)f->generic_data;
    if (!file) return FILE_OP_FAILED;
    kfree(file);
    return FILE_OP_SUCCESS;
}

int fs_write(generic_file_t* f, unsigned char* buffer,unsigned int size){

    
    open_file_t* file = (open_file_t*)f->generic_data;
    
    if (!file) return FILE_INVALID_FD;
    if (!(file->flags & FILE_FLAG_WRITE)) return FILE_INVALID_PERMISSIONS;
    
    unsigned int write_pos = file->rw_pointer;

    inode_t* inode = get_inode_by_id(file->inode_id);

    if (inode->type == FS_TYPE_DIR) return FILE_INVALID_TARGET;

    if (file->flags & FILE_FLAG_APPEND) write_pos = inode->size;

    unsigned int bytes_written = 0;
    unsigned int to_write;
    while(bytes_written < size){
        unsigned int sector_idx = write_pos / ATA_SECTOR_SIZE;

        if (sector_idx >= MAX_FILE_SECTORS)
            return FILE_NO_CAPACITY;
        
        unsigned int sector = get_sector_for_rw(inode,sector_idx,1);
        if (!sector) return FILE_NO_CAPACITY;

        unsigned int sector_offset = write_pos % ATA_SECTOR_SIZE;
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector);
        last_read_sector_idx = sector;

        // ATA_SECTOR_SIZE - sector_offset is the remaining bytes in the sector
        to_write = size - bytes_written;
        if (to_write > ATA_SECTOR_SIZE - sector_offset) to_write = ATA_SECTOR_SIZE - sector_offset;

        memcpy(&last_read_sector[sector_offset],&buffer[bytes_written],to_write); // write until end of sector

        write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector);

        bytes_written += to_write;
        write_pos += to_write;
    }

    if (write_pos > inode->size) {
        inode->size = write_pos;
    }

    return bytes_written;
}

int fs_read(generic_file_t* f, unsigned char* buffer, unsigned int size){
    
    open_file_t* file = (open_file_t*)f->generic_data;
    
    if (!file) return FILE_INVALID_FD;
    if (!(file->flags & FILE_FLAG_READ)) return FILE_INVALID_PERMISSIONS;
    unsigned int read_pos = file->rw_pointer;

    inode_t* inode = get_inode_by_id(file->inode_id);

    if (size == FILE_READ_ALL) size = inode->size;

    if (inode->type == FS_TYPE_DIR) return FILE_INVALID_TARGET;

    unsigned int bytes_read = 0;
    unsigned int to_read;

    while (bytes_read < size)
    {
        unsigned int sector_idx = read_pos / ATA_SECTOR_SIZE;

        if (sector_idx >= MAX_FILE_SECTORS)
            return FILE_READ_OVER_END;
        
        unsigned int sector = get_sector_for_rw(inode,sector_idx,0);

        unsigned int sector_offset = read_pos % ATA_SECTOR_SIZE;

        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector);
        last_read_sector_idx = sector;

        to_read = size - bytes_read;
        if (to_read > ATA_SECTOR_SIZE - sector_offset) to_read = ATA_SECTOR_SIZE - sector_offset;

        memcpy(&buffer[bytes_read],&last_read_sector[sector_offset],to_read);

        bytes_read += to_read;
        read_pos += to_read;
            
    }
    
    return bytes_read;

}
