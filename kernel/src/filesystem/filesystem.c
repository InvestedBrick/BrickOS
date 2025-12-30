#include "filesystem.h"
#include "../drivers/ATA_PIO/ata.h"
#include "../io/log.h"
#include "../utilities/util.h"
#include "../memory/kmalloc.h"
#include "file_operations.h"
#include "../screen/screen.h"
#include "../utilities/vector.h"
#include "devices/devs.h"
#include "IPC/pipes.h"
#include "../processes/user_process.h"
vector_t inodes;
vector_t inode_name_pairs; // Note to self: do not free this with vector free, the names have to be freed too
sectors_headerdata_t header_data; 
inode_t* active_dir = 0;
uint32_t active_partially_used_bitmaps;

uint8_t first_time_fs_init = 0;

unsigned char* last_read_sector;
uint32_t last_read_sector_idx;

inode_t* get_inode_by_path(unsigned char* path){
    inode_t* file;

    uint32_t first_name_end_idx = find_char(path,'/');

    uint32_t alloc_size = first_name_end_idx == (uint32_t)-1 ? strlen(path) : first_name_end_idx; 
    unsigned char* first_name = (unsigned char*)kmalloc(alloc_size + 1); // + '\0'
    memcpy(first_name,path,alloc_size);
    first_name[alloc_size] = '\0';

    if (dir_contains_name(active_dir,first_name) || strneq(path,"..",2,2)){
        file = get_inode_by_relative_file_path(path);
    }else{
        file = get_inode_by_full_file_path(path);
    }

    kfree(first_name);

    return file;
}

inode_t* change_active_dir(inode_t* new_dir){
    inode_t* dir_save = active_dir;
    active_dir = new_dir;
    return dir_save;
}

inode_name_pair_t* get_name_by_inode_id(uint32_t id){
    for(uint32_t i = 0; i < inode_name_pairs.size;i++){
        inode_name_pair_t* pair = (inode_name_pair_t*)inode_name_pairs.data[i];
        if (pair->id == id){
            return pair;
        }
    }
    return 0;
}

uint32_t get_inode_id_by_name(uint32_t parent_id, unsigned char* name){
    uint32_t len = strlen(name);
    for (uint32_t i = 0; i < inode_name_pairs.size; i++) {
        inode_name_pair_t* pair = (inode_name_pair_t*)inode_name_pairs.data[i];
        if (pair->parent_id == parent_id && strneq(pair->name, name,len,pair->length)) {
            return pair->id;
        }
    }
    return (uint32_t)-1;
}

uint8_t dir_contains_name(inode_t* dir,unsigned char* name){
    string_array_t* str_arr = get_all_names_in_dir(dir);
    
    uint8_t found = 0;
    for(uint32_t i = 0; i < str_arr->n_strings;i++){
        if (streq(str_arr->strings[i].str,name)){
            found = 1;
            break;
        }
    }
    free_string_arr(str_arr);
    return found;
}

inode_t* get_inode_by_id(uint32_t id){
    for(uint32_t i = 0; i < inodes.size;i++){
        inode_t* node = (inode_t*)inodes.data[i];
        if(node->id == id){
            return node;
        }
    }

    return 0;
}
string_t get_active_dir(){
    inode_name_pair_t* pair = get_name_by_inode_id(active_dir->id);
    string_t str;
    str.str = pair->name;
    str.length = pair->length;
    return str;
}

inode_t* get_parent_inode(inode_t* child){
    inode_name_pair_t* name_pair = get_name_by_inode_id(child->id);

    return get_inode_by_id(name_pair->parent_id);
}

inode_t* get_inode_by_file_path(unsigned char* path,uint32_t inode_id){
    
    inode_t* inode = get_inode_by_id(inode_id);
    unsigned char name_buffer[256 + 1]; // max len of dir name is max of unsigned char + 1 for null terminator
    
    uint32_t path_idx = 0;
    uint32_t buffer_idx;
    
    while(path[path_idx] != '\0'){
        
        memset(name_buffer,0x00,256 + 1);
        buffer_idx = 0;
        while(buffer_idx <= 256 && path[path_idx] != '/' && path[path_idx] != '\0'){
            name_buffer[buffer_idx++] = path[path_idx++];
        }
        if (path[path_idx] == '/') path_idx++;
        
        name_buffer[buffer_idx] = '\0';

        if (streq(name_buffer,"..")){
            inode = get_parent_inode(inode);
            continue;
        }

        uint32_t id = get_inode_id_by_name(inode->id,name_buffer);
        if (id == (uint32_t)-1) return 0;

        inode = get_inode_by_id(id);
        if (!inode) return 0;

    }

    return inode;
}

