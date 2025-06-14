#include "filesystem.h"
#include "../drivers/ATA_PIO/ata.h"
#include "../io/log.h"
#include "../util.h"
#include "../memory/kmalloc.h"

vector_t inodes;
vector_t inode_name_pairs;
sectors_headerdata_t header_data;

unsigned int active_partially_used_bitmaps;

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

unsigned int count_partially_used_big_sectors(unsigned int end){
    unsigned int partially_used_bitmaps = 0;
    for(unsigned int i = 0; i < end;i++){
        for(unsigned int j = 0; j < 8;j++){
            // check whether the big sector is partially used, if so, a bitmap for it should exist
            if (BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) &&
               !BIT_CHECK(header_data.big_sector_full_map,i * 8 + j)) partially_used_bitmaps++;
        }
    }

    return partially_used_bitmaps;
}

unsigned char is_bitmap_clear(unsigned char* bitmap, unsigned int byte_size){
    for (unsigned int i = 0; i < byte_size; i++){
        if (bitmap[i] != 0x00) return 0;
    }

    return 1;
}

unsigned char is_bitmap_full(unsigned char* bitmap, unsigned int byte_size){
    for (unsigned int i = 0; i < byte_size; i++){
        if (bitmap[i] != 0xff) return 0;
    }

    return 1;
}

void insert_sector_bitmap(unsigned char** bitmaps,unsigned char* new_map,unsigned int pos, unsigned int n_active_bitmaps){
    for (unsigned int i = n_active_bitmaps + 1; i > pos;i--){
        bitmaps[i] = bitmaps[i - 1];
    }
    bitmaps[pos] = new_map;
}

void shift_sector_bitmaps_left(unsigned char** bitmaps, unsigned int start, unsigned int n_active_bitmaps){
    for (unsigned int i = start; i < n_active_bitmaps - 1; i++){
        bitmaps[i] = bitmaps[i + 1];
    }
    bitmaps[n_active_bitmaps - 1] = 0;
}

void read_bitmaps(){
    // bitmap headerdata starts at sector 101 (BITMAP_SECTOR_START)
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,BITMAP_SECTOR_START * ATA_SECTOR_SIZE);
    last_read_sector_idx = BITMAP_SECTOR_START;

    memcpy(header_data.big_sector_used_map,last_read_sector,BIG_SECTOR_BITMAP_SIZE); // read in used bitmap
    memcpy(header_data.big_sector_full_map,&last_read_sector[BIG_SECTOR_BITMAP_SIZE],BIG_SECTOR_BITMAP_SIZE); // read in fully used bitmap

    unsigned int partially_used_bitmaps = count_partially_used_big_sectors(BIG_SECTOR_BITMAP_SIZE);
    if (partially_used_bitmaps > 128) panic("Not enough reserved sectors for bitmaps");
    active_partially_used_bitmaps = partially_used_bitmaps;

    for (unsigned int i = 0; i < partially_used_bitmaps;i++){
        unsigned int sector_bitmap_idx = (BITMAP_SECTOR_START + 1) + (i / SECTOR_BITMAPS_PER_SECTOR);  
        unsigned int bitmap_idx_in_sector = (i % SECTOR_BITMAPS_PER_SECTOR);

        if (last_read_sector_idx != sector_bitmap_idx){
            read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,last_read_sector_idx * ATA_SECTOR_SIZE);
            last_read_sector_idx = sector_bitmap_idx;
        }

        header_data.sector_bitmaps[i] = (unsigned char*)kmalloc(SECTOR_BITMAP_SIZE);

        // read in the desired sector bitmap
        memcpy(header_data.sector_bitmaps[i],last_read_sector[bitmap_idx_in_sector * SECTOR_BITMAP_SIZE],SECTOR_BITMAP_SIZE);
    }

}

