#include "memory.h"
#include "../util.h"
#include "../io/log.h"
static unsigned int page_frame_min;
static unsigned int page_frame_max;
static unsigned int total_alloc;
static unsigned int mem_number_vpages;

unsigned char physical_memory_bitmap[(NUM_PAGE_FRAMES / 8)]; // Update dynamically && bitarray

__attribute__((aligned(0x1000)))
static unsigned int page_dirs[NUM_PAGE_DIRS] [1024];
static unsigned char page_dir_used[NUM_PAGE_DIRS];

void pmm_init(unsigned int mem_low, unsigned int mem_high){
    page_frame_min = CEIL_DIV(mem_low,MEMORY_PAGE_SIZE);
    page_frame_max = mem_high / MEMORY_PAGE_SIZE;
    total_alloc = 0;

    memset(physical_memory_bitmap,0,sizeof(physical_memory_bitmap));
}

void mem_change_page_dir(unsigned int* pd){
    pd = (unsigned int*)(((unsigned int)pd) - KERNEL_START);
    mem_set_current_page_dir(pd);
    log("Called asm func to set current page dir");
}

void pmm_free_page_frame(unsigned int phys_addr){
    unsigned int frame = phys_addr / MEMORY_PAGE_SIZE;
    unsigned int byte = frame / 8;
    unsigned int bit = frame % 8;
    physical_memory_bitmap[byte] &= ~(1 << bit);
}

void free_user_page_dir(unsigned int* usr_pd){
    for (unsigned int i = 0; i < NUM_PAGE_DIRS;i++){
        if (page_dirs[i] == usr_pd){
            page_dir_used[i] = 0;

            // free page tables
            for(unsigned int pt_idx = 0; pt_idx < 768;pt_idx++){
                unsigned int pt_entry = usr_pd[pt_idx];
                if (pt_entry & PAGE_FLAG_PRESENT){
                    unsigned int pt_phys_addr = pt_entry & 0xfffff000;
                    unsigned int* page_table = (unsigned int*)(pt_phys_addr + KERNEL_START);

                    //Free all pages in the page table
                    for(unsigned int pf_idx = 0; pf_idx < 1024; pf_idx++){
                        unsigned int page_entry = page_table[pf_idx];
                        if (page_entry & PAGE_FLAG_PRESENT){
                            unsigned int page_frame_phys_addr = page_entry & 0xfffff000;
                            pmm_free_page_frame(page_frame_phys_addr);

                            unsigned int virt_addr = (pt_idx<< 22) | (pf_idx << 12);
                            invalidate(virt_addr);
                        }
                    }

                    memset(page_table,0,sizeof(unsigned int) * 1024);
                    //Free the page table
                    pmm_free_page_frame(pt_phys_addr);

                    //mark the page directory as free
                    usr_pd[pt_idx] = 0;
                }
            }
        }
    }
}

unsigned int* create_user_page_dir(){
    for (unsigned int i = 0; i < NUM_PAGE_DIRS;i++){
        if(!page_dir_used[i]){
            page_dir_used[i] = 1;
            unsigned int* pd = page_dirs[i];
            memset(pd,0,sizeof(unsigned int) * 1024);
            // Copy Kernel mapping
            for (unsigned int j = 768; j < 1024;j++){
                pd[j] = initial_page_dir[j];
            }
            pd[1023] = ((unsigned int)pd - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
            
            // Identity map the page directory so the CPU can read it after CR3 is loaded
            unsigned int physical_addr = (unsigned int)pd - KERNEL_START;
            pd[((unsigned int)pd >> 22) & 0x3ff] = physical_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
            return pd;
        }
    }
    return (unsigned int*)0;
}

void sync_page_dirs(){
    for (unsigned  i = 0; i < NUM_PAGE_DIRS;i++){
        if (page_dir_used[i]){
            unsigned int* page_dir = page_dirs[i];

            for(int j = 768; j < 1023;j++){
                page_dir[j] = initial_page_dir[j] & ~PAGE_FLAG_OWNER;
            }
        }
    }
}

void restore_kernel_memory_page_dir(){
    unsigned int* current_page_dir = mem_get_current_page_dir();
    if (current_page_dir != initial_page_dir){
        mem_change_page_dir(initial_page_dir);
    }
}

void mem_map_page(unsigned int virt_addr, unsigned int phys_addr, unsigned int flags){
    unsigned int* prev_page_dir = 0;

    if (virt_addr >= KERNEL_MALLOC_START){ // if we are in kernel memory
        prev_page_dir = mem_get_current_page_dir(); // switch to the kernel memory page
        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(initial_page_dir);
        }
    }

    unsigned int pd_index = virt_addr >> 22;
    unsigned int pt_index = virt_addr >> 12 & 0x3ff ;

    unsigned int* page_dir = REC_PAGE_DIR;
    unsigned int* page_table = REC_PAGE_TABLE(pd_index);

    if (!(page_dir[pd_index] & PAGE_FLAG_PRESENT)){ // if the given page is not present -> allocate it
        unsigned int pt_phys_addr = pmm_alloc_page_frame();
        page_dir[pd_index] = pt_phys_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE  | PAGE_FLAG_OWNER | flags;

        invalidate(virt_addr);
        for(unsigned int i = 0; i < 1024;i++){ 
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

unsigned int pmm_alloc_page_frame() {

    unsigned int start = page_frame_min / 8 + ((page_frame_min & 7) != 0 ? 1:0);
    unsigned int end = page_frame_max / 8 - ((page_frame_max & 7) != 0 ? 1:0);

    for (unsigned int b  = start; b < end;b++){
        
        unsigned char byte = physical_memory_bitmap[b];
        if (byte == 0xff){ // all 8 frames are in use
            continue;
        }

        for(unsigned int i = 0; i < 8;i++){
            unsigned char used = (byte >> i) & 0x1;

            if(!used){
                byte ^= (-1 ^ byte ) & (1 << i); //set 
                physical_memory_bitmap[b] = byte;
                total_alloc++;

                unsigned int phys_addr = (b * 8 * i) * MEMORY_PAGE_SIZE;
                return phys_addr;
            }
        }
    }
    return 0;
}

void un_identity_map_first_4MB(){
    initial_page_dir[0];
    invalidate(0);
}


void init_memory(unsigned int physical_alloc_start, unsigned int mem_high){
    mem_number_vpages = 0;
    initial_page_dir[0] = 0;
    invalidate(0);
    initial_page_dir[1023] = ((unsigned int)initial_page_dir - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE ;
    invalidate(0xfffff000);

    pmm_init(physical_alloc_start,mem_high);
    memset(page_dirs,0,MEMORY_PAGE_SIZE * NUM_PAGE_DIRS);
    memset(page_dir_used,0,NUM_PAGE_DIRS);
    // we now own 0xc0000000 - 0xc0ffffff as the kernel
}


