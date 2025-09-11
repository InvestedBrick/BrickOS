#include "io/io.h"
#include "io/log.h"
#include "screen/screen.h"
#include "tables/gdt.h"
#include "tables/interrupts.h"
#include "drivers/PS2/keyboard/keyboard.h"
#include "drivers/PS2/mouse/mouse.h"
#include "drivers/PS2/ps2_controller.h"
#include "multiboot.h"
#include "memory/memory.h"
#include "memory/kmalloc.h"
#include "processes/user_process.h"
#include "modules/module_handler.h"
#include "drivers/ATA_PIO/ata.h"
#include "filesystem/filesystem.h"
#include "filesystem/file_operations.h"
#include "filesystem/devices/devs.h"
#include "filesystem/IPC/pipes.h"
#include "drivers/timer/pit.h"
#include "processes/scheduler.h"
#include "tables/syscalls.h"
user_process_t global_kernel_process;
extern uint32_t stack_top;
uint8_t dispatched_user_mode = 0;

uint32_t calc_phys_alloc_start(multiboot_info_t* boot_info){
    multiboot_module_t* mods = (multiboot_module_t*) boot_info->mods_addr;
    uint32_t highest_mod_end = 0;

    for (uint32_t i = 0; i < boot_info->mods_count;i++){
        if (mods[i].mod_end > highest_mod_end){
            highest_mod_end = mods[i].mod_end;
        }
    }

    return (highest_mod_end + 0xfff) & ~ 0xfff;
}

void setup_kernel_fds(){
    global_kernel_process.fd_table[FD_STDIN] = fs_open("dev/kb0",FILE_FLAG_NONE);
    global_kernel_process.fd_table[FD_STDOUT] = fs_open("dev/null",FILE_FLAG_NONE);
}

void create_kernel_process(){
    memset(global_kernel_process.fd_table,0,MAX_FDS);

    global_kernel_process.kernel_stack = stack_top;
    global_kernel_process.page_dir = mem_get_current_page_dir();
    global_kernel_process.process_id = get_pid();
    global_kernel_process.vm_areas = 0;
    global_kernel_process.priv_lvl = PRIV_ALUCARD; // the all-powerful
    global_kernel_process.process_name = kmalloc(sizeof("root"));

    memcpy(global_kernel_process.process_name,"root",sizeof("root"));
    global_kernel_process.running = 1;
    
    vector_append(&user_process_vector,(uint32_t)&global_kernel_process);

}

void shutdown(){

    restore_kernel_memory_page_dir();

    cleanup_tmp();
    log("Cleaned up /tmp");

    write_to_disk();

    panic("This is the end of the world");
}

void kmain(multiboot_info_t* boot_info)
{   
    if (!(boot_info->flags & (1 << 12))) { // check for RGB graphics mode
        panic("RGB Setup failed!");
    }

    if (!(boot_info->framebuffer_type == 1)){
        panic("Invalid framebuffer type");
    }
    
    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);
    log("Set up serial port");
    
    // Set up global descriptor table
    init_gdt();
    log("Initialized the GDT");
    
    init_pit(DESIRED_STANDARD_FREQ);
    log("Initialized the PIT");
    disable_interrupts(); // We dont want interrupts right now, since we cant correctly return to kernel land once we interrupt

    init_and_test_I8042_controller();
    log("Initialized the I8042 PS/2 controller");

    init_keyboard();
    log("Initialized keyboard");
    
    init_mouse();
    log("Initialized the mouse");
    
    //Set up Interrupt descriptor table
    init_idt();
    log("Initialited the IDT");

    uint32_t physical_alloc_start = calc_phys_alloc_start(boot_info);
    init_memory(physical_alloc_start ,boot_info->mem_upper * 1024);
    log("Initialized paged memory");

    init_framebuffer(boot_info,SCREEN_PIXEL_BUFFER_START);
    log("Set up framebuffer");

    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");
    
    save_module_binaries(boot_info);
    log("Saved module binaries");
    
    // Fully commit to virtual memory now
    un_identity_map_first_page_table();
    
    init_shm_obj_vector();
    log("Initialized shared memory objects vector");

    init_user_process_vector();
    log("Initialized user process vector");

    init_disk_driver();
    log("Initialized disk driver");
    
    // set up the filesystem
    init_filesystem();
    log("Initialized the filesystem");

    if (first_time_fs_init){
        create_file(active_dir,"modules",strlen("modules"),FS_TYPE_DIR, FS_FILE_PERM_NONE,PRIV_STD);
        create_file(active_dir,"home",strlen("home"),FS_TYPE_DIR, FS_FILE_PERM_NONE,PRIV_STD);
        create_file(active_dir,"dev",strlen("dev"),FS_TYPE_DIR,FS_FILE_PERM_NONE,PRIV_STD);
        create_file(active_dir,"tmp",strlen("tmp"),FS_TYPE_DIR,FS_FILE_PERM_NONE,PRIV_STD);
        log("Initialized /modules, /home, /dev and /tmp directories");
    }

    create_kernel_process();
    log("Set up kernel process");

    initialize_devices(); // needs the global kernel process
    log("Initialized devices");

    setup_kernel_fds(); // needs devices

    // save the modules binaries to "modules/"
    write_module_binaries_to_file();
    log("Wrote module binaries to files in the modules directory");

    init_scheduler();
    log("Initialized the scheduler");
    
    // Everything is now set up
    run("modules/terminal.bin",nullptr,nullptr,PRIV_STD);

    run("modules/win_man.bin",nullptr,nullptr,PRIV_SPECIAL); // window manager should open dev/kb0

    setup_timer_switch();
    // need to manually enable since run just restores whatever was before that
    log("Shell setup complete");
    dispatched_user_mode = 1;
    enable_interrupts();
    
    panic("Not set up beyond here");

}