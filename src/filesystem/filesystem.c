#include "filesystem.h"
#include "../drivers/ATA_PIO/ata.h"
#include "../io/log.h"
#include "../util.h"
#include "../memory/kmalloc.h"
#include "file_operations.h"
#include "../screen.h"
vector_t inodes;
vector_t inode_name_pairs; // Note to self: do not free this with vector free, the names have to be freed too
sectors_headerdata_t header_data; 
inode_t* active_dir = 0;
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

unsigned int get_inode_id_by_name(unsigned int parent_id, unsigned char* name){
    unsigned int len = strlen(name);
    for (unsigned int i = 0; i < inode_name_pairs.size; i++) {
        inode_name_pair_t* pair = (inode_name_pair_t*)inode_name_pairs.data[i];
        if (pair->parent_id == parent_id && strneq(pair->name, name,len,pair->length)) {
            return pair->id;
        }
    }
    return (unsigned int)-1;
}

unsigned char dir_contains_name(inode_t* dir,unsigned char* name){
    string_array_t* str_arr = get_all_names_in_dir(dir,0);
    for(unsigned int i = 0; i < str_arr->n_strings;i++){
        if (streq(str_arr->strings[i].str,name)){
            return 1;
        }
    }
    return 0;
}

inode_t* get_inode_by_id(unsigned int id){
    for(unsigned int i = 0; i < inodes.size;i++){
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

inode_t* get_inode_by_full_file_path(unsigned char* path){
    unsigned char name_buffer[256 + 1]; // max len of dir name is max of unsigned char + 1 for null terminator
    
    unsigned int path_idx = 0;
    unsigned int buffer_idx;
    inode_t* inode = get_inode_by_id(FS_ROOT_DIR_ID);
    
    while(path[path_idx] != '\0'){
        memset(name_buffer,0x00,256 + 1);
        buffer_idx = 0;
        while(path[path_idx] != '/' && path[path_idx] != '\0'){
            name_buffer[buffer_idx++] = path[path_idx++];
        }
        path_idx++; //skip the newline

        unsigned int id = get_inode_id_by_name(inode->id,name_buffer);
        if (id == (unsigned int)-1) return 0;

        inode = get_inode_by_id(id);

    }

    return inode;
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

void read_bitmaps_from_disk(){
    // bitmap headerdata starts at sector 101 (BITMAP_SECTOR_START)
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,BITMAP_SECTOR_START);
    last_read_sector_idx = BITMAP_SECTOR_START;

    memcpy(header_data.big_sector_used_map,last_read_sector,BIG_SECTOR_BITMAP_SIZE); // read in used bitmap
    memcpy(header_data.big_sector_full_map,&last_read_sector[BIG_SECTOR_BITMAP_SIZE],BIG_SECTOR_BITMAP_SIZE); // read in fully used bitmap
    memcpy(header_data.inode_sector_map,&last_read_sector[BIG_SECTOR_BITMAP_SIZE * 2],RESERVED_INODE_SECTORS);

    unsigned int partially_used_bitmaps = count_partially_used_big_sectors(BIG_SECTOR_BITMAP_SIZE);
    if (partially_used_bitmaps > 128) panic("Not enough reserved sectors for bitmaps");
    active_partially_used_bitmaps = partially_used_bitmaps;

    for (unsigned int i = 0; i < partially_used_bitmaps;i++){
        unsigned int sector_bitmap_idx = (BITMAP_SECTOR_START + 1) + (i / SECTOR_BITMAPS_PER_SECTOR);  
        unsigned int bitmap_idx_in_sector = (i % SECTOR_BITMAPS_PER_SECTOR);

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

    unsigned char sector[512] = {0};
    memcpy(sector,header_data.big_sector_used_map,BIG_SECTOR_BITMAP_SIZE);
    memcpy(&sector[BIG_SECTOR_BITMAP_SIZE],header_data.big_sector_full_map,BIG_SECTOR_BITMAP_SIZE);
    memcpy(&sector[BIG_SECTOR_BITMAP_SIZE * 2],header_data.inode_sector_map,RESERVED_INODE_SECTORS);
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

unsigned int allocate_inode_section(){
    for (unsigned int i = 0; i < RESERVED_INODE_SECTORS;i++){
        for (unsigned int j = 0; j < 8; j++){
            if (!BIT_CHECK(header_data.inode_sector_map,i * 8 + j)){
                BIT_SET(header_data.inode_sector_map,i * 8 + j);
                return i * 8 + j;
            }
        }
    }
    
    panic("No free inode sector left");
    return (unsigned int)-1;
}

void free_inode_section(unsigned int inode_id){
    if (!BIT_CHECK(header_data.inode_sector_map,inode_id)) return;

    if (inode_id <= 0 || inode_id >= RESERVED_INODE_SECTORS * 8) return;

    BIT_CLEAR(header_data.inode_sector_map,inode_id);
}

unsigned char* read_dir_entries_to_buffer(inode_t* dir){
    unsigned int n_indirect_sectors = 0;
    unsigned int n_sectors = 0;

    if (dir->indirect_sector){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,dir->indirect_sector);
        last_read_sector_idx = dir->indirect_sector;

        for(unsigned int i = 0; *(unsigned int*)&last_read_sector[i * sizeof(unsigned int)] != 0 && i < ATA_SECTOR_SIZE / sizeof(unsigned int);i++){
            n_indirect_sectors++;
        }
    }

    for(unsigned int sec = 0; dir->data_sectors[sec] != 0 && sec < NUM_DATA_SECTORS_PER_FILE;sec++){n_sectors++;}
    if ((n_sectors + n_indirect_sectors) == 0) return 0;

    unsigned char* buffer = (char*)kmalloc((n_sectors + n_indirect_sectors) * ATA_SECTOR_SIZE);
    // read all the sectors individually, since they might not be continuous
    for (unsigned int i = 0; i < n_sectors + n_indirect_sectors;i++ ){
        // normal sectors
        if (i < NUM_DATA_SECTORS_PER_FILE){
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],dir->data_sectors[i]);
        }
        //indirect sectors
        else{
            read_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[i * ATA_SECTOR_SIZE],*(unsigned int*)&last_read_sector[sizeof(unsigned int) * (i - NUM_DATA_SECTORS_PER_FILE)]);            
        }
    }

    return buffer;

}

