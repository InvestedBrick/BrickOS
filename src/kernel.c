#include "io/io.h"
#include "io/log.h"
#include "screen.h"
#include "tables/gdt.h"
#include "tables/interrupts.h"
#include "drivers/keyboard/keyboard.h"
#include "multiboot.h"
#include "memory/memory.h"
#include "memory/kmalloc.h"
#include "user/user_process.h"
#include "tables/tss.h"
#include "util.h"

char program[MEMORY_PAGE_SIZE] = {0};

void kmain(multiboot_info_t* boot_info)
{   
    const char* msg = "Hello BrickOS!";
    unsigned int n_mods = boot_info->mods_count;
   
    multiboot_module_t* modules = (multiboot_module_t*) (boot_info->mods_addr);
    unsigned int size = modules[0].mod_end - modules[0].mod_start;
    memcpy(program,(char*)modules[0].mod_start,size);

    clear_screen();
    write_string(msg,15);

    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);

    log("Set up serial port");
    // Set up global descriptor table
    init_gdt();
    log("Initialized the GDT");

    //Set up Interrupt descriptor table
    init_idt();
    log("Initialited the IDT");

    //Initialize keyboard
    init_keyboard();
    log("Initialized keyboard");

    // calculate memory region
    unsigned int mod1 = *(unsigned int*) (boot_info->mods_addr + 4);
    unsigned int physical_alloc_start = (mod1 - 0xfff) & ~ 0xfff;

    init_memory(physical_alloc_start,boot_info->mem_upper);
    log("Initialized paged memory");
    
    // Set up kernel malloc
    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");
    
    init_user_process_vector();
    log("Initialized user process vector");
    
    //multiboot_module_t* modules = (multiboot_module_t*) (boot_info->mods_addr);
    //unsigned int mod_count = boot_info->mods_count;

    if (n_mods > 0){
        log("Found a module");
        //unsigned char* binary = (unsigned char*) modules[0].mod_start;
        //unsigned int size = modules[0].mod_end - modules[0].mod_start;
        // set up teh user process and load the binary into memory
        log("Before User process");
        unsigned int pid = create_user_process(program,size);
        log("Created User process");
        user_process_t* process = get_user_process_by_pid(pid);
        set_kernel_stack(process->kernel_stack + MEMORY_PAGE_SIZE);
        log("Set up kernel stack");
        enter_user_mode(); // we are never getting out of here
        restore_kernel_memory_page_dir();
    } 
    //un_identity_map_first_4MB(); // can no longer use boot info now

    //get input
    while(1){
        while(!kb_buffer_is_empty())
            handle_screen_input();
        asm ("hlt");
    }
    return;
}