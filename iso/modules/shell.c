#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
void main(){
    print("\nBrickOS Shell started\n");
    unsigned char* line = (unsigned char*)malloc(SCREEN_COLUMNS + 1);
    unsigned char* dir_buffer = (unsigned char*)malloc(100);
    while(1){
        memset(line,0x0,SCREEN_COLUMNS + 1);
        print("|-(");
        getcwd(dir_buffer,100);
        print(dir_buffer);
        print(")-> ");
        int read_bytes = read_input(line,SCREEN_COLUMNS);
        line[read_bytes] = '\0';
        print("\n");
        if (streq(line,"help")){
            print("Shell command list\n------------------\n");
            print("clear - Clears the screen\n");
            print("ls - Lists all entries of the current directory\n");
            print("exit - Exits the shell and shuts down the OS\n\n");

        }
        else if (streq(line,"exit")){
            break;
        }
        else if (streq(line,"clear")){
            print("\e"); // special clear char
        }
        else if (streq(line,"ls")){
            unsigned int dir_fd = open(dir_buffer,FILE_FLAG_NONE);

            unsigned char buffer[1024];

            int n_entries = getdents(dir_fd,(dirent_t*)buffer,sizeof(buffer));

            if (n_entries == -1) {print("Unable to read directory entries\n");continue;};

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

            close(dir_fd);
        }
        else{
            print("Command '");
            print(line);
            print("' was not found!\n");
        }
    }

    exit(1);
}