inode_t* get_inode_by_full_file_path(unsigned char* path) {
    return get_inode_by_file_path(path,FS_ROOT_DIR_ID);
}

inode_t* get_inode_by_relative_file_path(unsigned char* path){
    return get_inode_by_file_path(path,active_dir->id);
}

uint32_t count_partially_used_big_sectors(uint32_t end){
    uint32_t partially_used_bitmaps = 0;
    for(uint32_t i = 0; i < end;i++){
        for(uint32_t j = 0; j < 8;j++){
            // check whether the big sector is partially used, if so, a bitmap for it should exist
            if (BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) &&
               !BIT_CHECK(header_data.big_sector_full_map,i * 8 + j)) partially_used_bitmaps++;
        }
    }

    return partially_used_bitmaps;
}

uint8_t is_bitmap_clear(unsigned char* bitmap, uint32_t byte_size){
    for (uint32_t i = 0; i < byte_size; i++){
        if (bitmap[i] != 0x00) return 0;
    }

    return 1;
}

uint8_t is_bitmap_full(unsigned char* bitmap, uint32_t byte_size){
    for (uint32_t i = 0; i < byte_size; i++){
        if (bitmap[i] != 0xff) return 0;
    }

    return 1;
}

void insert_sector_bitmap(unsigned char** bitmaps,unsigned char* new_map,uint32_t pos, uint32_t n_active_bitmaps){
    for (uint32_t i = n_active_bitmaps + 1; i > pos;i--){
        bitmaps[i] = bitmaps[i - 1];
    }
    bitmaps[pos] = new_map;
}

void shift_sector_bitmaps_left(unsigned char** bitmaps, uint32_t start, uint32_t n_active_bitmaps){
    for (uint32_t i = start; i < n_active_bitmaps - 1; i++){
        bitmaps[i] = bitmaps[i + 1];
    }
    bitmaps[n_active_bitmaps - 1] = 0;
}

void read_bitmaps_from_disk(){
    // bitmap headerdata starts at sector 101 (BITMAP_SECTOR_START)
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,BITMAP_SECTOR_START);
    last_read_sector_idx = BITMAP_SECTOR_START;

    memcpy(header_data.big_sector_used_map,last_read_sector,BIG_SECTOR_BITMAP_SIZE); // read in used bitmap
    memcpy(header_data.big_sector_full_map,&last_read_sector[BIG_SECTOR_BITMAP_SIZE],BIG_SECTOR_BITMAP_SIZE); // read in fully used bitmap
    memcpy(header_data.inode_sector_map,&last_read_sector[BIG_SECTOR_BITMAP_SIZE * 2],RESERVED_INODE_SECTORS);

    uint32_t partially_used_bitmaps = count_partially_used_big_sectors(BIG_SECTOR_BITMAP_SIZE);
    if (partially_used_bitmaps > 128) panic("Not enough reserved sectors for bitmaps");
    active_partially_used_bitmaps = partially_used_bitmaps;

    for (uint32_t i = 0; i < partially_used_bitmaps;i++){
        uint32_t sector_bitmap_idx = (BITMAP_SECTOR_START + 1) + (i / SECTOR_BITMAPS_PER_SECTOR);  
        uint32_t bitmap_idx_in_sector = (i % SECTOR_BITMAPS_PER_SECTOR);

        if (last_read_sector_idx != sector_bitmap_idx){
            read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector_bitmap_idx);
            last_read_sector_idx = sector_bitmap_idx;
        }

        header_data.sector_bitmaps[i] = (unsigned char*)kmalloc(SECTOR_BITMAP_SIZE);

        // read in the desired sector bitmap
        memcpy(header_data.sector_bitmaps[i],&last_read_sector[bitmap_idx_in_sector * SECTOR_BITMAP_SIZE],SECTOR_BITMAP_SIZE);
    }

}