void build_inodes(inode_t* start_node,unsigned int parent_id){
    /**
     * Directory entry:
     *               [n_entries (4 bytes)| inode id (4 bytes) | name len (1 byte) | name (name len bytes) | inode id (4 bytes) | name len (1 byte) | name (name len bytes) | ...] 
     */
    if (start_node->type == FS_TYPE_FILE) return;


    unsigned char* buffer = read_dir_entries_to_buffer(start_node);
    if (!buffer) return;

    //parse the sectors
    unsigned int n_entries = *(unsigned int*)(&buffer[0]);
    unsigned int buffer_idx = sizeof(unsigned int);
    for (unsigned int i = 0; i < n_entries;i++){
        unsigned int node_id = *(unsigned int*)&buffer[buffer_idx];
        buffer_idx += sizeof(unsigned int);

        unsigned char name_len = buffer[buffer_idx];

        // save the name
        inode_name_pair_t* pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
        pair->id = node_id;
        pair->length = name_len;
        pair->parent_id = parent_id;
        pair->name = (unsigned char*)kmalloc(name_len);
        memcpy(pair->name,&buffer[buffer_idx + 1],name_len);
        pair->name[name_len] = 0;
        vector_append(&inode_name_pairs,(unsigned int)pair);

        buffer_idx += name_len + sizeof(unsigned char);

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


}

void init_filesystem(){
    // Check whether the disk driver has been set up
    if (!addressable_LBA28_sectors) {error("Disk driver has not been set up");return;}

    last_read_sector = kmalloc(ATA_SECTOR_SIZE);

    // read the first sector 
    read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,0);
    last_read_sector_idx = 0;

    init_vector(&inodes);
    init_vector(&inode_name_pairs);
    init_vector(&fd_vector);
    
    inode_t* root_node = (inode_t*)kmalloc(sizeof(inode_t));
    
    // standard root dir stuff
    root_node->id = FS_ROOT_DIR_ID;
    root_node->type = FS_TYPE_DIR;
    if(!(last_read_sector[ATA_SECTOR_SIZE - 1] == FS_SECTOR_0_INIT_FLAG)){
        first_time_fs_init = 1;
        memset(root_node->data_sectors,0x0,NUM_DATA_SECTORS_PER_FILE * sizeof(unsigned int)); // no data in the root dir right now
        root_node->size = 0;
        *(unsigned int*)&last_read_sector[0x0] = root_node->id;
        *(unsigned int*)&last_read_sector[0x4] = root_node->size;
        last_read_sector[0x8] = root_node->type;
        last_read_sector[0x9] = 0;
        last_read_sector[0xa] = 0;
        last_read_sector[0xb] = 0;
        *(unsigned int*)&last_read_sector[0xc] = 0;
        memset(&last_read_sector[0x10],0x0,NUM_DATA_SECTORS_PER_FILE * sizeof(unsigned int));
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
        root_node->size = *(unsigned int*)&last_read_sector[0x04];
        root_node->type = last_read_sector[0x08];
        root_node->unused_flag_one = last_read_sector[0x09];
        root_node->unused_flag_two = last_read_sector[0x0a];
        root_node->unused_flag_three = last_read_sector[0x0b];
        root_node->indirect_sector = *(unsigned int*)&last_read_sector[0xc];
        memcpy(root_node->data_sectors,&last_read_sector[0x10],NUM_DATA_SECTORS_PER_FILE * sizeof(unsigned int));
        read_bitmaps_from_disk();
    }
    
    active_dir = root_node;
    
    inode_name_pair_t* name_pair = (inode_name_pair_t*)kmalloc(sizeof(inode_name_pair_t));
    name_pair->length = 4;
    name_pair->id = FS_ROOT_DIR_ID;
    name_pair->parent_id = FS_ROOT_DIR_ID;
    name_pair->name = kmalloc(sizeof(unsigned char) * 5);
    memcpy(name_pair->name,"root",sizeof(unsigned char) * 5);
    vector_append(&inode_name_pairs,(unsigned int)name_pair);
    
    vector_append(&inodes,(unsigned int)root_node);
    
    build_inodes(root_node,root_node->id);
}

