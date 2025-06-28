#include "file_operations.h"
#include "filesystem.h"
#include "../util.h"
#include "../vector.h"
#include "../memory/kmalloc.h"
#include "../screen.h"
#include "../drivers/ATA_PIO/ata.h"

vector_t fd_vector;
static unsigned char fd_used[MAX_FDS] = {0};
static unsigned int next_fd = 1;

unsigned int get_fd(){
    for (unsigned int i = 0; i < MAX_FDS; ++i) {
        unsigned int fd = next_fd;
        next_fd = (next_fd % MAX_FDS) + 1;
        if (!fd_used[fd]) {
            fd_used[fd] = 1;
            return fd;
        }
    }
    return 0;
}

void free_fd(unsigned int fd){
    if (fd > 0 && fd < MAX_FDS){
        fd_used[fd] = 0;
    }
}

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


open_file_t* get_open_file_by_fd(unsigned int fd){
    for(unsigned int i = 0; i < fd_vector.size;i++){
        open_file_t* file = (open_file_t*)fd_vector.data[i];
        if(file->fd == fd){
            return file;
        }
    }
    return 0;
}

int open(unsigned char* filepath,unsigned char flags){
    
    inode_t* inode;
    if(dir_contains_name(active_dir,filepath)){
        inode = get_inode_by_id(get_inode_id_by_name(active_dir->id,filepath));
    }else{
        inode = get_inode_by_full_file_path(filepath);
    }
    if (!inode) return FILE_OP_FAILED;

    open_file_t* file = (open_file_t*)kmalloc(sizeof(open_file_t));
    file->fd = get_fd();
    if (!file->fd) return FILE_OP_FAILED;
    file->inode_id = inode->id;
    file->flags = flags;
    file->rw_pointer = 0;

    vector_append(&fd_vector,(unsigned int)file);
    return file->fd;
}

int close(unsigned int fd){
    for(unsigned int i = 0; i < fd_vector.size;++i){
        open_file_t* file = (open_file_t*)fd_vector.data[i];
        if (file->fd == fd){
            free_fd(fd);
            vector_erase(&fd_vector,i);
            kfree(file);
            return FILE_OP_SUCCESS;
        }
    }

    return FILE_INVALID_FD;
}

int write(unsigned int fd, unsigned char* buffer,unsigned int size){

    
    open_file_t* file = get_open_file_by_fd(fd);
    
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

int read(unsigned int fd, unsigned char* buffer, unsigned int size){
    open_file_t* file = get_open_file_by_fd(fd);
    
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