void write_bitmaps_to_disk() {

    uint8_t sector[512] = {0};
    memcpy(sector,header_data.big_sector_used_map,BIG_SECTOR_BITMAP_SIZE);
    memcpy(&sector[BIG_SECTOR_BITMAP_SIZE],header_data.big_sector_full_map,BIG_SECTOR_BITMAP_SIZE);
    memcpy(&sector[BIG_SECTOR_BITMAP_SIZE * 2],header_data.inode_sector_map,RESERVED_INODE_SECTORS);
    write_sectors(ATA_PRIMARY_BUS_IO,1,sector,BITMAP_SECTOR_START);

    unsigned char* bitmaps = (unsigned char*)kmalloc(active_partially_used_bitmaps * SECTOR_BITMAP_SIZE);
    for (uint32_t i = 0; i < active_partially_used_bitmaps;i++){
        memcpy(&bitmaps[i * SECTOR_BITMAP_SIZE],header_data.sector_bitmaps[i],SECTOR_BITMAP_SIZE);
    }

    write_sectors(ATA_PRIMARY_BUS_IO,CEIL_DIV(active_partially_used_bitmaps, 8),bitmaps,(BITMAP_SECTOR_START + 1));

    kfree(bitmaps);
}

void free_sector(uint32_t sector_idx) {
    uint32_t big_sector_idx = sector_idx / BIG_SECTOR_SECTOR_COUNT;
    uint32_t big_sector_sector_idx = sector_idx % BIG_SECTOR_SECTOR_COUNT;

    if (!BIT_CHECK(header_data.big_sector_used_map,big_sector_idx)) return;
    if (big_sector_idx >= TOTAL_BIG_SECTORS || big_sector_sector_idx >= BIG_SECTOR_SECTOR_COUNT) return;

    uint32_t bitmap_idx = count_partially_used_big_sectors(big_sector_idx);

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

uint32_t allocate_sector(){

    uint32_t partially_free_big_sector_idx = (uint32_t)-1;
    uint32_t first_free_big_sector_idx = (uint32_t)-1;

    uint8_t break_loop = 0; 
    uint8_t free_sector_found = 0;
    for (uint32_t i = 0; i < 128;i++){

        for (uint32_t j = 0; j < 8;j++){
            
            // partially used big sector
            if (BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) &&
               !BIT_CHECK(header_data.big_sector_full_map,i * 8 + j)) {partially_free_big_sector_idx = i * 8 + j;break_loop = 1;break;}

            if (!BIT_CHECK(header_data.big_sector_used_map,i * 8 + j) && !free_sector_found) {first_free_big_sector_idx = i * 8 + j; free_sector_found = 1;}
        }
        if (break_loop) break;
    }

    // there was no partially free sector found
    if (partially_free_big_sector_idx == (uint32_t)-1){

        if (first_free_big_sector_idx == (uint32_t)-1) panic("No free big sector exists");

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

        uint32_t free_sector_idx = (uint32_t)-1;

        for (uint32_t i = 0; i < SECTOR_BITMAP_SIZE; i++){
            for (uint32_t j = 0; j < 8; j++){
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
    return (uint32_t)-1;
}

uint32_t allocate_inode_section(){
    for (uint32_t i = 0; i < RESERVED_INODE_SECTORS;i++){
        for (uint32_t j = 0; j < 8; j++){
            if (!BIT_CHECK(header_data.inode_sector_map,i * 8 + j)){
                BIT_SET(header_data.inode_sector_map,i * 8 + j);
                return i * 8 + j;
            }
        }
    }
    
    panic("No free inode sector left");
    return (uint32_t)-1;
}

void free_inode_section(uint32_t inode_id){
    if (!BIT_CHECK(header_data.inode_sector_map,inode_id)) return;

    if (inode_id <= 0 || inode_id >= RESERVED_INODE_SECTORS * 8) return;

    BIT_CLEAR(header_data.inode_sector_map,inode_id);
}

unsigned char* read_dir_entries_to_buffer(inode_t* dir){
    uint32_t n_indirect_sectors = 0;
    uint32_t n_sectors = 0;

    if (dir->indirect_sector){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,dir->indirect_sector);
        last_read_sector_idx = dir->indirect_sector;

        for(uint32_t i = 0; *(uint32_t*)&last_read_sector[i * sizeof(uint32_t)] != 0 && i < ATA_SECTOR_SIZE / sizeof(uint32_t);i++){
            n_indirect_sectors++;
        }
    }

    for (uint32_t sec = 0; dir->data_sectors[sec] != 0 && sec < NUM_DATA_SECTORS_PER_FILE;sec++){n_sectors++;}

    if ((n_sectors + n_indirect_sectors) == 0) return 0;

    unsigned char* buffer = (char*)kmalloc((n_sectors + n_indirect_sectors) * ATA_SECTOR_SIZE);
    // read all the sectors individually, since they might not be continuous
    for (uint32_t i = 0; i < n_sectors + n_indirect_sectors;i++ ){
        // normal sectors
        if (i < NUM_DATA_SECTORS_PER_FILE){
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],dir->data_sectors[i]);
        }
        //indirect sectors
        else{
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],*(uint32_t*)&last_read_sector[sizeof(uint32_t) * (i - NUM_DATA_SECTORS_PER_FILE)]);            
        }
    }

    return buffer;

}