unsigned char write_directory_entry(inode_t* parent_dir, unsigned int child_inode_id, unsigned char* name, unsigned char name_length){
    unsigned char data_buffer[sizeof(unsigned int) + sizeof(unsigned char) + 256];
    unsigned int data_buffer_size = sizeof(unsigned int) + sizeof(unsigned char) + name_length;

    // copy our data into the byte datastream buffer
    *(unsigned int*)&data_buffer[0] = child_inode_id;
    data_buffer[sizeof(unsigned int)] = name_length;
    memcpy(&data_buffer[sizeof(unsigned int) + sizeof(unsigned char)],name,name_length);

    unsigned int total_bytes = parent_dir->size;
    unsigned int sector_idx = total_bytes / ATA_SECTOR_SIZE;
    unsigned int sector_offset = total_bytes % ATA_SECTOR_SIZE;

    if (sector_idx >= NUM_DATA_SECTORS_PER_FILE) return 0;

    if(parent_dir->data_sectors[sector_idx] == 0){
        parent_dir->data_sectors[sector_idx] = allocate_sector();
    }

    unsigned int sector = parent_dir->data_sectors[sector_idx];

    if (last_read_sector_idx != sector){
        read_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, sector);
        last_read_sector_idx = sector;
    }

    unsigned int bytes_written = 0;
    unsigned int remaining_bytes_in_sector = ATA_SECTOR_SIZE - sector_offset;
    
    while(bytes_written < data_buffer_size){

        unsigned int to_write = data_buffer_size - bytes_written;
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
    *(unsigned int*)&last_read_sector[0] = *(unsigned int*)&last_read_sector[0] + 1; // increment the number of entries
    write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,parent_dir->data_sectors[0]); 

    return 1;
}

string_array_t* get_all_names_in_dir(inode_t* dir, unsigned char add_slash){

    // assure that size was set
    if (dir->size < sizeof(unsigned int)) {
        return 0;
    }
    
    unsigned char* buffer = read_dir_entries_to_buffer(dir);
    if (!buffer) return 0;
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    
    unsigned int n_strings = *(unsigned int*)&buffer[0];
    string_t* strings = (string_t*)kmalloc(sizeof(string_t) * n_strings);
    
    unsigned int buffer_idx = sizeof(unsigned int);
    for (unsigned int i = 0; i < n_strings;i++){
        unsigned int id = *(unsigned int*)&buffer[buffer_idx];
        buffer_idx += sizeof(unsigned int); // skip id
        unsigned char raw_len = buffer[buffer_idx++];
        inode_t* node = get_inode_by_id(id);
        if (!node) error("Invalid node");
        unsigned char is_dir = node->type == FS_TYPE_DIR;
        unsigned int str_len = raw_len + (is_dir && add_slash ? 1 : 0);

        strings[i].length = str_len;
        strings[i].str = (unsigned char*)kmalloc(str_len + 1);
        strings[i].str[str_len] = 0;
        memcpy(strings[i].str,&buffer[buffer_idx],str_len);
        if (is_dir && add_slash) {strings[i].str[str_len - 1] = '/';} // add a '/' to signal it is a directory
        buffer_idx += raw_len; 
    }

    
    str_arr->n_strings = n_strings;
    str_arr->strings = strings;
    kfree(buffer);

    return str_arr;

}