void write_bitmaps() {

    unsigned char sector[512] = {0};
    memcpy(sector,header_data.big_sector_used_map,BIG_SECTOR_BITMAP_SIZE);
    memcpy(&sector[BIG_SECTOR_BITMAP_SIZE],header_data.big_sector_full_map,BIG_SECTOR_BITMAP_SIZE);
    write_sectors(ATA_PRIMARY_BUS_IO,1,sector,BITMAP_SECTOR_START);

    unsigned char* bitmaps = (unsigned char*)kmalloc(active_partially_used_bitmaps * SECTOR_BITMAP_SIZE);
    for (unsigned int i = 0; i < active_partially_used_bitmaps;i++){
        memcpy(&bitmaps[i * SECTOR_BITMAP_SIZE],header_data.sector_bitmaps[i],SECTOR_BITMAP_SIZE);
    }

    write_sectors(ATA_PRIMARY_BUS_IO,CEIL_DIV(active_partially_used_bitmaps, 8),bitmaps,(BITMAP_SECTOR_START + 1));

    kfree(bitmaps);
}

void free_sector(unsigned int sector_idx) {
    unsigned int big_sector_idx = sector_idx / BIG_SECTOR_SECTOR_COUNT;
    unsigned int big_sector_sector_idx = sector_idx % BIG_SECTOR_SECTOR_COUNT;

    if (!BIT_CHECK(header_data.big_sector_used_map,big_sector_idx)) return;
    if (big_sector_idx >= TOTAL_BIG_SECTORS || big_sector_sector_idx >= BIG_SECTOR_SECTOR_COUNT) return;
    
    unsigned int bitmap_idx = count_partially_used_big_sectors(big_sector_idx);

    if (BIT_CHECK(header_data.big_sector_full_map,big_sector_idx)){
        // sector is full, needs to be re-allocated

        // insert bitmap with bitmap[big_sector_sector_idx] = 0
        unsigned char* map = (unsigned char*)kmalloc(SECTOR_BITMAP_SIZE);
        memset(map,0xff,SECTOR_BITMAP_SIZE);
        BIT_CLEAR(map,big_sector_sector_idx);

        insert_sector_bitmap(header_data.sector_bitmaps,map,bitmap_idx,active_partially_used_bitmaps);
        active_partially_used_bitmaps++;

        // set full to 0
        BIT_CLEAR(header_data.big_sector_full_map,big_sector_idx);

    }else{
        // sector is partially full

        // get bitmap index

        unsigned char* bitmap = header_data.sector_bitmaps[bitmap_idx];

        BIT_CLEAR(bitmap,big_sector_sector_idx);

        if (is_bitmap_clear(bitmap,SECTOR_BITMAP_SIZE)){
            // remove the free bitmap
            BIT_CLEAR(header_data.big_sector_used_map,big_sector_idx);
            kfree(bitmap);
            shift_sector_bitmaps_left(header_data.sector_bitmaps,bitmap_idx,active_partially_used_bitmaps);
            active_partially_used_bitmaps--;
        }

    }
}