void build_inodes(inode_t* parent){
    /**
     * Directory entry:
     *               [n_entries (4 bytes)| inode id (4 bytes) | name len (1 byte) | name (name len bytes) | inode id (4 bytes) | name len (1 byte) | name (name len bytes) | ...] 
     */
    if (parent->type != FS_TYPE_DIR) return;

    
    unsigned char* buffer = read_dir_entries_to_buffer(parent);
    if (!buffer) return;
    
    //parse the sectors
    uint32_t n_entries = *(uint32_t*)(&buffer[0]);
    uint32_t buffer_idx = sizeof(uint32_t);
    for (uint32_t i = 0; i < n_entries;i++){
        uint32_t node_id = *(uint32_t*)&buffer[buffer_idx];
        buffer_idx += sizeof(uint32_t);

        uint8_t name_len = buffer[buffer_idx];

        // save the name
        inode_name_pair_t* pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
        pair->id = node_id;
        pair->length = name_len;
        pair->parent_id = parent->id;
        pair->name = (unsigned char*)kmalloc(name_len + 1);
        memcpy(pair->name,&buffer[buffer_idx + 1],name_len);
        pair->name[name_len] = 0;

        vector_append(&inode_name_pairs,(vector_data_t)pair);

        buffer_idx += name_len + sizeof(uint8_t);

        uint32_t inode_sector =  (node_id / FS_INODES_PER_SECTOR) + 1; // rounded towards 0 and +1 for the root sector
        uint32_t sector_offset = (node_id % FS_INODES_PER_SECTOR) * sizeof(inode_t);        


        inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));
        
        if (last_read_sector_idx != inode_sector){
            read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode_sector);
            last_read_sector_idx = inode_sector;
        }

        // copy the data from disk to memory
        memcpy((void*)inode,&last_read_sector[sector_offset],sizeof(inode_t));

        vector_append(&inodes,(vector_data_t)inode);
        build_inodes(inode); 
    }

    // We are nice today and return our memory
    kfree(buffer);

}

void init_filesystem(){
    // Check whether the disk driver has been set up
    if (!addressable_LBA28_sectors) {error("Disk driver has not been set up");return;}

    last_read_sector = (unsigned char*)kmalloc(ATA_SECTOR_SIZE);

    // read the first sector 
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0);
    last_read_sector_idx = 0;

    init_vector(&inodes);
    init_vector(&inode_name_pairs);
    init_dev_vec();
    init_pipe_vec();
    
    inode_t* root_node = (inode_t*)kmalloc(sizeof(inode_t));
    
    // standard root dir stuff
    root_node->id = FS_ROOT_DIR_ID;
    root_node->type = FS_TYPE_DIR;
    root_node->priv_lvl = PRIV_STD;
    if(!(last_read_sector[ATA_SECTOR_SIZE - 1] == FS_SECTOR_0_INIT_FLAG)){
        first_time_fs_init = 1;
        memset(root_node->data_sectors,0x0,NUM_DATA_SECTORS_PER_FILE * sizeof(uint32_t)); // no data in the root dir right now
        root_node->size = 0;
        *(uint32_t*)&last_read_sector[0x0] = root_node->id;
        *(uint32_t*)&last_read_sector[sizeof(uint32_t)] = root_node->size;
        last_read_sector[sizeof(uint32_t) * 2] = root_node->type;
        last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t)] = root_node->priv_lvl;
        last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2] = 0;
        last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t) * 3] = 0;
        *(uint32_t*)&last_read_sector[sizeof(uint32_t) * 3] = 0;
        memset(&last_read_sector[sizeof(uint32_t) * 4],0x0,NUM_DATA_SECTORS_PER_FILE * sizeof(uint32_t));
        last_read_sector[ATA_SECTOR_SIZE - 1] = FS_SECTOR_0_INIT_FLAG;
        write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0); 
        
        memset(header_data.big_sector_used_map,0,BIG_SECTOR_BITMAP_SIZE);
        memset(header_data.big_sector_full_map,0,BIG_SECTOR_BITMAP_SIZE);
        
        // reserve the reserved sectors
        for (uint32_t i = 0; i < RESERVED_SECTORS;i++){
            allocate_sector();
        }
    }else{
        /**
         * 0x00000000: root_id
         * 0x00000004: root_size
         * 0x00000008: root_type
         * 0x00000009: perms
         * 0x0000000a: priv_lvl
         * 0x0000000b: flags 3
         * 0x0000000c: indirect_sector
         * 0x00000010: root_data_sectors
         */
        root_node->id = *(uint32_t*)&last_read_sector[0x00];
        root_node->size = *(uint32_t*)&last_read_sector[sizeof(uint32_t)];
        root_node->type = last_read_sector[sizeof(uint32_t) * 2];
        root_node->perms = last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t)];
        root_node->priv_lvl = last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2];
        root_node->unused_flag_three = last_read_sector[sizeof(uint32_t) * 2 + sizeof(uint8_t) * 3];
        root_node->indirect_sector = *(uint32_t*)&last_read_sector[sizeof(uint32_t) * 3];
        memcpy(root_node->data_sectors,&last_read_sector[sizeof(uint32_t) * 4],NUM_DATA_SECTORS_PER_FILE * sizeof(uint32_t));
        read_bitmaps_from_disk();
    }
    
    active_dir = root_node;
    
    inode_name_pair_t* name_pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
    name_pair->length = 4;
    name_pair->id = FS_ROOT_DIR_ID;
    name_pair->parent_id = FS_ROOT_DIR_ID;
    name_pair->name = kmalloc(sizeof(unsigned char) * 5);
    memcpy(name_pair->name,"root",sizeof(unsigned char) * 5);
    vector_append(&inode_name_pairs,(vector_data_t)name_pair);
    
    vector_append(&inodes,(vector_data_t)root_node);
    
    build_inodes(root_node);
}

