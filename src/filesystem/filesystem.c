#include "filesystem.h"
#include "../drivers/ATA_PIO/ata.h"
#include "../io/log.h"
#include "../util.h"
#include "../memory/kmalloc.h"

vector_t inodes;
vector_t inode_name_pairs;

unsigned char first_time_fs_init = 0;

unsigned char* last_read_sector;
unsigned int last_read_sector_idx;

inode_name_pair_t* get_name_by_inode_id(unsigned int id){
    for(unsigned int i = 0; i < inode_name_pairs.size;i++){
        inode_name_pair_t* pair = (inode_name_pair_t*)inode_name_pairs.data[i];
        if (pair->id == id){
            return pair;
        }
    }
    return 0;
}

unsigned int get_inode_id_by_name(unsigned int parent_id, const char* name){
    for (unsigned int i = 0; i < inode_name_pairs.size; i++) {
        inode_name_pair_t* pair = (inode_name_pair_t*)inode_name_pairs.data[i];
        if (pair->parent_id == parent_id && streq(pair->name, name, pair->length)) {
            return pair->id;
        }
    }
    return (unsigned int)-1;
}

void build_inodes(inode_t* start_node,unsigned int parent_id){
    /**
     * Directory entry:
     *               [n_entries (4 bytes)| inode id (4 bytes) | name len (1 byte) | name (name len bytes) | inode id (4 bytes) | name len (1 byte) | name (name len bytes) | ...] 
     */
    if (start_node->type == FS_TYPE_FILE) return;


    unsigned int indirect_sectors_count = 0;
    unsigned char* indirect_sectors = 0;
    if (start_node->indirect_sector){
        indirect_sectors = (char*)kmalloc(ATA_SECTOR_SIZE);
        read_sectors(ATA_PRIMARY_BUS_IO,1,indirect_sectors,start_node->indirect_sector * ATA_SECTOR_SIZE);

        for(unsigned int i = 0; *(unsigned int*)indirect_sectors[i] != 0, i < ATA_SECTOR_SIZE / sizeof(unsigned int);i+=4){
            indirect_sectors_count++;
        }
    }

    unsigned int n_sectors = 0;
    for(unsigned int sec = 0; start_node->data_sectors[sec] != 0 && sec < NUM_DATA_SECTORS;sec++){n_sectors++;}
    
    if (n_sectors == 0) return;

    unsigned char* buffer = (char*)kmalloc((n_sectors + indirect_sectors_count) * ATA_SECTOR_SIZE);


    // read all the sectors individually, since they might not be continuous
    for (unsigned int i = 0; i < n_sectors + indirect_sectors_count;i++ ){
        // normal sectors
        if (i < NUM_DATA_SECTORS){
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],start_node->data_sectors[i] * ATA_SECTOR_SIZE);
        }
        //indirect sectors
        else{
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],indirect_sectors[i - NUM_DATA_SECTORS] * ATA_SECTOR_SIZE);            
        }
    }

    //parse the sectors
    unsigned int n_entries = *(unsigned int*)(&buffer[0]);
    unsigned int buffer_idx = 4;
    for (unsigned int i = 0; i < n_entries;i++){
        unsigned int node_id = *(unsigned int*)buffer[buffer_idx];
        buffer_idx += 4;

        unsigned char name_len = buffer[buffer_idx];

        // save the name
        inode_name_pair_t* pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
        pair->id = node_id;
        pair->length = name_len;
        pair->parent_id = parent_id;
        memcpy(pair->name,buffer[buffer_idx + 1],name_len);
        vector_append(&inode_name_pairs,(unsigned int)pair);

        buffer_idx += name_len + 1;

        unsigned int inode_sector =  (node_id / FS_INODES_PER_SECTOR) + 1; // rounded towards 0 and +1 for the root sector
        unsigned int sector_offset = (node_id % FS_INODES_PER_SECTOR) * sizeof(inode_t);        


        inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));
        
        if (last_read_sector_idx != inode_sector){
            read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode_sector * ATA_SECTOR_SIZE);
            last_read_sector_idx = inode_sector;
        }

        // copy the data from disk to memory
        memcpy((void*)inode,&last_read_sector[sector_offset],sizeof(inode_t));

        vector_append(&inodes,(unsigned int)inode);
        build_inodes(inode,inode->id);
    }

    // We are nice today and return our memory
    kfree(buffer);
    kfree(indirect_sectors);

}

void init_filesystem(){
    // Check wether the disk driver has been set up
    if (!addressable_LBA28_sectors) {error("Disk driver has not been set up");return;}

    last_read_sector = kmalloc(ATA_SECTOR_SIZE);

    // read the first sector 
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0x00000000);
    last_read_sector_idx = 0;

    init_vector(&inodes);
    init_vector(&inode_name_pairs);

    inode_t* root_node = (inode_t*)kmalloc(sizeof(inode_t));

    // standard root dir stuff
    root_node->id = 0;
    root_node->type = FS_TYPE_DIR;

    if(!(last_read_sector[ATA_SECTOR_SIZE - 1] == FS_SECTOR_0_INIT_FLAG)){
        first_time_fs_init = 1;
        memset(root_node->data_sectors,0x0,NUM_DATA_SECTORS); // no data in the root dir right now
        root_node->size = 0;
        *(unsigned int*)&last_read_sector[0x0] = root_node->id;
        *(unsigned int*)&last_read_sector[0x4] = root_node->size;
        last_read_sector[0x8] = root_node->type;
        last_read_sector[0x9] = 0;
        last_read_sector[0xa] = 0;
        last_read_sector[0xb] = 0;
        *(unsigned int*)&last_read_sector[0xc] = 0;
        memset(&last_read_sector[0x10],0x0,NUM_DATA_SECTORS);
        last_read_sector[ATA_SECTOR_SIZE - 1] = FS_SECTOR_0_INIT_FLAG;
        write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0x00000000); 

    }else{
        /**
         * 0x00000000: root_id
         * 0x00000004: root_size
         * 0x00000008: root_type
         * 0x00000009: flags 1
         * 0x0000000a: flags 2
         * 0x0000000b: flags 3
         * 0x0000000c: indirect_sector
         * 0x00000010: root_data_sectors
         */
        root_node->id = *(unsigned int*)&last_read_sector[0x00];
        root_node->type = last_read_sector[0x08];
        root_node->size = *(unsigned int*)&last_read_sector[0x04];
        root_node->indirect_sector = *(unsigned int*)&last_read_sector[0xc];
        memcpy(root_node->data_sectors,last_read_sector[0x10],NUM_DATA_SECTORS);


    }

    vector_append(&inodes,(unsigned int)root_node);

    build_inodes(root_node,root_node->id);
}

void create_directory(inode_t* parent_dir){

}