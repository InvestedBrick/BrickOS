#include "io/io.h"
#include "io/log.h"
#include "ACPI/acpi.h"
#include "tables/gdt.h"
#include "kernel_header.h"
#include "memory/memory.h"
#include "screen/screen.h"
#include "memory/kmalloc.h"
#include "tables/syscalls.h"
#include "drivers/PCI/pci.h"
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
#include "ACPI/io_apic.h"
#include <uacpi/sleep.h>
#include <stdint.h>

struct user_process global_kernel_process;
extern uint8_t check_SSE();
extern void enable_SSE();

void finish_up_kernel_proc(){
    global_kernel_process.fd_table[FD_STDIN] = fs_open("dev/kb0",FILE_FLAG_NONE);
    global_kernel_process.fd_table[FD_STDOUT] = fs_open("dev/null",FILE_FLAG_NONE);

}

void create_kernel_process(uint64_t stack_top){
    init_user_process_vector();

    memset(global_kernel_process.fd_table,0,MAX_FDS);

    global_kernel_process.kernel_stack_top = stack_top;
    global_kernel_process.pml4 = mem_get_current_pml4_table();
    global_kernel_process.process_id = get_pid();
    global_kernel_process.vm_areas = 0;
    global_kernel_process.priv_lvl = PRIV_ALUCARD; // the all-powerful
    global_kernel_process.process_name = kmalloc(sizeof("root"));

    memcpy(global_kernel_process.process_name,"root",sizeof("root"));
    global_kernel_process.running = 1;
    
    global_kernel_process.main_thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(global_kernel_process.main_thread,0x0,sizeof(thread_t));
    global_kernel_process.main_thread->tid = get_pid();
    global_kernel_process.main_thread->owner_proc = &global_kernel_process;

    vector_append(&user_process_vector,(vector_data_t)&global_kernel_process);

}

void shutdown(){
    
    restore_kernel_pml4_table();
    log("SHUTTING DOWN...");
    
    cleanup_tmp();
    log("Cleaned up /tmp");
    
    write_to_disk();
    
    
    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)){
        logf("Failed to prepare sleep: %s",uacpi_status_to_string(ret));
        panic("");
    }
    disable_interrupts();

    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(ret)){
        logf("Failed to prepare sleep: %s",uacpi_status_to_string(ret));
        panic("");
    }
    // should be unreachable

    panic("This is the end of the world");
}

void kmain()
{   
    uint64_t stack_top;
    asm volatile ("mov %%rsp, %0" : "=r"(stack_top));
    
    if (check_SSE()) enable_SSE();

    // Serial port setup
    serial_configure_baud_rate(SERIAL_COM1_BASE,3);
    serial_configure_line(SERIAL_COM1_BASE);
    serial_configure_buffer(SERIAL_COM1_BASE);
    serial_configure_modem(SERIAL_COM1_BASE);
    log("Set up serial port");
    
    parse_bootloader_data();

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

    register_basic_interrupts();

    create_kernel_process(stack_top);
    log("Set up kernel process");

    init_scheduler();
    log("Initialized the scheduler");

    init_acpi();
    log("Initialized ACPI");

    discover_ioapics();

    pci_check_all_busses();
    log("Scanned PCI busses");

    save_module_binaries(limine_data.modules);
    log("Saved module binaries");
    
    init_shm_obj_vector();
    log("Initialized shared memory objects vector");

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

    initialize_devices(); // needs the global kernel process
    log("Initialized devices");

    finish_up_kernel_proc(); // needs devices

    // save the modules binaries to "modules/"
    write_module_binaries_to_file();
    log("Wrote module binaries to files in the modules directory");
    
    // Everything is now set up
    run("modules/terminal.bin",nullptr,nullptr,PRIV_STD);

    run("modules/win_man.bin",nullptr,nullptr,PRIV_SPECIAL); // window manager should open dev/kb0

    sys_settimeofday(&global_kernel_process,sys_gettimeofday() + 3600); // add 1h for UTC+1 timezone (mine)

    setup_timer_switch();
    // need to manually enable since run just restores whatever was before that
    log("Shell setup complete");
    enable_interrupts();
    
    panic("Not set up beyond here");

}