#include "virt_files_util.h"
#include "virt_files.h"
#include "../../processes/user_process.h"
#include "../../memory/kmalloc.h"

uint32_t virt_inode_id = FS_ROOT_DIR_ID + 1;

virt_dir_t* get_virt_dir_by_inode_id(uint32_t id){
    for (uint32_t i = 0; i < virt_dirs.size;i++){
        virt_dir_t* virt_dir = (virt_dir_t*)virt_dirs.data[i];
        if (virt_dir->inode_id == id){
            return virt_dir;
        }
    }

    return nullptr;
}

string_array_t* get_virt_dir_names_in_dir(inode_t* phys_dir){
    uint32_t virt_dirs_cnt = 0;
    
    
    for (uint32_t i = 0; i < virt_dirs.size;i++){
        virt_dir_t* virt_dir = (virt_dir_t*)virt_dirs.data[i];
        inode_t* parent_dir = get_parent_inode(get_inode_by_id(virt_dir->inode_id));
        if (parent_dir->id == phys_dir->id){
            virt_dirs_cnt++;
        }
    }
    if (virt_dirs_cnt == 0) return nullptr;
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    str_arr->strings = (string_t*)kmalloc(sizeof(string_t) * virt_dirs_cnt);
    uint32_t virt_dir_idx = 0;

    for (uint32_t i = 0; i < virt_dirs.size;i++){
        virt_dir_t* virt_dir = (virt_dir_t*)virt_dirs.data[i];
        inode_t* parent_dir = get_parent_inode(get_inode_by_id(virt_dir->inode_id));
        if (parent_dir->id == phys_dir->id){
            inode_name_pair_t* name_pair = get_name_by_inode_id(virt_dir->inode_id);
          
            str_arr->strings[virt_dir_idx].str = (unsigned char*)kmalloc(name_pair->length + 1);
            memcpy(str_arr->strings[virt_dir_idx].str,name_pair->name,name_pair->length);
            str_arr->strings[virt_dir_idx].str[name_pair->length] = 0;
            str_arr->strings[virt_dir_idx].length = name_pair->length;
            virt_dir_idx++;
        }

        if (virt_dir_idx == virt_dirs_cnt) break;

    }

    str_arr->n_strings = virt_dirs_cnt;
    
    return str_arr;
}

string_array_t* get_all_names_in_virt_dir(inode_t* virt_dir){
    if (!virt_dir || virt_dir->type != FS_TYPE_VIRT_DIR) return nullptr;
    virt_dir_t* virt_dir_struct =  get_virt_dir_by_inode_id(virt_dir->id);
    if (!virt_dir_struct) return nullptr;
    
    string_array_t* str_arr = (string_array_t*)kmalloc(sizeof(string_array_t));
    str_arr->n_strings = virt_dir_struct->entry_names.size;
    string_t* strings = (string_t*)kmalloc(sizeof(string_t) * str_arr->n_strings);
    
    for (uint32_t i = 0; i < virt_dir_struct->entry_names.size;i++){
        unsigned char* str = (unsigned char*)virt_dir_struct->entry_names.data[i];
        uint32_t len = strlen(str);
        strings[i].length = len;
        strings[i].str = (unsigned char*)kmalloc(len + 1);
        memcpy(strings[i].str,str,len);
        strings[i].str[len] = 0;
        
    }
    
    str_arr->strings = strings;
    
    return str_arr;
}

inode_t* create_virt_dir(inode_t* parent_dir, unsigned char* name){
    // not checking if the name already exists since only the kernel can create virtual directories rn
    inode_t* virt_dir = create_inode(FS_FILE_PERM_NONE,FS_TYPE_VIRT_DIR,PRIV_STD, virt_inode_id++);
    create_inode_name_pair(virt_dir->id,parent_dir->id,strlen(name),name);
    
    virt_dir_t* virt_dir_struct = (virt_dir_t*)kmalloc(sizeof(virt_dir_t));
    virt_dir_struct->inode_id = virt_dir->id;
    init_vector(&virt_dir_struct->entry_names);

    vector_append(&virt_dirs,(vector_data_t)virt_dir_struct);

    return virt_dir;
}



void create_virt_file(inode_t* parent_dir,unsigned char* name,vfs_handles_t* handles,uint8_t priv_lvl){
    if (parent_dir->type != FS_TYPE_VIRT_DIR) return;
    inode_t* virt_file = create_inode(FS_FILE_PERM_NONE,FS_TYPE_VIRT_FILE,priv_lvl,virt_inode_id++);
    create_inode_name_pair(virt_file->id,parent_dir->id,strlen(name),name);

    virt_file_t* virt_file_struct = (virt_file_t*)kmalloc(sizeof(virt_file_t));
    virt_file_struct->inode_id = virt_file->id;
    virt_file_struct->gen_file = (generic_file_t*)kmalloc(sizeof(generic_file_t));
    virt_file_struct->gen_file->ops = handles;
    virt_file_struct->gen_file->generic_data = nullptr;
    virt_file_struct->gen_file->object_id = virt_file->id;
    vector_append(&virt_files,(vector_data_t)virt_file_struct);
    
    virt_dir_t* virt_dir = get_virt_dir_by_inode_id(parent_dir->id);
    uint32_t name_len = strlen(name);
    unsigned char* str = (unsigned char*)kmalloc(name_len);
    memcpy(str,name,name_len);
    str[name_len] = 0;
    vector_append(&virt_dir->entry_names,(vector_data_t)str);
}