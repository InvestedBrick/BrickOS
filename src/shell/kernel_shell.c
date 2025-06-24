#include "kernel_shell.h"
#include "../screen.h"
#include "../memory/kmalloc.h"
#include "../drivers/keyboard/keyboard.h"
#include "../util.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/file_operations.h"
#include "../io/log.h"

void free_command(command_t com){
    kfree(com.command.str);
    for (unsigned int i = 0; i < com.n_args;i++){
        kfree(com.args[i].str);
    }
    kfree(com.args);
}

string_t parse_word(unsigned char* line, unsigned char start_idx, unsigned int line_length){
    string_t word;
    word.length = 0;
    unsigned char* buffer = (unsigned char*)kmalloc(line_length + 1);
    memset(buffer,0,line_length + 1);

    for (unsigned int i = start_idx; i < line_length && line[i] != ' '  ;i++){
        buffer[i - start_idx] = line[i];
        word.length++;
    }

    word.str = (unsigned char*)kmalloc(word.length + 1);
    memcpy(word.str,buffer,word.length);
    word.str[word.length] = '\0';

    kfree(buffer);

    return word;
}

command_t parse_line(unsigned char* line,unsigned int line_length){
    command_t comd;
    comd.n_args = 0;
    // skip any leading spaces
    unsigned int command_start = 0;
    while(line[command_start] == ' ') command_start++;

    // parse the main command
    comd.command = parse_word(line,command_start,line_length);

    unsigned int i = command_start + comd.command.length;

    while (i < line_length) {
        while (i < line_length && line[i] == ' ') i++;
        if (i < line_length) {
            comd.n_args++;
            // Skip the word
            while (i < line_length && line[i] != ' ') i++;
        }
    }
    comd.args = (string_t*)kmalloc(sizeof(string_t) * comd.n_args);

    unsigned int arg_idx = 0;
    //parse optional arguments
    for(unsigned int i = command_start + comd.command.length; i < line_length;i++){
        while (i < line_length && line[i] == ' ') i++;
        
        if (i >= line_length) break;

        string_t word = parse_word(line,i,line_length);
        comd.args[arg_idx++] = word;

        i+=word.length;
    }

    return comd;

}

void readline(unsigned char* line){
    unsigned int line_idx = 0;
    char ch;
    while(1){
        if (kb_buffer_pop(&ch)){
            switch (ch)
            {
            case '\n':
            {
                line[line_idx] = 0;
                return;
            }
            case '\b':{
                if (line_idx > 0){
                    erase_one_char();
                    line[--line_idx] = 0x00;
                }
            }
            //don't care about tabs right now
            case '\t':{
                break;
            }
            default:
                if (line_idx < SCREEN_COLUMNS){
                    line[line_idx++] = ch;
                    write_string(&ch,1);
                }
                break;
            }
        }
    }
}

void change_directory(command_t comd){
    if (streq(comd.args[0].str,"..")){
        inode_t* inode = get_parent_inode(active_dir);
        if(!inode) error("Failed to retrieve inode"); // should not happen, therefore log to logs if fails
        active_dir = inode;
        return;
    }


    string_array_t* strs = get_all_names_in_dir(active_dir,1);
    if (!strs) {write_string("The current directory is empty",30);newline();return;}

    for(unsigned int i = 0; i < strs->n_strings;i++){
        if (!(strs->strings[i].str[strs->strings[i].length - 1] == '/')){
            continue;
        }
        unsigned int name_len = strs->strings[i].length - 1; // ignore the '/' because it is not part of the actualy name
        if(strneq(strs->strings[i].str,comd.args[0].str,name_len,name_len)){
            unsigned int id = get_inode_id_by_name(active_dir->id,comd.args[0].str);
            inode_t* inode = get_inode_by_id(id);
            if(!inode) error("Failed to retrieve inode");
            active_dir = inode;
            free_string_arr(strs);
            return;
        }
    }

    free_string_arr(strs);
    write_string("Target directory was not found",30);
    newline();
    return;


}

string_t get_full_active_path(){
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

    unsigned char* path;
    unsigned int path_size = 0;
    
    for (unsigned int i = 0; i < str_counter;i++){
        path_size += str_arr->strings[i].length + 1; // one more for the '/' 
    }

    path = (unsigned char*)kmalloc(path_size);
    
    unsigned int path_idx = 0;
    for(int i = str_counter - 1;i >= 0;i--){
        memcpy(&path[path_idx],str_arr->strings[i].str,str_arr->strings[i].length);
        path_idx += str_arr->strings[i].length;
        path[path_idx++] = '/';
    }

    free_string_arr(str_arr);

    string_t full_path;
    full_path.length = path_size;
    full_path.str = path;

    return full_path;
} 