uint8_t write_directory_entry(inode_t* parent_dir, uint32_t child_inode_id, unsigned char* name, uint8_t name_length){
    uint8_t data_buffer[sizeof(uint32_t) + sizeof(uint8_t) + 256];
    uint32_t data_buffer_size = sizeof(uint32_t) + sizeof(uint8_t) + name_length;

    // copy our data into the byte datastream buffer
    *(uint32_t*)&data_buffer[0] = child_inode_id;
    data_buffer[sizeof(uint32_t)] = name_length;
    memcpy(&data_buffer[sizeof(uint32_t) + sizeof(uint8_t)],name,name_length);

    uint32_t total_bytes = parent_dir->size;
    uint32_t sector_idx = total_bytes / ATA_SECTOR_SIZE;
    uint32_t sector_offset = total_bytes % ATA_SECTOR_SIZE;

    if (sector_idx >= NUM_DATA_SECTORS_PER_FILE) return 0;

    if(parent_dir->data_sectors[sector_idx] == 0){
        parent_dir->data_sectors[sector_idx] = allocate_sector();
    }

    uint32_t sector = parent_dir->data_sectors[sector_idx];

    if (last_read_sector_idx != sector){
        read_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, sector);
        last_read_sector_idx = sector;
    }

    uint32_t bytes_written = 0;
    uint32_t remaining_bytes_in_sector = ATA_SECTOR_SIZE - sector_offset;
    
    while(bytes_written < data_buffer_size){

        uint32_t to_write = data_buffer_size - bytes_written;
        if(to_write > remaining_bytes_in_sector) to_write = remaining_bytes_in_sector;

        memcpy(&last_read_sector[sector_offset],&data_buffer[bytes_written],to_write);
        write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector);

        bytes_written += to_write;
        parent_dir->size += to_write;

        if (bytes_written == data_buffer_size) break;

        sector_idx++;
        if (sector_idx >= NUM_DATA_SECTORS_PER_FILE) return 0;
        if (parent_dir->data_sectors[sector_idx] == 0) {
            parent_dir->data_sectors[sector_idx] = allocate_sector();
        }
        sector = parent_dir->data_sectors[sector_idx];
        read_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, sector);
        last_read_sector_idx = sector;

        sector_offset = 0;
        remaining_bytes_in_sector = ATA_SECTOR_SIZE;
    }

    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,parent_dir->data_sectors[0]);
    last_read_sector_idx = parent_dir->data_sectors[0];
    *(uint32_t*)&last_read_sector[0] = *(uint32_t*)&last_read_sector[0] + 1; // increment the number of entries
    write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,parent_dir->data_sectors[0]); 

    return 1;
}

