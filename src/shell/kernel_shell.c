#include "kernel_shell.h"
#include "../screen.h"
#include "../memory/kmalloc.h"
#include "../drivers/keyboard/keyboard.h"
#include "../util.h"
#include "../filesystem/filesystem.h"
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

void start_shell(){
    unsigned char* line = kmalloc(SCREEN_COLUMNS + 1);
    newline(); 
    command_t comd;
    while (1){
        memset(line,0x00,SCREEN_COLUMNS + 1);
        write_string("|-(",3);
        string_t curr_dir = get_active_dir();
        write_string(curr_dir.str,curr_dir.length);
        write_string(")-> ",4);

        readline(line);
        newline();
        unsigned int len = strlen(line);

        comd = parse_line(line,len);
        
        if(!comd.command.length) continue; // no need to free or parse what is not there
        
        if (streq(comd.command.str,"clear",sizeof("clear"))) {
            clear_screen();
        }
        else if (streq(comd.command.str,"exit",sizeof("exit"))){
            break;
        }
        else if (streq(comd.command.str,"ls",sizeof("ls"))){
            string_array_t* names = get_all_names_in_dir(active_dir);

            if (names) {

                for(unsigned int i = 0; i < names->n_strings;i++){
                    write_string(names->strings[i].str,names->strings[i].length);
                    newline();
                }
                
                free_string_arr(names);
            } 

        }
        else if (streq(comd.command.str,"mkdir",sizeof("mkdir"))){
            if (!comd.args) {write_string("Expected name of directory",26); newline();}
            else{
                create_directory(active_dir,comd.args[0].str,comd.args[0].length);
            }
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