#include "file_operations.h"
#include "filesystem.h"
#include "../util.h"
#include "../memory/kmalloc.h"
#include "../screen/screen.h"
#include "../drivers/ATA_PIO/ata.h"
#include "fs_defines.h"
#include "devices/devs.h"
#include "IPC/pipes.h"

vfs_handles_t fs_ops = {
    .open = fs_open,
    .close = fs_close,
    .write = fs_write,
    .read = fs_read,
    .seek = fs_seek,
    .ioctl = 0,
};

int fs_seek(generic_file_t* file, uint32_t offset){
    open_file_t* open_file = (open_file_t*)file->generic_data;
    inode_t* inode = get_inode_by_id(open_file->inode_id);
    if (offset >= inode->size) offset = inode->size - 1;
    open_file->rw_pointer = offset;

    return offset;
}

uint32_t get_sector_for_rw(inode_t* inode, uint32_t sector_idx, uint8_t is_write) {
    uint32_t sector;

    if (sector_idx >= NUM_DATA_SECTORS_PER_FILE + ATA_SECTOR_SIZE / sizeof(uint32_t))
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

        uint32_t indirect_idx = sector_idx - NUM_DATA_SECTORS_PER_FILE;
        uint32_t* indirect_entries = (uint32_t*)last_read_sector;
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

generic_file_t* fs_open(unsigned char* filepath,uint8_t flags){
    
    inode_t* inode = get_inode_by_path(filepath);
    
    if (inode && inode->type == FS_TYPE_DEV) return dev_open(inode);
    if (inode && inode->type == FS_TYPE_PIPE) return open_pipe(inode,flags);

    if (inode && flags & FILE_FLAG_CREATE) return nullptr;
    if (!inode) {
        if (flags & FILE_FLAG_CREATE){
            if (flags & FILE_CREATE_DIR){
                if (create_file(active_dir,filepath,strlen(filepath),FS_TYPE_DIR,FS_FILE_PERM_NONE) < 0)
                    return nullptr;
            }else {
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
    gen_file->object_id = inode->id;
    return gen_file;
}

int fs_close(generic_file_t* f){

    open_file_t* file = (open_file_t*)f->generic_data;
    if (!file) return FILE_OP_FAILED;
    kfree(file);
    return FILE_OP_SUCCESS;
}

int fs_write(generic_file_t* f, unsigned char* buffer,uint32_t size){

    
    open_file_t* file = (open_file_t*)f->generic_data;
    
    if (!file) return FILE_INVALID_FD;
    if (!(file->flags & FILE_FLAG_WRITE)) return FILE_INVALID_PERMISSIONS;
    
    uint32_t write_pos = file->rw_pointer;

    inode_t* inode = get_inode_by_id(file->inode_id);

    if (inode->type == FS_TYPE_DIR) return FILE_INVALID_TARGET;

    if (file->flags & FILE_FLAG_APPEND) write_pos = inode->size;

    uint32_t bytes_written = 0;
    uint32_t to_write;
    while(bytes_written < size){
        uint32_t sector_idx = write_pos / ATA_SECTOR_SIZE;

        if (sector_idx >= MAX_FILE_SECTORS)
            return FILE_NO_CAPACITY;
        
        uint32_t sector = get_sector_for_rw(inode,sector_idx,1);
        if (!sector) return FILE_NO_CAPACITY;

        uint32_t sector_offset = write_pos % ATA_SECTOR_SIZE;
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

int fs_read(generic_file_t* f, unsigned char* buffer, uint32_t size){
    
    open_file_t* file = (open_file_t*)f->generic_data;
    
    if (!file) return FILE_INVALID_FD;
    if (!(file->flags & FILE_FLAG_READ)) return FILE_INVALID_PERMISSIONS;
    uint32_t read_pos = file->rw_pointer;

    inode_t* inode = get_inode_by_id(file->inode_id);

    if (size == FILE_READ_ALL) size = inode->size;

    if (inode->type == FS_TYPE_DIR) return FILE_INVALID_TARGET;

    uint32_t bytes_read = 0;
    uint32_t to_read;

    while (bytes_read < size)
    {
        uint32_t sector_idx = read_pos / ATA_SECTOR_SIZE;

        if (sector_idx >= MAX_FILE_SECTORS)
            return FILE_READ_OVER_END;
        
        uint32_t sector = get_sector_for_rw(inode,sector_idx,0);

        uint32_t sector_offset = read_pos % ATA_SECTOR_SIZE;

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
