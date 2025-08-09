#include "io/io.h"
#include "io/log.h"
#include "screen.h"
#include "tables/gdt.h"
#include "tables/interrupts.h"
#include "drivers/keyboard/keyboard.h"
#include "multiboot.h"
#include "memory/memory.h"
#include "memory/kmalloc.h"
#include "processes/user_process.h"
#include "modules/module_handler.h"
#include "drivers/ATA_PIO/ata.h"
#include "filesystem/filesystem.h"
#include "filesystem/file_operations.h"
#include "drivers/timer/pit.h"
#include "processes/scheduler.h"
#include "tables/syscalls.h"
user_process_t global_kernel_process;
extern unsigned int stack_top;


unsigned int calc_phys_alloc_start(multiboot_info_t* boot_info){
    multiboot_module_t* mods = (multiboot_module_t*) boot_info->mods_addr;
    unsigned int highest_mod_end = 0;

    for (unsigned int i = 0; i < boot_info->mods_count;i++){
        if (mods[i].mod_end > highest_mod_end){
            highest_mod_end = mods[i].mod_end;
        }
    }

    return (highest_mod_end + 0xfff) & ~ 0xfff;
}

void create_kernel_process(){
    memset(global_kernel_process.fd_table,0,MAX_FDS);
    
    global_kernel_process.fd_table[FD_STDIN] = &kb_file;
    global_kernel_process.fd_table[FD_STDOUT] = &screen_file;

    global_kernel_process.kernel_stack = stack_top;
    global_kernel_process.page_dir = mem_get_current_page_dir();
    global_kernel_process.process_id = get_pid();
    global_kernel_process.process_name = kmalloc(sizeof(unsigned char) * 5);
    
    memcpy(global_kernel_process.process_name,"root",sizeof("root"));
    global_kernel_process.running = 1;

    vector_append(&user_process_vector,(unsigned int)&global_kernel_process);
}

void shutdown(){

    restore_kernel_memory_page_dir();
    
    write_to_disk();

    panic("This is the end of the world");
}

void kmain(multiboot_info_t* boot_info)
{   
    disable_cursor();
    clear_screen();
    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);

    log("Set up serial port");

    init_pit(DESIRED_STANDARD_FREQ);
    log("Initialized the PIT");

    // Set up global descriptor table
    init_gdt();
    log("Initialized the GDT");
    
    //Set up Interrupt descriptor table
    init_idt();
    log("Initialited the IDT");
    
    //Initialize keyboard
    init_keyboard();
    log("Initialized keyboard");
    
    unsigned int physical_alloc_start = calc_phys_alloc_start(boot_info);
    init_memory(physical_alloc_start ,boot_info->mem_upper * 1024);
    log("Initialized paged memory");
    
    // Set up kernel malloc
    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");
    
    create_kernel_process();
    log("Set up kernel process");
    
    save_module_binaries(boot_info);
    log("Saved module binaries");
    
    // Fully commit to virtual memory now
    un_identity_map_first_page_table();
    
    init_user_process_vector();
    log("Initialized user process vector");
    
    disable_interrupts(); // We dont want interrupts right now, since we cant correctly return to kernel land once we interrupt
    
    init_disk_driver();
    log("Initialized disk driver");
    
    // set up the filesystem
    init_filesystem();
    log("Initialized the filesystem");

    if (first_time_fs_init){
        create_file(active_dir,"modules",strlen("modules"),FS_TYPE_DIR, FS_FILE_PERM_NONE);
        create_file(active_dir,"home",strlen("home"),FS_TYPE_DIR, FS_FILE_PERM_NONE);
        log("Initialized modules and home directory");
    }
    
    // save the modules binaries to "modules/"
    write_module_binaries_to_file();
    log("Wrote module binaries to files in the modules directory");

    init_scheduler();
    log("Initialized the scheduler");
    
    // Everything is now set up
    
    sys_write(&global_kernel_process,FD_STDOUT,"Hello BrickOS!\n",15);
    sys_write(&global_kernel_process,FD_STDOUT,"Type 'help' for command list\n",29);
    
    // running module
    run("modules/c_test.bin");

    // need to manually enable since run just restores whatever was before that
    enable_interrupts();
    
    panic("Not set up beyond here");

}