unsigned int allocate_sector(){

    unsigned int partially_free_big_sector_idx = (unsigned int)-1;
    unsigned int first_free_big_sector_idx = (unsigned int)-1;

    unsigned char break_loop = 0; 
    unsigned char free_sector_found = 0;
    for (unsigned int i = 0; i < 128;i++){

        for (unsigned int j = 0; j < 8;j++){
            
            // partially used big sector
            if (BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) &&
               !BIT_CHECK(header_data.big_sector_full_map,i * 8 + j)) {partially_free_big_sector_idx = i * 8 + j;break_loop = 1;break;}

            if (!BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) && !free_sector_found) {first_free_big_sector_idx = i * 8 + j; free_sector_found = 1;}
        }
        if (break_loop) break;
    }

    // there was no partially free sector found
    if (partially_free_big_sector_idx == (unsigned int)-1){

        if (first_free_big_sector_idx == (unsigned int)-1) panic("No free big sector exists");

        unsigned char* bitmap = (unsigned char*)kmalloc(SECTOR_BITMAP_SIZE);

        // alloc first sector of big sector
        BIT_SET(bitmap,0); 

        BIT_SET(header_data.big_sector_used_map,first_free_big_sector_idx);
        active_partially_used_bitmaps++;

        // this bitmap must be at position 0 because if there are no partially free sectors, there are no bitmaps present
        header_data.sector_bitmaps[0] = bitmap;

        return first_free_big_sector_idx * BIG_SECTOR_SECTOR_COUNT;

    }else{
        unsigned char* big_sector_bitmap = header_data.sector_bitmaps[0];

        unsigned int free_sector_idx = (unsigned int)-1;

        for (unsigned int i = 0; i < SECTOR_BITMAP_SIZE; i++){
            for (unsigned int j = 0; j < 8; j++){
                if (!BIT_CHECK(big_sector_bitmap,i * 8 + j)) {

                    BIT_SET(big_sector_bitmap,i * 8 + j);
                    
                    if (is_bitmap_full(big_sector_bitmap,SECTOR_BITMAP_SIZE)){
                        kfree(big_sector_bitmap);
                        shift_sector_bitmaps_left(header_data.sector_bitmaps,0,active_partially_used_bitmaps);
                        active_partially_used_bitmaps--;
                        BIT_SET(header_data.big_sector_full_map,i * 8 + j);
                    }
                    return (partially_free_big_sector_idx * BIG_SECTOR_SECTOR_COUNT + i * 8 + j);
                }
            }
        }
    }

    // should never happen
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
        read_sectors(ATA_PRIMARY_BUS_IO,1,indirect_sectors,start_node->indirect_sector);
        last_read_sector_idx = start_node->indirect_sector;

        for(unsigned int i = 0; *(unsigned int*)indirect_sectors[i] != 0, i < ATA_SECTOR_SIZE / sizeof(unsigned int);i+=4){
            indirect_sectors_count++;
        }
    }

    unsigned int n_sectors = 0;
    for(unsigned int sec = 0; start_node->data_sectors[sec] != 0 && sec < NUM_DATA_SECTORS_PER_FILE;sec++){n_sectors++;}
    
    if (n_sectors == 0) return;

    unsigned char* buffer = (char*)kmalloc((n_sectors + indirect_sectors_count) * ATA_SECTOR_SIZE);


    // read all the sectors individually, since they might not be continuous
    for (unsigned int i = 0; i < n_sectors + indirect_sectors_count;i++ ){
        // normal sectors
        if (i < NUM_DATA_SECTORS_PER_FILE){
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],start_node->data_sectors[i]);
        }
        //indirect sectors
        else{
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],indirect_sectors[i - NUM_DATA_SECTORS_PER_FILE]);            
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
            read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode_sector);
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
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0);
    last_read_sector_idx = 0;

    init_vector(&inodes);
    init_vector(&inode_name_pairs);

    inode_t* root_node = (inode_t*)kmalloc(sizeof(inode_t));

    // standard root dir stuff
    root_node->id = 0;
    root_node->type = FS_TYPE_DIR;

    if(!(last_read_sector[ATA_SECTOR_SIZE - 1] == FS_SECTOR_0_INIT_FLAG)){
        first_time_fs_init = 1;
        memset(root_node->data_sectors,0x0,NUM_DATA_SECTORS_PER_FILE); // no data in the root dir right now
        root_node->size = 0;
        *(unsigned int*)&last_read_sector[0x0] = root_node->id;
        *(unsigned int*)&last_read_sector[0x4] = root_node->size;
        last_read_sector[0x8] = root_node->type;
        last_read_sector[0x9] = 0;
        last_read_sector[0xa] = 0;
        last_read_sector[0xb] = 0;
        *(unsigned int*)&last_read_sector[0xc] = 0;
        memset(&last_read_sector[0x10],0x0,NUM_DATA_SECTORS_PER_FILE);
        last_read_sector[ATA_SECTOR_SIZE - 1] = FS_SECTOR_0_INIT_FLAG;
        write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0); 

        memset(header_data.big_sector_used_map,0,BIG_SECTOR_BITMAP_SIZE);
        memset(header_data.big_sector_full_map,0,BIG_SECTOR_BITMAP_SIZE);

        // reserve the reserved sectors
        for (unsigned int i = 0; i < RESERVED_SECTORS;i++){
            allocate_sector();
        }
        
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
        memcpy(root_node->data_sectors,last_read_sector[0x10],NUM_DATA_SECTORS_PER_FILE);
        
        read_bitmaps();

    }

    vector_append(&inodes,(unsigned int)root_node);

    build_inodes(root_node,root_node->id);
}

void create_directory(inode_t* parent_dir){

}