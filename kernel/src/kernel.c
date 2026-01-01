#include "io/io.h"
#include "io/log.h"
#include "tables/gdt.h"
#include "kernel_header.h"
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
#include "../limine-protocol/include/limine.h"

typedef struct XSDP {
 char Signature[8];
 uint8_t Checksum;
 char OEMID[6];
 uint8_t Revision;
 uint32_t RsdtAddress;      // deprecated since version 2.0

 uint32_t Length;
 uint64_t XsdtAddress;
 uint8_t ExtendedChecksum;
 uint8_t reserved[3];
}__attribute__ ((packed)) XSDP_t ;

limine_data_t limine_data;

__attribute__((used, section(".limine_requests")))
volatile static LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 1,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .min_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
    .max_mode = LIMINE_PAGING_MODE_X86_64_4LVL,
};

__attribute__((used,section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used,section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

__attribute__((used,section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_boot_time_request date_at_boot_request = {
    .id = LIMINE_BOOT_TIME_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute((used,section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

struct user_process global_kernel_process;
uint8_t dispatched_user_mode = 0;

void setup_kernel_fds(){
    global_kernel_process.fd_table[FD_STDIN] = fs_open("dev/kb0",FILE_FLAG_NONE);
    global_kernel_process.fd_table[FD_STDOUT] = fs_open("dev/null",FILE_FLAG_NONE);
}

void create_kernel_process(uint64_t stack_top){
    memset(global_kernel_process.fd_table,0,MAX_FDS);

    global_kernel_process.kernel_stack_top = stack_top;
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
    dispatched_user_mode = 0;

    cleanup_tmp();
    log("Cleaned up /tmp");

    write_to_disk();

    panic("This is the end of the world");
}

void kmain()
{   
    uint64_t stack_top;
    asm volatile ("mov %%rsp, %0" : "=r"(stack_top));
    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);
    log("Set up serial port");
    
    if (!LIMINE_BASE_REVISION_SUPPORTED){
        panic("Limine base revision unsupported");
    }

    if (!rsdp_request.response) warn("No XSDP table given");
    else {
        uint64_t rsdp = (uint64_t)rsdp_request.response->address;
        logf("RSDP: %x",rsdp);
        XSDP_t* xsdp = (XSDP_t*)rsdp;
        logf("Physical XSDT addr: %x",xsdp->XsdtAddress);
    }
    if (!hhdm_request.response) panic("No HHDM request response");

    limine_data.hhdm = hhdm_request.response->offset;
    logf("Kernel mapped at offset: %x",limine_data.hhdm);
    if (date_at_boot_request.response){
        limine_data.boot_time = date_at_boot_request.response->boot_time;
        current_timestamp = limine_data.boot_time; // kernel setup is disregarded as it only takes ~2secs
        logf("Boot unix timestamp: %d",current_timestamp);
    }

    if (memmap_request.response == NULL) panic("No memmap recieved");
    if (module_request.response == NULL) warn("No modules were loaded, likely not intended");

    limine_data.mmap_data.n_entries = memmap_request.response->entry_count;
    limine_data.mmap_data.memmap_entries = memmap_request.response->entries; 

    if (framebuffer_request.response == NULL 
     || framebuffer_request.response->framebuffer_count < 1){
        panic("No Framebuffer");
    }

    limine_data.framebuffer = framebuffer_request.response->framebuffers[0];

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
    
    init_memory();
    log("Initialized paged memory");

    init_framebuffer();
    log("Set up framebuffer");

    init_kmalloc(MEMORY_PAGE_SIZE);
    log("Initialized kmalloc");

    save_module_binaries(module_request.response);
    log("Saved module binaries");
    
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
        inode_t* current_dir = get_active_dir();
        create_file(current_dir,"modules",strlen("modules"),FS_TYPE_DIR, FS_FILE_PERM_NONE,PRIV_STD);
        create_file(current_dir,"home",strlen("home"),FS_TYPE_DIR, FS_FILE_PERM_NONE,PRIV_STD);
        create_file(current_dir,"dev",strlen("dev"),FS_TYPE_DIR,FS_FILE_PERM_NONE,PRIV_STD);
        create_file(current_dir,"tmp",strlen("tmp"),FS_TYPE_DIR,FS_FILE_PERM_NONE,PRIV_STD);
        log("Initialized /modules, /home, /dev and /tmp directories");
    }

    create_kernel_process(stack_top);
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

    sys_settimeofday(&global_kernel_process,sys_gettimeofday() + 3600); // add 1h for UTC+1 timezone (mine)

    setup_timer_switch();
    // need to manually enable since run just restores whatever was before that
    log("Shell setup complete");
    dispatched_user_mode = 1;
    enable_interrupts();
    
    panic("Not set up beyond here");

}