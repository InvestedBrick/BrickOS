#include "cstdlib/stdio.h"
#include "cstdlib/stdutils.h"
#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
void main(){
    print("\nBrick OS Shell - type 'help' for command list\n");
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
            print("exit - Exits the shell and shuts down the OS\n\n");

        }
        else if (streq(line,"exit")){
            break;
        }
        else if (streq(line,"clear")){
            print("\e"); // special clear char
        }else{
            print("Command '");
            print(line);
            print("' was not found!\n");
        }
    }

    exit(1);
}