int write_data_buffer_to_disk(inode_t* inode, unsigned char* buffer, unsigned int buffer_size){
    
    if (!inode) return INTERNAL_FUNCTION_FAIL;
    if (!buffer) return INTERNAL_FUNCTION_FAIL;

    unsigned int buffer_index = 0;
    unsigned int bytes_written = 0;

    while(bytes_written < buffer_size){
        unsigned int sector_idx = bytes_written / ATA_SECTOR_SIZE;
        
        if (sector_idx >= MAX_FILE_SECTORS) return INTERNAL_FUNCTION_FAIL;

        unsigned int sector;

        if(sector_idx >= FS_INODES_PER_SECTOR){

            if (!inode->indirect_sector) return INTERNAL_FUNCTION_FAIL;
            if(last_read_sector_idx != inode->indirect_sector){
                read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode->indirect_sector);
                last_read_sector_idx = inode->indirect_sector;
            }

            sector = *(unsigned int*)&last_read_sector[sizeof(unsigned int) * (sector_idx - FS_INODES_PER_SECTOR )];
        }else{
            sector = inode->data_sectors[sector_idx];
        }

        write_sectors(ATA_PRIMARY_BUS_IO,1,&buffer[buffer_index],sector);
        buffer_index += ATA_SECTOR_SIZE;
        unsigned int remaining = buffer_size - bytes_written;
        bytes_written += (remaining >= ATA_SECTOR_SIZE) ? ATA_SECTOR_SIZE : remaining;

    }

    return INTERNAL_FUNCTION_SUCCESS;
    
}

void erase_directory_entry(inode_t* dir, unsigned int entry_inode_id){
    unsigned int start_idx = 0;
    unsigned int data_len = 0;
    unsigned int n_entries = 0;
    
    if (!dir->data_sectors[0]) return; // nothing there so nothing to remove

    unsigned char* buffer = read_dir_entries_to_buffer(dir);

    n_entries = *(unsigned int*)&buffer[0];

    unsigned int buffer_offset = sizeof(unsigned int);
    for (unsigned int i = 0; i < n_entries;i++){
        unsigned int id = *(unsigned int*)&buffer[buffer_offset];
        if (id == entry_inode_id) {
            start_idx = buffer_offset;
            data_len = buffer[buffer_offset + sizeof(unsigned int)] + sizeof(unsigned char) + sizeof(unsigned int);
        }
        buffer_offset += sizeof(unsigned int);
        unsigned int len = buffer[buffer_offset];
        buffer_offset += sizeof(unsigned char) + len;
    }

    memmove(&buffer[start_idx],&buffer[start_idx + data_len],buffer_offset - data_len - start_idx);
    memset(&buffer[buffer_offset - data_len],0x00,data_len);
    n_entries--;
    *(unsigned int*)&buffer[0] = n_entries;
    dir->size -= data_len;

    unsigned char last_sector_free = (buffer_offset % ATA_SECTOR_SIZE) < data_len;
    if (write_data_buffer_to_disk(dir,buffer,buffer_offset) != INTERNAL_FUNCTION_SUCCESS){
        error("Writing data to sector failed");
    }
    kfree(buffer);
    
}

