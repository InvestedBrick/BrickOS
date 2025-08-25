#include "fsutil.h"
#include "filesystem.h"
#include "../memory/kmalloc.h"
#include "../util.h"
int get_full_active_path(unsigned char* path_buffer, uint32_t buf_len){
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    inode_t* inode = active_dir;

    uint32_t str_counter = 0;

    while(1) {
        str_counter++;
        inode_t* parent = get_parent_inode(inode);
        if (parent == inode) break; // we have reached root
        inode = parent;
    }

    str_arr->strings = (string_t*)kmalloc(str_counter * sizeof(string_t));
    str_arr->n_strings = str_counter;
    inode = active_dir;

    uint32_t str_arr_idx = 0;
    while (1){
        inode_name_pair_t* name_pair = get_name_by_inode_id(inode->id);
        
        str_arr->strings[str_arr_idx].length = name_pair->length;
        str_arr->strings[str_arr_idx].str = (unsigned char*)kmalloc(name_pair->length);

        memcpy(str_arr->strings[str_arr_idx].str,name_pair->name,name_pair->length);
        str_arr_idx++;
        inode_t* parent = get_parent_inode(inode);

        if (parent == inode) break;

        inode = parent;
    }

    // str_arr now contains all the strings in reverse order

    uint32_t path_size = 0;
    
    for (uint32_t i = 0; i < str_counter;i++){
        path_size += str_arr->strings[i].length + 1; // one more for the '/' 
    }

    if (path_size + 1 >= buf_len){
        free_string_arr(str_arr); 
        return -1;
    }

    uint32_t path_idx = 0;
    for(int i = str_counter - 1;i >= 0;i--){
        memcpy(&path_buffer[path_idx],str_arr->strings[i].str,str_arr->strings[i].length);
        path_idx += str_arr->strings[i].length;
        path_buffer[path_idx++] = '/';
    }
    path_buffer[path_size] = '\0';

    free_string_arr(str_arr); 

    return 0;
} 


string_array_t* split_filepath(unsigned char* path){
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    uint32_t split_idx = rfind_char(path,'/');
    if (split_idx == (uint32_t)-1){
        str_arr->n_strings = 1;
        str_arr->strings = (string_t*)kmalloc(sizeof(string_t));
        str_arr->strings[0].length = strlen(path);
        str_arr->strings[0].str = (unsigned char*)kmalloc(str_arr->strings[0].length + 1);
        memcpy(str_arr->strings[0].str,path,str_arr->strings[0].length + 1);
        return str_arr;
    }

    str_arr->n_strings = 2;
    // preceding dirs
    str_arr->strings = (string_t*)kmalloc(sizeof(string_t) * 2);
    str_arr->strings[0].length = split_idx;
    str_arr->strings[0].str = (unsigned char*)kmalloc(str_arr->strings[0].length + 1);
    memcpy(str_arr->strings[0].str,path,str_arr->strings[0].length);
    str_arr->strings[0].str[str_arr->strings[0].length] = '\0';

    // final filename
    str_arr->strings[1].length = strlen(path) - split_idx - 1;
    str_arr->strings[1].str = (unsigned char*)kmalloc(str_arr->strings[1].length + 1);
    memcpy(str_arr->strings[1].str,&path[split_idx + 1],str_arr->strings[1].length);
    str_arr->strings[1].str[str_arr->strings[1].length] = '\0';

    return str_arr;
}