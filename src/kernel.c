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
#include "modules/module_handler.h"
#include "drivers/ATA_PIO/ata.h"
#include "filesystem/filesystem.h"
#include "shell/kernel_shell.h"
void kmain(multiboot_info_t* boot_info)
{   
    clear_screen();
    disable_cursor();
    write_string("Hello BrickOS!",15);
    newline();
    write_string("Type 'help' for command list",28);
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
    unsigned int physical_alloc_start = (mod1 + 0xfff) & ~ 0xfff;

    init_memory(physical_alloc_start ,boot_info->mem_upper * 1024);
    log("Initialized paged memory");
    
    // Set up kernel malloc
    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");
    
    save_module_binaries(boot_info);
    log("Saved module binaries");
    
    un_identity_map_first_page_table();

    init_user_process_vector();
    log("Initialized user process vector");
    
    init_disk_driver();
    log("Initialized disk driver");

    init_filesystem();
    log("Initialized the filesystem");

    if (first_time_fs_init){
        create_file(active_dir,"modules",strlen("modules"),FS_TYPE_DIR);
        create_file(active_dir,"home",strlen("home"),FS_TYPE_DIR);
    }

    write_module_binaries_to_file();
    log("Wrote module binaries to files in the modules directory");
    //if (module_count > 0){
    //    unsigned int pid = create_user_process((char*)module_binary_structs[0].start,module_binary_structs[0].size);
    //    free_saved_module_binaries();
    //    user_process_t* process = get_user_process_by_pid(pid);
    //    set_kernel_stack(process->kernel_stack + MEMORY_PAGE_SIZE);
    //    enter_user_mode(); // we are never getting out of here
    //    restore_kernel_memory_page_dir();
    //} 

    //get input
    start_shell();
    log("User returned from shell");
    // write the data to disk when the user exits
    write_to_disk();

    panic("This is the end of the world");
    return;
}