string_array_t* get_all_names_in_dir(inode_t* dir){

    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));

    // assure that size was set
    if (dir->size < sizeof(uint32_t)) {
        str_arr->n_strings = 0;
        return str_arr;
    }
    
    unsigned char* buffer = read_dir_entries_to_buffer(dir);
    if (!buffer) {
        str_arr->n_strings = 0;
        return str_arr;
    }
    
    uint32_t n_strings = *(uint32_t*)&buffer[0];
    string_t* strings = (string_t*)kmalloc(sizeof(string_t) * n_strings);
    
    uint32_t buffer_idx = sizeof(uint32_t);
    for (uint32_t i = 0; i < n_strings;i++){
        uint32_t id = *(uint32_t*)&buffer[buffer_idx];
        buffer_idx += sizeof(uint32_t); // skip id
        uint8_t raw_len = buffer[buffer_idx++];
        uint32_t str_len = raw_len; 
        strings[i].length = str_len;
        strings[i].str = (unsigned char*)kmalloc(str_len + 1);
        strings[i].str[str_len] = 0;
        memcpy(strings[i].str,&buffer[buffer_idx],str_len);
        buffer_idx += raw_len; 
    }

    
    str_arr->n_strings = n_strings;
    str_arr->strings = strings;
    kfree(buffer);

    return str_arr;

}

int write_data_buffer_to_disk(inode_t* inode, unsigned char* buffer, uint32_t buffer_size){
    
    if (!inode) return INTERNAL_FUNCTION_FAIL;
    if (!buffer) return INTERNAL_FUNCTION_FAIL;

    uint32_t buffer_index = 0;
    uint32_t bytes_written = 0;

    while(bytes_written < buffer_size){
        uint32_t sector_idx = bytes_written / ATA_SECTOR_SIZE;
        
        if (sector_idx >= MAX_FILE_SECTORS) return INTERNAL_FUNCTION_FAIL;

        uint32_t sector;

        if(sector_idx >= FS_INODES_PER_SECTOR){

            if (!inode->indirect_sector) return INTERNAL_FUNCTION_FAIL;
            if(last_read_sector_idx != inode->indirect_sector){
                read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode->indirect_sector);
                last_read_sector_idx = inode->indirect_sector;
            }

            sector = *(uint32_t*)&last_read_sector[sizeof(uint32_t) * (sector_idx - FS_INODES_PER_SECTOR )];
        }else{
            sector = inode->data_sectors[sector_idx];
        }
        if (!sector) break; // should only happend when removing last item from a dir
        write_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[buffer_index],sector);
        buffer_index += ATA_SECTOR_SIZE;
        uint32_t remaining = buffer_size - bytes_written;
        bytes_written += (remaining >= ATA_SECTOR_SIZE) ? ATA_SECTOR_SIZE : remaining;

    }

    return INTERNAL_FUNCTION_SUCCESS;
    
}

void erase_directory_entry(inode_t* dir, uint32_t entry_inode_id){
    uint32_t start_idx = 0;
    uint32_t data_len = 0;
    uint32_t n_entries = 0;
    
    if (!dir->data_sectors[0]) return; // nothing there so nothing to remove

    unsigned char* buffer = read_dir_entries_to_buffer(dir);

    n_entries = *(uint32_t*)&buffer[0];

    uint32_t buffer_offset = sizeof(uint32_t);
    for (uint32_t i = 0; i < n_entries;i++){
        uint32_t id = *(uint32_t*)&buffer[buffer_offset];
        if (id == entry_inode_id) {
            start_idx = buffer_offset;
            data_len = buffer[buffer_offset + sizeof(uint32_t)] + sizeof(uint8_t) + sizeof(uint32_t);
        }
        buffer_offset += sizeof(uint32_t);
        uint32_t len = buffer[buffer_offset];
        buffer_offset += sizeof(uint8_t) + len;
    }

    memmove(&buffer[start_idx],&buffer[start_idx + data_len],buffer_offset - data_len - start_idx);
    memset(&buffer[buffer_offset - data_len],0x00,data_len);
    n_entries--;
    *(uint32_t*)&buffer[0] = n_entries;
    dir->size -= data_len;

    // test if we should free the sector and if it is the first sector (now empty dir) also account for the 4 byte header
    uint32_t first_sector_header = dir->data_sectors[1] == 0 ? sizeof(int) : 0;
    uint8_t last_sector_free = ((buffer_offset - first_sector_header) % ATA_SECTOR_SIZE) <= data_len;
    if (last_sector_free){
        uint32_t sector_idx = buffer_offset / ATA_SECTOR_SIZE;
        uint32_t sector = dir->data_sectors[sector_idx];
        free_sector(sector);
        dir->data_sectors[sector_idx] = 0;

        if (first_sector_header) dir->size = 0; // directory has no entries now
    }
    if (write_data_buffer_to_disk(dir,buffer,buffer_offset) != INTERNAL_FUNCTION_SUCCESS){
        error("Writing data to sector failed");
    }
    kfree(buffer);
    
}

