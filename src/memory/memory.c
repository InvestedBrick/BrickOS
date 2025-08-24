#include "memory.h"
#include "../util.h"
#include "../io/log.h"
#include "../vector.h"

static uint32_t page_frame_min;
static uint32_t page_frame_max;
static uint32_t total_alloc;
static uint32_t mem_number_vpages;

uint8_t physical_memory_bitmap[(NUM_PAGE_FRAMES / 8)]; // Update dynamically && bitarray

__attribute__((aligned(0x1000)))
static uint32_t page_dirs[NUM_PAGE_DIRS] [1024] __attribute__((aligned(4096)));
static uint8_t page_dir_used[NUM_PAGE_DIRS];

vector_t shm_obj_vec;

void init_shm_obj_vector(){
    init_vector(&shm_obj_vec);
}

virt_mem_area_t* find_virt_mem_area(virt_mem_area_t* start,uint32_t addr){
    virt_mem_area_t* vma = start;
    while(vma && !(addr >= (uint32_t)vma->addr && addr <= (uint32_t)vma->addr + vma->size)){
        vma = vma->next;
    }

    return vma;
}

shared_object_t* find_shared_object_by_id(uint32_t unique_id){
    for (uint32_t i = 0; i < shm_obj_vec.size;i++){
        shared_object_t* shrd_obj = (shared_object_t*)shm_obj_vec.data[i];
        if (shrd_obj->unique_id == unique_id) 
            return shrd_obj;
    }

    return 0;
}

void pmm_init(uint32_t mem_low, uint32_t mem_high){
    page_frame_min = CEIL_DIV(mem_low,MEMORY_PAGE_SIZE);
    page_frame_max = mem_high / MEMORY_PAGE_SIZE;
    total_alloc = 0;

    memset(physical_memory_bitmap,0,sizeof(physical_memory_bitmap));
}

void mem_change_page_dir(uint32_t* pd){
    pd = (uint32_t*)(((uint32_t)pd) - KERNEL_START);
    mem_set_current_page_dir(pd);
}


void flush_tlb(){
    mem_change_page_dir(mem_get_current_page_dir());
}

void pmm_free_page_frame(uint32_t phys_addr){
    uint32_t frame = phys_addr / MEMORY_PAGE_SIZE;
    uint32_t byte = frame / 8;
    uint32_t bit = frame % 8;
    physical_memory_bitmap[byte] &= ~(1 << bit);
}

void free_user_page_dir(uint32_t* usr_pd){
    for (uint32_t i = 0; i < NUM_PAGE_DIRS;i++){
        if (page_dirs[i] == usr_pd){
            page_dir_used[i] = 0;

            // free page tables
            for(uint32_t pt_idx = 0; pt_idx < 768;pt_idx++){
                uint32_t pt_entry = usr_pd[pt_idx];
                if (pt_entry & PAGE_FLAG_PRESENT){
                    uint32_t pt_phys_addr = pt_entry & 0xfffff000;
                    uint32_t* page_table = (uint32_t*)(pt_phys_addr + KERNEL_START);

                    //Free all pages in the page table
                    for(uint32_t pf_idx = 0; pf_idx < 1024; pf_idx++){
                        uint32_t page_entry = page_table[pf_idx];
                        if (page_entry & PAGE_FLAG_PRESENT){
                            uint32_t page_frame_phys_addr = page_entry & 0xfffff000;
                            pmm_free_page_frame(page_frame_phys_addr);

                            uint32_t virt_addr = (pt_idx<< 22) | (pf_idx << 12);
                            invalidate(virt_addr);
                        }
                    }

                    memset(page_table,0,sizeof(uint32_t) * 1024);
                    //Free the page table
                    pmm_free_page_frame(pt_phys_addr);

                    //mark the page directory as free
                    usr_pd[pt_idx] = 0;
                }
            }
        }
    }
}

