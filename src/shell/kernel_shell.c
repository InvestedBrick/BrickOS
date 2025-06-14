#include "kernel_shell.h"
#include "../screen.h"
#include "../memory/kmalloc.h"
#include "../drivers/keyboard/keyboard.h"
#include "../util.h"
#include "../filesystem/filesystem.h"
#include "../io/log.h"

void readline(unsigned char* line){
    unsigned int line_idx = 0;
    char ch;
    while(1){
        if (kb_buffer_pop(&ch)){
            switch (ch)
            {
            case '\n':
                return;
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

void start_shell(){
    unsigned char* line = kmalloc(SCREEN_COLUMNS + 1);
    newline();
    while (1){
        memset(line,0x00,SCREEN_COLUMNS + 1);
        string_t active_dir = get_active_dir();
        write_string("|-(",3);
        write_string(active_dir.str,active_dir.length);
        write_string(")-> ",4);

        readline(line);
        newline();
        unsigned int len = strlen(line) + 1;
        if (streq(line,"clear",len)) {
            clear_screen();
        }
        else if (streq(line,"exit",len)){
            break;
        }
        else{
            write_string("Command '",9);
            write_string(line,len - 1);
            write_string("' was not found!",16);
            newline();
        }
    }
}