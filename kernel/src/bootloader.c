#include "../limine-protocol/include/limine.h"
#include "processes/scheduler.h"
#include "kernel_header.h"
#include "io/log.h"
#include <stddef.h>

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

void parse_bootloader_data(){
    if (!LIMINE_BASE_REVISION_SUPPORTED){
        panic("Limine base revision unsupported");
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

    if (!rsdp_request.response) warn("No XSDP table given");
    else {
        uint64_t rsdp = (uint64_t)rsdp_request.response->address;
        limine_data.rsdp = rsdp - limine_data.hhdm; // is virtual address for base revision 4
    }

    if (module_request.response == NULL) warn("No modules were loaded, likely not intended");
    limine_data.modules = module_request.response;

    limine_data.mmap_data.n_entries = memmap_request.response->entry_count;
    limine_data.mmap_data.memmap_entries = memmap_request.response->entries; 

    if (framebuffer_request.response == NULL 
     || framebuffer_request.response->framebuffer_count < 1){
        panic("No Framebuffer");
    }

    limine_data.framebuffer = framebuffer_request.response->framebuffers[0];

}