uint32_t* create_user_page_dir(){
    for (uint32_t i = 0; i < NUM_PAGE_DIRS;i++){
        if(!page_dir_used[i]){
            page_dir_used[i] = 1;
            uint32_t* pd = page_dirs[i];
            memset(pd,0,sizeof(uint32_t) * 1024);
            // Copy Kernel mapping
            for (uint32_t j = 768; j < 1024;j++){
                pd[j] = initial_page_dir[j];
            }
            pd[1023] = ((uint32_t)pd - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
            
            // Identity map the page directory so the CPU can read it after CR3 is loaded
            uint32_t physical_addr = (uint32_t)pd - KERNEL_START;
            pd[((uint32_t)pd >> 22) & 0x3ff] = physical_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
            return pd;
        }
    }
    return (uint32_t*)0;
}

void sync_page_dirs(){
    for (uint32_t  i = 0; i < NUM_PAGE_DIRS;i++){
        if (page_dir_used[i]){
            uint32_t* page_dir = page_dirs[i];

            for(int j = 768; j < 1023;j++){
                page_dir[j] = initial_page_dir[j] & ~PAGE_FLAG_OWNER;
            }
        }
    }
}

void restore_kernel_memory_page_dir(){
    uint32_t* current_page_dir = mem_get_current_page_dir();
    if (current_page_dir != initial_page_dir){
        mem_change_page_dir(initial_page_dir);
    }
}

void mem_map_page_in_dir(uint32_t* page_dir, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags){
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = virt_addr >> 12 & 0x3ff;

    if (!(page_dir[pd_index] & PAGE_FLAG_PRESENT)){ // if the given page tables are not present -> allocate them
        uint32_t pt_phys_addr = pmm_alloc_page_frame();
        page_dir[pd_index] = pt_phys_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE  | PAGE_FLAG_OWNER | flags;
        
        uint32_t* page_table = (uint32_t*)(pt_phys_addr + KERNEL_START);
        for(uint32_t i = 0; i < 1024;i++){ 
            page_table[i] = 0;
        }
    }

    uint32_t pt_phys_addr = page_dir[pd_index] & 0xfffff000;
    uint32_t* page_table = (uint32_t*)(pt_phys_addr + KERNEL_START);

    page_table[pt_index] = phys_addr | PAGE_FLAG_PRESENT | flags;
}

void mem_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags){
    uint32_t* prev_page_dir = 0;

    if (virt_addr >= KERNEL_MALLOC_START){ // if we are in kernel memory
        prev_page_dir = mem_get_current_page_dir(); // switch to the kernel memory page
        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(initial_page_dir);
        }
    }

    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = virt_addr >> 12 & 0x3ff;

    uint32_t* page_dir = REC_PAGE_DIR;
    uint32_t* page_table = REC_PAGE_TABLE(pd_index);

    if (!(page_dir[pd_index] & PAGE_FLAG_PRESENT)){ // if the given page tables are not present -> allocate them
        uint32_t pt_phys_addr = pmm_alloc_page_frame();
        page_dir[pd_index] = pt_phys_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE  | PAGE_FLAG_OWNER | flags;
        
        invalidate(virt_addr);
        for(uint32_t i = 0; i < 1024;i++){ 
            page_table[i] = 0;
        }
    }
    page_table[pt_index] = phys_addr | PAGE_FLAG_PRESENT | flags;
    mem_number_vpages++;
    invalidate(virt_addr);
    if (prev_page_dir != 0){
        sync_page_dirs();
        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(prev_page_dir);
        }
    }
}

void mem_unmap_page(uint32_t virt_addr){

    uint32_t* prev_page_dir = 0;

    if (virt_addr >= KERNEL_MALLOC_START){ // if we are in kernel memory
        prev_page_dir = mem_get_current_page_dir(); // switch to the kernel memory page
        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(initial_page_dir);
        }
    }
    
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = virt_addr >> 12 & 0x3ff;
    
    uint32_t* page_dir = REC_PAGE_DIR;

    if (page_dir[pd_index] & PAGE_FLAG_PRESENT){ // only free if present
        
        uint32_t* page_table = REC_PAGE_TABLE(pd_index);
        
        // we explicitly do NOT free the page frame 
        page_table[pt_index] = 0x00000000;
        mem_number_vpages--;
        invalidate(virt_addr);
    }
    if (prev_page_dir != 0){
        sync_page_dirs();
        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(prev_page_dir);
        }
    }
}

uint32_t pmm_alloc_page_frame() {

    uint32_t start = page_frame_min / 8 + ((page_frame_min & 7) != 0 ? 1:0);
    uint32_t end = page_frame_max / 8 - ((page_frame_max & 7) != 0 ? 1:0);

    for (uint32_t b  = start; b < end;b++){
        
        uint8_t byte = physical_memory_bitmap[b];
        if (byte == 0xff){ // all 8 frames are in use
            continue;
        }

        for(uint32_t i = 0; i < 8;i++){
            uint8_t used = (byte >> i) & 0x1;

            if(!used){
                byte ^= (-1 ^ byte ) & (1 << i); //set 
                physical_memory_bitmap[b] = byte;
                total_alloc++;

                uint32_t phys_addr = (b * 8 + i) * MEMORY_PAGE_SIZE;
                
                return phys_addr;
            }
        }
    }
    // We can out of physical memory
    panic("System ran out of physical memory");
}

void un_identity_map_first_page_table(){
    initial_page_dir[0] = 0;
    invalidate(0);
}

void init_memory(uint32_t physical_alloc_start, uint32_t mem_high){
    mem_number_vpages = 0;
    initial_page_dir[1023] = ((uint32_t)initial_page_dir - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE ;
    invalidate(0xfffff000);

    pmm_init(physical_alloc_start,mem_high);
    memset(page_dirs,0,MEMORY_PAGE_SIZE * NUM_PAGE_DIRS);
    memset(page_dir_used,0,NUM_PAGE_DIRS);
    // we now own 0xc0000000 - 0xc0ffffff as the kernel
}


