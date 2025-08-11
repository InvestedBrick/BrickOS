#include "fsutil.h"
#include "filesystem.h"
#include "../memory/kmalloc.h"
#include "../util.h"
int get_full_active_path(unsigned char* path_buffer, unsigned int buf_len){
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    inode_t* inode = active_dir;

    unsigned int str_counter = 0;

    while(1) {
        str_counter++;
        inode_t* parent = get_parent_inode(inode);
        if (parent == inode) break; // we have reached root
        inode = parent;
    }

    str_arr->strings = (string_t*)kmalloc(str_counter * sizeof(string_t));
    str_arr->n_strings = str_counter;
    inode = active_dir;

    unsigned int str_arr_idx = 0;
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

    unsigned int path_size = 0;
    
    for (unsigned int i = 0; i < str_counter;i++){
        path_size += str_arr->strings[i].length + 1; // one more for the '/' 
    }

    if (path_size + 1 >= buf_len){
        free_string_arr(str_arr); 
        return -1;
    }

    unsigned int path_idx = 0;
    for(int i = str_counter - 1;i >= 0;i--){
        memcpy(&path_buffer[path_idx],str_arr->strings[i].str,str_arr->strings[i].length);
        path_idx += str_arr->strings[i].length;
        path_buffer[path_idx++] = '/';
    }
    path_buffer[path_size] = '\0';

    free_string_arr(str_arr); 

    return 0;
} 