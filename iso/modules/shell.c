#include "shell.h"
#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
#include "cstdlib/time.h"
#include "../../kernel/src/filesystem/devices/device_defines.h"
#include <stdint.h>

#define LINE_BUFFER_SIZE 256
//TODO: extract most commands to individual executables
shell_command_t commands[] = {
    {"help", cmd_help, "Shows help menu"},
    {"exit", 0, "Exits the shell"},
    {"clear",cmd_clear, "Clears the screen"},
    {"ls", cmd_ls, "Lists active directory entries"},
    {"cd", cmd_cd, "Changes active directory"},
    {"mkf", cmd_mkf, "Creates a file"},
    {"mkdir",cmd_mkdir, "Creates a directory"},
    {"rm",cmd_rm,"Deletes a file"},
    {"clock",cmd_clock,"Prints current date and time"},
    {0,0,0}
};

int delete_dir_recursive(unsigned char* dir_name){
    int dir_fd = open(dir_name,FILE_FLAG_NONE);
    if (chdir(dir_name) < 0) {
        print("Invalid directory\n");
        return -1;
    }
    if (dir_fd < 0) 
        {print("Failed to open directory\n"); return -1;}
    const uint32_t buffer_size = 512;
    unsigned char* buffer = (unsigned char*)malloc(buffer_size);
    int n_entries = getdents(dir_fd,(dirent_t*)buffer,buffer_size);
    uint32_t bpos = 0;
    for (int i = 0; i < n_entries;i++){
        dirent_t* ent = (dirent_t*)(buffer + bpos);
        if (ent->type == TYPE_FILE){
            if (rmfile(ent->name) < 0) {
                print("Failed to remove file '");
                print(ent->name);
                print("'\n");
            }
        }else{
            if (delete_dir_recursive(ent->name) < 0){
                print("Failed to remote directory '");
                print(ent->name);
                print("'\n");
            }
        }
        bpos += ent->len;
    }

    free(buffer);
    close(dir_fd);

    chdir("..");
    if (rmfile(dir_name) < 0) {
        print("Failed to remove directory '");
        print(dir_name);
        print("'\n");
        return -1;
    }

    return 0;
}

int argcheck(command_t* cmd,const char* msg){
    if (!cmd->args)
        {print(msg);return 1;}

    return 0;
}

void cmd_help(command_t* cmd){
    print("Shell command list\n------------------\n");
    for (int i = 0; commands[i].name;i++){
        print(commands[i].name);
        print(" - ");
        print(commands[i].help);
        print("\n");
    }
}
void cmd_clear(command_t* cmd){
    print("\e");
}
void cmd_ls(command_t* cmd){
    int dir_fd = open(cmd->executing_dir,FILE_FLAG_NONE);

    if (dir_fd < 0) 
        {print("Failed to open directory\n"); return;}
    unsigned char buffer[1024];
    int n_entries = getdents(dir_fd,(dirent_t*)buffer,sizeof(buffer));
    if (n_entries == -1) return; // dir is empty
    uint32_t bpos = 0;
    for (uint32_t i = 0; i < n_entries;i++){
        dirent_t* ent = (dirent_t*)(buffer + bpos);
        print(ent->name);
        if (ent->type == TYPE_DIR){
            print("/");
        }
        print("\n");
        bpos += ent->len;
    }
    
    if (close(dir_fd) < 0)
        print("Failed to close directory\n");
}
void cmd_cd(command_t* cmd){
    if (argcheck(cmd,"Expected name of directory\n")) return;

    if (chdir(cmd->args[0].str) == SYSCALL_FAIL) print("Not a directory\n");
}
void cmd_mkf(command_t* cmd){
    if (argcheck(cmd,"Expected name of file\n")) return;
    int fd = open(cmd->args[0].str,FILE_FLAG_CREATE);
    if (fd < 0)
        {print("Failed to create file\n");return;}
    if (close(fd) < 0)
        print("Failed to close created file\n");
}
void cmd_mkdir(command_t* cmd){
    if (argcheck(cmd,"Expected name of directory\n")) return;
        
    int fd = open(cmd->args[0].str,FILE_FLAG_CREATE | FILE_CREATE_DIR);
    if (fd < 0)
        {print("Failed to create directory\n");return;}
    if (close(fd) < 0)
        print("Failed to close created directory\n");
}
void cmd_rm(command_t* cmd){
    if (argcheck(cmd,"Expected filename\n")) return;

    unsigned char* filename = cmd->args[0].str;
    if (cmd->n_args > 1){
        if (streq(cmd->args[1].str,"-r")){
            delete_dir_recursive(filename);
        }else{
            print("Invalid argument\n");
        }
    }else{
        if (rmfile(cmd->args[0].str) < 0) 
            print("Failed to delete file\n");
    }
}