int delete_file(inode_t* parent_dir,unsigned char* name,unsigned int name_length){
    inode_t* inode;
    if(!dir_contains_name(parent_dir,name)){
        return FS_FILE_NOT_FOUND;
    }
    
    inode = get_inode_by_id(get_inode_id_by_name(parent_dir->id,name));

    if(inode->type == FS_TYPE_DIR) return FILE_INVALID_TARGET;

    if (inode->indirect_sector){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,inode->indirect_sector);
        last_read_sector_idx = inode->indirect_sector;

        for(unsigned int i = 0;i < ATA_SECTOR_SIZE / sizeof(unsigned int);i++){
            unsigned int sector = *(unsigned int*)&last_read_sector[i * sizeof(unsigned int)];
            if (sector) free_sector(sector);
        }
        free_sector(inode->indirect_sector);
    }

    for (unsigned int i = 0; i < NUM_DATA_SECTORS_PER_FILE;i++){
        unsigned int data_sector = inode->data_sectors[i];
        if(data_sector) free_sector(data_sector);
        
    }

    erase_directory_entry(parent_dir,inode->id);
    
    inode_name_pair_t* pair = get_name_by_inode_id(inode->id);
    unsigned int name_pair_idx = vector_find(&inode_name_pairs,(unsigned int)pair);
    vector_erase(&inode_name_pairs,name_pair_idx);
    kfree(pair->name);
    kfree(pair);

    free_inode_section(inode->id);
    unsigned int idx = vector_find(&inodes, (unsigned int)inode);
    vector_erase(&inodes,idx);
    kfree(inode);



    return FS_FILE_DELETION_SUCCESS;
}

int create_file(inode_t* parent_dir, unsigned char* name, unsigned char name_length, unsigned char type){

    string_array_t* strs_in_parent_dir = get_all_names_in_dir(parent_dir,0);
    if (strs_in_parent_dir){
        for (unsigned int i = 0; i < strs_in_parent_dir->n_strings;i++){
            if(strneq(name,strs_in_parent_dir->strings[i].str,name_length,strs_in_parent_dir->strings[i].length)) {
                free_string_arr(strs_in_parent_dir);
                return FS_FILE_ALREADY_EXISTS;
            }
        }
        free_string_arr(strs_in_parent_dir);
    }


    if (parent_dir->data_sectors[0] == 0){
        parent_dir->data_sectors[0] = allocate_sector();
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,parent_dir->data_sectors[0]);
        last_read_sector_idx = parent_dir->data_sectors[0];
        memset(last_read_sector, 0, ATA_SECTOR_SIZE); 
        *(unsigned int*)&last_read_sector[0] = 0; 
        write_sectors(ATA_PRIMARY_BUS_IO, 1, last_read_sector, parent_dir->data_sectors[0]);
        parent_dir->size = sizeof(unsigned int);
    } 
    
    
    inode_t* file = (inode_t*)kmalloc(sizeof(inode_t));
    file->type = type;
    file->id = allocate_inode_section();
    memset(file->data_sectors,0,NUM_DATA_SECTORS_PER_FILE * sizeof(unsigned int));
    file->indirect_sector = 0;
    file->size = 0;
    vector_append(&inodes,(unsigned int)file);

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
    vector_append(&inode_name_pairs,(unsigned int)name_pair);

    return FS_FILE_CREATION_SUCCESS;
}

void write_inode_to_disk(inode_t* inode,unsigned char is_root){
    
    unsigned int sector_idx = (inode->id / 8) + (is_root != 1); // add one if it is not root

    unsigned int sector_start = (inode->id % 8) * SECTOR_BITMAP_SIZE;
    if (is_root){
        sector_idx = 0;
        sector_start = 0;
    }
    if (last_read_sector_idx != sector_idx){
        read_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector_idx);
        last_read_sector_idx = sector_idx;
    }

    *(unsigned int*)&last_read_sector[sector_start] = inode->id;
    *(unsigned int*)&last_read_sector[sector_start + 0x4] = inode->size;
    last_read_sector[sector_start + 0x8] = inode->type;
    last_read_sector[sector_start + 0x9] = 0x0;
    last_read_sector[sector_start + 0xa] = 0x0;
    last_read_sector[sector_start + 0xb] = 0x0;
    *(unsigned int*)&last_read_sector[sector_start + 0xc] = inode->indirect_sector;
    memcpy(&last_read_sector[sector_start + 0x10],inode->data_sectors,sizeof(unsigned int) * NUM_DATA_SECTORS_PER_FILE);

    write_sectors(ATA_PRIMARY_BUS_IO,1,last_read_sector,sector_idx);
}

void write_to_disk(){
    for (unsigned int i = 0; i < inodes.size;i++){
        write_inode_to_disk((inode_t*)inodes.data[i], i == 0);
    }
    log("Wrote all inodes to disk");
    write_bitmaps_to_disk();
    log("Wrote all bitmaps to disk");
}