inode_t* get_file_if_exists(inode_t* parent_dir,unsigned char* name){
    
    if(dir_contains_name(parent_dir,name)){
        return get_inode_by_id(get_inode_id_by_name(parent_dir->id,name));
    }else{
        return get_inode_by_full_file_path(name);
    }
}

int erase_empty_dir(inode_t* parent_dir, inode_t* dir){
    dir->type = FS_TYPE_ERASED;
    return delete_file_by_inode(parent_dir,dir);
}

int delete_dir_recursive(inode_t* parent_dir, inode_t* dir){
    string_array_t* names = get_all_names_in_dir(dir);
    if (names->n_strings == 0) {
        if (erase_empty_dir(parent_dir,dir) != FS_FILE_DELETION_SUCCESS) 
            return FS_FILE_DELETION_FAILED;
        return FS_FILE_DELETION_SUCCESS;
    }
    for(uint32_t i = 0; i < names->n_strings;i++){
        uint32_t id = get_inode_id_by_name(dir->id,names->strings[i].str);
        inode_t* inode = get_inode_by_id(id);
        if (!inode) return FILE_INVALID_FD;
        int result;
        if (inode->type != FS_TYPE_DIR) {
            result = delete_file_by_inode(dir,inode);
        }else{
            result = delete_dir_recursive(dir,inode);
        }

        if (result != FS_FILE_DELETION_SUCCESS) return FS_FILE_DELETION_FAILED;
    }
    free_string_arr(names);
    if (erase_empty_dir(parent_dir,dir) != FS_FILE_DELETION_SUCCESS) 
        return FS_FILE_DELETION_FAILED;
    
    return FS_FILE_DELETION_SUCCESS;
}

int delete_dir(inode_t* parent_dir,unsigned char* name){
    // get dir
    inode_t* dir = get_file_if_exists(parent_dir,name);
    if (!dir) return FS_FILE_NOT_FOUND;
    
    if (dir->type != FS_TYPE_DIR) return FILE_INVALID_TARGET;
    

    return delete_dir_recursive(parent_dir,dir);
}

int delete_file_by_inode(inode_t* parent_dir,inode_t* inode){

    if (inode->type == FS_TYPE_DIR && inode->data_sectors[0] != 0)
        return FS_FILE_DELETION_FAILED; // cant remote a non-empty dir

    if (inode->indirect_sector){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode->indirect_sector);
        last_read_sector_idx = inode->indirect_sector;

        for(uint32_t i = 0;i < ATA_SECTOR_SIZE / sizeof(uint32_t);i++){
            uint32_t sector = *(uint32_t*)&last_read_sector[i * sizeof(uint32_t)];
            if (sector) free_sector(sector);
        }
        free_sector(inode->indirect_sector);
    }

    for (uint32_t i = 0; i < NUM_DATA_SECTORS_PER_FILE;i++){
        uint32_t data_sector = inode->data_sectors[i];
        if(data_sector) free_sector(data_sector); else break;
        
    }

    erase_directory_entry(parent_dir,inode->id);
    
    inode_name_pair_t* pair = get_name_by_inode_id(inode->id);
    vector_erase_item(&inode_name_pairs,(uint64_t)pair);
    kfree(pair->name);
    kfree(pair);

    free_inode_section(inode->id);
    vector_erase_item(&inodes,(uint64_t)inode);
    kfree(inode);

    return FS_FILE_DELETION_SUCCESS;
}

