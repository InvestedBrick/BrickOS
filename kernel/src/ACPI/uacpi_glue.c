#include <uacpi/uacpi.h>
#include "../kernel_header.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"
#include "../io/io.h"
#include "../io/log.h"
#include "../drivers/PCI/pci.h"
#include "../tables/interrupts.h"
#include "../processes/scheduler.h"
#include "../processes/spinlocks.h"

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address){
    *out_rsdp_address = limine_data.rsdp;
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len){
    uint64_t aligned_addr = addr & (~(MEMORY_PAGE_SIZE - 1)); // first align addr and fix len, specified in removed comment
    uint64_t real_len = (len + (addr - aligned_addr));
    uint64_t n_pages = CEIL_DIV(real_len,MEMORY_PAGE_SIZE);
    uint64_t virt_addr = limine_data.hhdm + aligned_addr;
    
    for (uint32_t i = 0; i < n_pages; i++){
        mem_map_page(virt_addr + i * MEMORY_PAGE_SIZE, aligned_addr + i * MEMORY_PAGE_SIZE, PAGE_FLAG_WRITE);
    }
    
    return (void *)(virt_addr + (addr - aligned_addr));
}

void uacpi_kernel_unmap(void *addr, uacpi_size len){
    uint64_t virt_addr = (uint64_t)addr & (~(MEMORY_PAGE_SIZE - 1));
    uint64_t offset = (uint64_t)addr - virt_addr;
    uint64_t real_len = (len + offset);
    uint64_t n_pages = CEIL_DIV(real_len,MEMORY_PAGE_SIZE);

    for (uint32_t i = 0; i < n_pages; i++){
        mem_unmap_page(virt_addr + i * MEMORY_PAGE_SIZE);
    }
}

void uacpi_kernel_log(uacpi_log_level lvl , const uacpi_char* str){
    const char* lvl_str;

  switch (lvl) {
  case UACPI_LOG_DEBUG:
    lvl_str = "debug";
    break;
  case UACPI_LOG_TRACE:
    lvl_str = "trace";
    break;
  case UACPI_LOG_INFO:
    lvl_str = "info";
    break;
  case UACPI_LOG_WARN:
    lvl_str = "warn";
    break;
  case UACPI_LOG_ERROR:
    lvl_str = "error";
    break;
  default:
    lvl_str = "invalid";
  }

  logf("[uacpi::%s] %s", lvl_str, str);
    
}

/*
 * Only the above ^^^ API may be used by early table access and
 * UACPI_BAREBONES_MODE.
 */


uacpi_status uacpi_kernel_pci_device_open( uacpi_pci_address address, uacpi_handle *out_handle){
    uacpi_pci_address* addr = (uacpi_pci_address*)kmalloc(sizeof(uacpi_pci_address));
    memcpy(addr,&address,sizeof(uacpi_pci_address));
    *out_handle = addr;
    return UACPI_STATUS_OK;
}
void uacpi_kernel_pci_device_close(uacpi_handle handle){
    kfree(handle);
}