void start_shell(){
    unsigned char* line = kmalloc(SCREEN_COLUMNS + 1);
    newline(); 
    command_t comd;
    while (1){
        memset(line,0x00,SCREEN_COLUMNS + 1);
        write_string("|-(",3);
        string_t curr_dir = get_full_active_path();
        write_string(curr_dir.str,curr_dir.length);
        kfree(curr_dir.str);
        write_string(")-> ",4);

        readline(line);
        newline();
        unsigned int len = strlen(line);

        comd = parse_line(line,len);
        
        if(!comd.command.length) continue; // no need to free or parse what is not there
        
        if (streq(comd.command.str,"help")){
            write_string("Shell command list",18);
            newline();
            write_string("------------------",18);
            newline();
            write_string("clear - Clears the screen",25);
            newline();
            write_string("exit - Exits the shell and therefore shuts down the OS",54);
            newline();
            write_string("ls - Lists all entries of the current directory",47);
            newline();
            write_string("mkdir [directory name] - Creates a directory",44);
            newline();
            write_string("mkf [filename] - Creates a file",31);
            newline();
            write_string("cd - Changes directory to a subdirectory or the parent directory (with cd ..)",77);
            newline();

        }
        else if (streq(comd.command.str,"clear")) {
            clear_screen();
        }
        else if (streq(comd.command.str,"exit")){
            break;
        }
        else if (streq(comd.command.str,"ls")){
            string_array_t* names = get_all_names_in_dir(active_dir,1);

            if (names) {

                for(unsigned int i = 0; i < names->n_strings;i++){
                    write_string(names->strings[i].str,names->strings[i].length);
                    newline();
                }
                
                free_string_arr(names);
            } 

        }
        else if (streq(comd.command.str,"mkdir")){
            if (!comd.args) {write_string("Expected name of directory",26); newline();}
            else{
                int ret_val = create_file(active_dir,comd.args[0].str,comd.args[0].length,FS_TYPE_DIR);
                if (ret_val < 0) {write_string("Creation of directory failed",28); newline();}
            }
        }
        else if (streq(comd.command.str,"mkf")){
            if (!comd.args) {write_string("Expected name of file",21); newline();}
            else{
                int ret_val = create_file(active_dir,comd.args[0].str,comd.args[0].length,FS_TYPE_FILE);
                if (ret_val < 0) {write_string("Creation of file failed",23); newline();}
            }
        }
        else if (streq(comd.command.str,"rm")){
            if (!comd.args) {write_string("Expected name of file to remove",31); newline();}
            else{
                int ret_val = delete_file(active_dir,comd.args[0].str, comd.args[0].length);
                if (ret_val < 0){write_string("Deletion of file failed",23); newline();}
            }
        }
        else if (streq(comd.command.str,"cd")){
            if (!comd.args) {write_string("Expected name of directory",26); newline();}
            else{
                change_directory(comd);
            }
        }
        else if (streq(comd.command.str, "ftest")){
            if (!comd.args) {write_string("Expected name of file",21); newline();continue;};
            int fd = open(comd.args[0].str,FILE_FLAG_RW);
            unsigned char failed = 0;
            if (fd < 0){
                warn("opening failed");
                log_uint((unsigned int)fd);
                failed = 1;
            }
            unsigned char buf[] = {"Hello World, I hope this works"};
            unsigned int buf_size = sizeof("Hello World, I hope this works");
            int ret = write(fd,buf,buf_size);
            log("Wrote bytes:");
            log_uint(ret);
            if (ret < 0){
                warn("writing failed");
                log_uint((unsigned int)ret);
                failed = 1;
            }
            memset(buf,0,buf_size);
            ret = read(fd,buf,buf_size);
            if (ret < 0){
                warn("Reading failed");
                log_uint((unsigned int)ret);
                failed = 1;
            }
            log(buf);
            if (!streq(buf,"Hello World, I hope this works")) {
                warn("data not identical");
                failed = 1;
            }
            ret = close(fd);
            if (ret < 0){
                warn("closing failed");
                log_uint((unsigned int)ret);
                failed = 1;
            }
            if (!failed) log("ftest succeded");
        }
        else {
            write_string("Command '",9);
            write_string(comd.command.str,comd.command.length);
            write_string("' was not found!",16);
            newline();
        }
        free_command(comd);
    }
}