int create_file(inode_t* parent_dir, unsigned char* name, uint8_t name_length, uint8_t type,uint8_t perms,uint8_t priv_lvl){

    string_array_t* strs_in_parent_dir = get_all_names_in_dir(parent_dir);
    
    for (uint32_t i = 0; i < strs_in_parent_dir->n_strings;i++){
        if(strneq(name,strs_in_parent_dir->strings[i].str,name_length,strs_in_parent_dir->strings[i].length)) {
            free_string_arr(strs_in_parent_dir);
            return FS_FILE_ALREADY_EXISTS;
        }
    }
    free_string_arr(strs_in_parent_dir);

    if (parent_dir->data_sectors[0] == 0){
        parent_dir->data_sectors[0] = allocate_sector();
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,parent_dir->data_sectors[0]);
        last_read_sector_idx = parent_dir->data_sectors[0];
        memset(last_read_sector, 0, ATA_SECTOR_SIZE); 
        *(uint32_t*)&last_read_sector[0] = 0; 
        write_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, parent_dir->data_sectors[0]);
        parent_dir->size = sizeof(uint32_t);
    } 
    
    
    inode_t* file = (inode_t*)kmalloc(sizeof(inode_t));
    file->perms = perms;
    file->type = type;
    file->id = allocate_inode_section();
    memset(file->data_sectors,0,NUM_DATA_SECTORS_PER_FILE * sizeof(uint32_t));
    file->indirect_sector = 0;
    file->size = 0;
    file->priv_lvl = priv_lvl;
    vector_append(&inodes,(vector_data_t)file);

    if (!write_directory_entry(parent_dir,file->id,name,name_length)) {
        return FS_FILE_CREATION_FAILED;
    }

    // cache to memory, TODO: gets cleared automatically after some time
    inode_name_pair_t* name_pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
    name_pair->id = file->id;
    name_pair->parent_id = parent_dir->id;
    name_pair->length = name_length;
    name_pair->name = kmalloc(name_length + 1);
    memcpy(name_pair->name,name,name_length);
    name_pair->name[name_length] = 0;
    vector_append(&inode_name_pairs,(vector_data_t)name_pair);

    return FS_FILE_CREATION_SUCCESS;
}

void change_file_permissions(unsigned char* filename,uint8_t perms){
    inode_t* file = get_inode_by_path(filename);

    if (!file) return;


    if (file->type == FS_TYPE_DIR) return;
    file->perms = perms; 
}

void change_file_priviledge_level(unsigned char* filename,uint8_t priv){
    inode_t* file = get_inode_by_path(filename);
    if (!file) return;

    user_process_t* curr_proc = get_current_user_process();

    if (priv < curr_proc->priv_lvl) return;

    file->priv_lvl = priv;
}

void write_inode_to_disk(inode_t* inode){
    uint32_t sector_idx = (inode->id / FS_INODES_PER_SECTOR) + 1; 

    uint32_t sector_start = (inode->id % FS_INODES_PER_SECTOR) * SECTOR_BITMAP_SIZE;
    if (inode->id == FS_ROOT_DIR_ID){
        sector_idx = 0;
        sector_start = 0;
    }
    if (last_read_sector_idx != sector_idx){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector_idx);
        last_read_sector_idx = sector_idx;
    }

    *(uint32_t*)&last_read_sector[sector_start] = inode->id;
    *(uint32_t*)&last_read_sector[sector_start + sizeof(uint32_t)] = inode->size;
    last_read_sector[sector_start + sizeof(uint32_t) * 2] = inode->type;
    last_read_sector[sector_start + sizeof(uint32_t) * 2 + sizeof(uint8_t)] = inode->perms;
    last_read_sector[sector_start + sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2] = inode->priv_lvl;
    last_read_sector[sector_start + sizeof(uint32_t) * 2 + sizeof(uint8_t) * 3] = 0x0;
    *(uint32_t*)&last_read_sector[sector_start + sizeof(uint32_t) * 3] = inode->indirect_sector;
    memcpy(&last_read_sector[sector_start + sizeof(uint32_t) * 4],inode->data_sectors,sizeof(uint32_t) * NUM_DATA_SECTORS_PER_FILE);

    write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector_idx);
}

void write_to_disk(){
    for (uint32_t i = 0; i < inodes.size;i++){
        inode_t* inode = (inode_t*)inodes.data[i];
        write_inode_to_disk(inode);
    }
    log("Wrote all inodes to disk");
    write_bitmaps_to_disk();
    log("Wrote all bitmaps to disk");
}

void cleanup_tmp(){
    active_dir = get_inode_by_full_file_path("tmp/");

    string_array_t* names = get_all_names_in_dir(active_dir);

    for (uint32_t i = 0; i < names->n_strings;i++){
        inode_t* entry = get_inode_by_relative_file_path(names->strings[i].str);

        if (entry->type == TYPE_DIR) {
            if (delete_dir_recursive(active_dir,entry) < 0) error("Failed to delete dir in /tmp");
        }
        else {
            if (delete_file_by_inode(active_dir,entry) < 0) error("Failed to delete file in /tmp");
        }
    }

    free_string_arr(names);
    active_dir = get_inode_by_id(FS_ROOT_DIR_ID);
}