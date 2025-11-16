#include "io/io.h"
#include "io/log.h"
#include "multiboot.h"
#include "tables/gdt.h"
#include "memory/memory.h"
#include "screen/screen.h"
#include "memory/kmalloc.h"
#include "tables/syscalls.h"
#include "drivers/timer/pit.h"
#include "tables/interrupts.h"
#include "drivers/ATA_PIO/ata.h"
#include "processes/scheduler.h"
#include "filesystem/IPC/pipes.h"
#include "filesystem/filesystem.h"
#include "processes/user_process.h"
#include "modules/module_handler.h"
#include "filesystem/devices/devs.h"
#include "drivers/PS2/mouse/mouse.h"
#include "filesystem/file_operations.h"
#include "drivers/PS2/ps2_controller.h"
#include "drivers/PS2/keyboard/keyboard.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../limine/limine.h"


__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);


__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 1,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_date_at_boot_request date_at_boot_request = {
    .id = LIMINE_DATE_AT_BOOT_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_request_end_marker[] = LIMINE_REQUESTS_END_MARKER;



user_process_t global_kernel_process;
uint32_t stack_top = 0; //TODO: adjust to limine
uint8_t dispatched_user_mode = 0;

void setup_kernel_fds(){
    global_kernel_process.fd_table[FD_STDIN] = fs_open("dev/kb0",FILE_FLAG_NONE);
    global_kernel_process.fd_table[FD_STDOUT] = fs_open("dev/null",FILE_FLAG_NONE);
}

void create_kernel_process(){
    memset(global_kernel_process.fd_table,0,MAX_FDS);

    global_kernel_process.kernel_stack = stack_top;
    global_kernel_process.pml4 = mem_get_current_pml4_table();
    global_kernel_process.process_id = get_pid();
    global_kernel_process.vm_areas = 0;
    global_kernel_process.priv_lvl = PRIV_ALUCARD; // the all-powerful
    global_kernel_process.process_name = kmalloc(sizeof("root"));

    memcpy(global_kernel_process.process_name,"root",sizeof("root"));
    global_kernel_process.running = 1;
    
    vector_append(&user_process_vector,(vector_data_t)&global_kernel_process);

}

void shutdown(){

    restore_kernel_pml4_table();

    cleanup_tmp();
    log("Cleaned up /tmp");

    write_to_disk();

    panic("This is the end of the world");
}

void kmain(multiboot_info_t* boot_info)
{   
    
    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);
    log("Set up serial port");
    
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)){
        panic("Limine base revision unsupported");
    }

    if (date_at_boot_request.response){
        uint64_t timestamp = date_at_boot_request.response->timestamp;
        logf("Seconds since 1970: %d",timestamp);
    }

    if (framebuffer_request.response == NULL 
     || framebuffer_request.response->framebuffer_count < 1){
        panic("No Framebuffer");
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    panic("End of the world");

    // Set up global descriptor table
    init_gdt();
    log("Initialized the GDT");
    
    init_pit(DESIRED_STANDARD_FREQ);
    log("Initialized the PIT");
    disable_interrupts(); // We dont want interrupts right now, since we cant correctly return to kernel land once we interrupt

    //Set up Interrupt descriptor table
    init_idt();
    log("Initialized the IDT");

    init_and_test_I8042_controller();
    log("Initialized the I8042 PS/2 controller");

    init_memory(0/*TODO:*/ ,boot_info->mem_upper * 1024); // mem_upper is in kb
    log("Initialized paged memory");

    init_framebuffer(boot_info,SCREEN_PIXEL_BUFFER_START);
    log("Set up framebuffer");

    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");
    
    save_module_binaries(boot_info);
    log("Saved module binaries");
    
    // Fully commit to virtual memory now
    //un_identity_map_first_page_table();
    
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