void cmd_clock(command_t* cmd){
    uint64_t timestamp = gettimeofday();
    date_t date;
    parse_unix_timestamp(timestamp,&date);
    print("Today is the ");
    char* day = uint32_to_ascii(date.day);
    char* year = uint32_to_ascii(date.year);
    char* months[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
    print(day);
    char last_num = day[strlen(day) - 1];
    switch (last_num)
    {
    case '1':
        print("st");
        break;
    case '2':
        print("nd");
    case '3':
        print("rd");
    default:
        print("th");
    }

    print(" of ");
    print(months[date.month - 1]);
    print(" "); print(year); print("\nThe time is: ");
    free(day); 
    free(year);
    char* hour = uint32_to_ascii(date.hour);
    char* minute = uint32_to_ascii(date.minute);
    char* second = uint32_to_ascii(date.second);
    print(hour); print(":"); 
    if (date.minute < 10) print("0");
    print(minute); print(" and "); print(second); print(" seconds\n");
    free(hour);
    free(minute);
    free(second);

}
void free_command(command_t com){
    free(com.command.str);
    for (uint32_t i = 0; i < com.n_args;i++){
        free(com.args[i].str);
    }
    free(com.args);
}

string_t parse_word(unsigned char* line, uint32_t start_idx, uint32_t line_length){
    string_t word;
    word.length = 0;
    unsigned char* buffer = (unsigned char*)malloc(line_length + 1);
    memset(buffer,0,line_length + 1);

    for (uint32_t i = start_idx; i < line_length && line[i] != ' ' && line[i] != '\0' && line[i] != '\n' ;i++){
        buffer[i - start_idx] = line[i];
        word.length++;
    }

    word.str = (unsigned char*)malloc(word.length + 1);
    memcpy(word.str,buffer,word.length);
    word.str[word.length] = '\0';

    free(buffer);

    return word;
}

void parse_line(unsigned char* line,uint32_t line_length,command_t* comd){
    comd->n_args = 0;
    // skip any leading spaces
    uint32_t command_start = 0;
    while(line[command_start] == ' ') command_start++;

    // parse the main command
    comd->command = parse_word(line,command_start,line_length);

    uint32_t i = command_start + comd->command.length;

    while (i < line_length) {
        while (i < line_length && line[i] == ' ') i++;
        if (i < line_length) {
            comd->n_args++;
            // Skip the word
            while (i < line_length && line[i] != ' ') i++;
        }
    }
    comd->args = (string_t*)malloc(sizeof(string_t) * comd->n_args);

    uint32_t arg_idx = 0;
    //parse optional arguments
    uint32_t j = command_start + comd->command.length;
    while (j < line_length) {
        while (j < line_length && line[j] == ' ') j++;
        if (j >= line_length) break;
        if (arg_idx >= comd->n_args) break;

        string_t word = parse_word(line, j, line_length);
        comd->args[arg_idx++] = word;

        j += word.length;
    }

}

__attribute__((section(".text.start")))
void main(int argc, char* argv[]){
    print("BrickOS Shell started\n");
    unsigned char* line = (unsigned char*)malloc(LINE_BUFFER_SIZE + 1);
    unsigned char* dir_buffer = (unsigned char*)malloc(100);
    command_t comd;
    while(1){
        memset(line,0x0,LINE_BUFFER_SIZE + 1);
        print("|-(");
        getcwd(dir_buffer,100);
        print(dir_buffer);
        print(")-> ");
        int read_bytes = read_input(line,LINE_BUFFER_SIZE);
        line[read_bytes] = '\0';
        print("\n");
        
        parse_line(line,strlen(line),&comd);
        
        comd.executing_dir = dir_buffer;

        if (!comd.command.length) continue;
        
        unsigned char* main_cmd = comd.command.str;
        uint8_t found = 0;
        if (streq(main_cmd,"exit")){
            break;
        }
        for (int i = 0; commands[i].name;i++){
            if (streq(main_cmd,commands[i].name)){
                commands[i].handler(&comd);
                found = 1;
                break;
            }
        }
        if (!found){
            print("Command '");
            print(main_cmd);
            print("' was not found!\n");
        }  
        free_command(comd);
    }

    exit(0);
}