uacpi_status uacpi_kernel_pci_read(uacpi_handle handle, uacpi_size offset, uacpi_u8 width, uacpi_u64* value){
    uacpi_pci_address* addr = (uacpi_pci_address*)handle;
    switch (width)
    {
    case sizeof(uint8_t):
        *value = pci_config_read_word(addr->bus,addr->device,addr->function,offset & ~0x1); // align offset to word size
        *value = (*value >> ((offset & 0x1) ? 8 : 0) & 0xff); // correctly shift into place if higher half of word
        break;
    case sizeof(uint16_t):
        *value = pci_config_read_word(addr->bus,addr->device,addr->function,offset);
        break;
    case sizeof(uint64_t):
        *value = pci_config_read_word(addr->bus,addr->device,addr->function,offset);
        *value |= (pci_config_read_word(addr->bus,addr->device,addr->function, offset + 2) << 16);
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read8( uacpi_handle device, uacpi_size offset, uacpi_u8 *value){
    return uacpi_kernel_pci_read(device,offset,sizeof(uint8_t),(uint64_t*)value);
}
uacpi_status uacpi_kernel_pci_read16( uacpi_handle device, uacpi_size offset, uacpi_u16 *value) {
    return uacpi_kernel_pci_read(device,offset,sizeof(uint16_t),(uint64_t*)value);
}
uacpi_status uacpi_kernel_pci_read32( uacpi_handle device, uacpi_size offset, uacpi_u32 *value){
    return uacpi_kernel_pci_read(device,offset,sizeof(uint32_t),(uint64_t*)value);
}

uacpi_status uacpi_kernel_pci_write(uacpi_handle handle, uacpi_size offset, uacpi_u8 width, uacpi_u64 value){
    uacpi_pci_address* addr = (uacpi_pci_address*)handle;
    switch (width)
    {
    case sizeof(uint8_t):
        uint32_t dword = pci_config_read_word(addr->bus,addr->device,addr->function,offset & ~0x3);
        uint32_t shift = (offset & 0x3) * 8;
        dword &= ~(0xff << shift);
        dword |= (value & 0xff) << shift; // only overwrite the byte we want to change
        pci_config_write_dword(addr->bus, addr->device,addr->function,offset & ~0x3,dword);
        break;
    case sizeof(uint16_t):
        uint32_t dword = pci_config_read_word(addr->bus,addr->device,addr->function,offset & ~0x3);
        uint32_t shift = (offset & 0x2) * 8;
        dword &= ~(0xffff << shift);
        dword |= (value & 0xffff) << shift;
        pci_config_write_dword(addr->bus, addr->device,addr->function,offset & ~0x3,dword);
        break;
    case sizeof(uint32_t):
        pci_config_write_dword(addr->bus, addr->device,addr->function,offset,value);
        break;
    default:
        return UACPI_STATUS_INVALID_ARGUMENT;
    }

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8( uacpi_handle device, uacpi_size offset, uacpi_u8 value ){
    return uacpi_kernel_pci_write(device,offset,sizeof(uint8_t),value);
}
uacpi_status uacpi_kernel_pci_write16( uacpi_handle device, uacpi_size offset, uacpi_u16 value ){
    return uacpi_kernel_pci_write(device,offset,sizeof(uint16_t),value);
}
uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value ){
    return uacpi_kernel_pci_write(device,offset,sizeof(uint32_t),value);
}

uacpi_status uacpi_kernel_io_map( uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle){
    *out_handle = (uacpi_handle)base; // no mapping, just saving
    return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle handle){ (void)0; }

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value){
    *out_value = inb((uint16_t)handle + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value){
    *out_value = inw((uint16_t)handle + offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value){
    *out_value = inl((uint16_t)handle + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value){
    outb((uint16_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value){
    outw((uint16_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value){
    outl((uint16_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_alloc(uacpi_size size){ return kmalloc((uint32_t)size); }

void uacpi_kernel_free(void *mem){ kfree(mem); }

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void){
    return ticks * 1000 * 1000; // only works because ticks are in ms
}

void uacpi_kernel_stall(uacpi_u8 usec){
    uint64_t start = uacpi_kernel_get_nanoseconds_since_boot();
    if (!start) return;

    uint64_t end = start +  usec * 1000;
    while (uacpi_kernel_get_nanoseconds_since_boot() < end) invoke_scheduler();
}


void uacpi_kernel_sleep(uacpi_u64 msec){
    uint64_t start = ticks;
    if (!start) return;
    while (ticks < (start + msec)) invoke_scheduler();
}

uacpi_handle uacpi_kernel_create_mutex(){
    mutex_t* mutex = kmalloc(sizeof(mutex_t)); 
    mutex_init(mutex);
    return (uacpi_handle)mutex;
}
void uacpi_kernel_free_mutex(uacpi_handle handle){
    kfree(handle);
}

uacpi_handle uacpi_kernel_create_event(){
    semaphore_t* sem = (semaphore_t*)kmalloc(sizeof(semaphore_t));
    semaphore_init(sem,0);
    return (uacpi_handle)sem;
}
void uacpi_kernel_free_event(uacpi_handle handle){ kfree(handle); }

uacpi_thread_id uacpi_kernel_get_thread_id(void){
    thread_t* curr_thread = get_current_thread();
    return (uacpi_thread_id)curr_thread->tid;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout){
    mutex_t* mutex = (mutex_t*)handle;
    uint32_t adj_timeout = timeout;
    if (timeout == 0xfff) adj_timeout = (uint32_t)-1; //effectively infinite
    if (!mutex_wait(mutex,adj_timeout)) return UACPI_STATUS_TIMEOUT;

    return UACPI_STATUS_OK;
}
void uacpi_kernel_release_mutex(uacpi_handle handle){
    mutex_t* mutex = (mutex_t*)handle;
    mutex_signal(mutex);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout){
    semaphore_t* sem = (semaphore_t*)handle;
    uint32_t adj_timeout = timeout;
    if (timeout = 0xffff) adj_timeout = (uint32_t)-1;
    if (!semaphore_wait(sem,adj_timeout)) return UACPI_FALSE;

    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle handle){
    semaphore_t* sem = (semaphore_t*)handle;
    semaphore_signal(sem);
}

void uacpi_kernel_reset_event(uacpi_handle handle){
    semaphore_t* sem = (semaphore_t*)handle;
    semaphore_init(sem,0);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* req){
    switch (req->type)
    {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        break;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        errorf("Fatal firmware error: type{%d} code{%d} arg{%d}",req->fatal.type,req->fatal.code,req->fatal.arg);
        break;
    }
    return UACPI_STATUS_OK;
}

//TODO:
/*
 * Install an interrupt handler at 'irq', 'ctx' is passed to the provided
 * handler for every invocation.
 *
 * 'out_irq_handle' is set to a kernel-implemented value that can be used to
 * refer to this handler from other API.
 */
uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
);

/*
 * Uninstall an interrupt handler. 'irq_handle' is the value returned via
 * 'out_irq_handle' during installation.
 */
uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
);

uacpi_handle uacpi_kernel_create_spinlock(void){
    Spinlock* lock = (Spinlock*)kmalloc(sizeof(Spinlock));
    spinlock_init(lock);
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle){ kfree(handle); }

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle){
    Spinlock* lock = (Spinlock*)handle;
    uacpi_cpu_flags flags;

    asm volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");

    spinlock_aquire(handle);

    return flags;
}
void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags){
    spinlock_release(handle);

    asm volatile("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}

/*
 * Schedules deferred work for execution.
 * Might be invoked from an interrupt context.
 */
uacpi_status uacpi_kernel_schedule_work( uacpi_work_type, uacpi_work_handler, uacpi_handle ctx){
    panic("schedule work !TODO!");
    return UACPI_STATUS_OK;
}

/*
 * Waits for two types of work to finish:
 * 1. All in-flight interrupts installed via uacpi_kernel_install_interrupt_handler
 * 2. All work scheduled via uacpi_kernel_schedule_work
 *
 * Note that the waits must be done in this order specifically.
 */
uacpi_status uacpi_kernel_wait_for_work_completion(){
    panic("schedule work !TODO!");
    return UACPI_STATUS_OK;
}
