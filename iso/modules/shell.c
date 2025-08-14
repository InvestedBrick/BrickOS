#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"

typedef struct {
    string_t command;
    unsigned int n_args;
    string_t* args;
}command_t;

void free_command(command_t com){
    free(com.command.str);
    for (unsigned int i = 0; i < com.n_args;i++){
        free(com.args[i].str);
    }
    free(com.args);
}

string_t parse_word(unsigned char* line, unsigned char start_idx, unsigned int line_length){
    string_t word;
    word.length = 0;
    unsigned char* buffer = (unsigned char*)malloc(line_length + 1);
    memset(buffer,0,line_length + 1);

    for (unsigned int i = start_idx; i < line_length && line[i] != ' '  ;i++){
        buffer[i - start_idx] = line[i];
        word.length++;
    }

    word.str = (unsigned char*)malloc(word.length + 1);
    memcpy(word.str,buffer,word.length);
    word.str[word.length] = '\0';

    free(buffer);

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
    comd.args = (string_t*)malloc(sizeof(string_t) * comd.n_args);

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
__attribute__((section(".text.start")))
void main(){
    print("\nBrickOS Shell started\n");
    unsigned char* line = (unsigned char*)malloc(SCREEN_COLUMNS + 1);
    unsigned char* dir_buffer = (unsigned char*)malloc(100);
    command_t comd;
    while(1){
        memset(line,0x0,SCREEN_COLUMNS + 1);
        print("|-(");
        getcwd(dir_buffer,100);
        print(dir_buffer);
        print(")-> ");
        int read_bytes = read_input(line,SCREEN_COLUMNS);
        line[read_bytes] = '\0';
        print("\n");

        comd = parse_line(line,strlen(line));

        if (!comd.command.length) continue;

        unsigned char* main_cmd = comd.command.str;

        if (streq(main_cmd,"help")){
            print("Shell command list\n------------------\n");
            print("clear - Clears the screen\n");
            print("ls - Lists all entries of the current directory\n");
            print("cd [dir name] - Changes the active directory\n");
            print("exit - Exits the shell and shuts down the OS\n\n");

        }
        else if (streq(main_cmd,"exit")){
            break;
        }
        else if (streq(main_cmd,"clear")){
            print("\e"); // special clear char
        }
        else if (streq(main_cmd,"ls")){
            unsigned int dir_fd = open(dir_buffer,FILE_FLAG_NONE);

            unsigned char buffer[1024];

            int n_entries = getdents(dir_fd,(dirent_t*)buffer,sizeof(buffer));

            if (n_entries == -1) {print("Unable to read directory entries\n");}
            else{

                unsigned int bpos = 0;
                for (unsigned int i = 0; i < n_entries;i++){
                    dirent_t* ent = (dirent_t*)(buffer + bpos);
                    print(ent->name);
                    if (ent->type == TYPE_DIR){
                        print("/");
                    }
                    print("\n");
                    bpos += ent->len;
                }
            }
            
            close(dir_fd);
        }
        else if(streq(main_cmd,"cd")){
            if (!comd.args)
                {print("Expected name of directory\n");}
            else{
                chdir(comd.args[0].str);
            }
        }
        else{
            print("Command '");
            print(main_cmd);
            print("' was not found!\n");
        }
        
        free_command(comd);
    }

    exit(1);
}