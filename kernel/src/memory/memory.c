#include "memory.h"
#include "../util.h"
#include "../io/log.h"
#include "../vector.h"
#include "../kernel_header.h"
#include "../../limine-protocol/include/limine.h"

static uint64_t total_alloced_pages;
static uint64_t total_pages;
static uint64_t mem_number_vpages;
static uint64_t mem_phys_alloc_start;
static uint8_t* memory_bitmap;
static uint8_t pml4_bitmap[N_PML4_TABLES / 8];
vector_t shm_obj_vec;
static uint64_t* kernel_pml4;

void init_shm_obj_vector(){
    init_vector(&shm_obj_vec);
}

virt_mem_area_t* find_virt_mem_area(virt_mem_area_t* start,uint64_t addr){
    virt_mem_area_t* vma = start;
    while(vma && !(addr >= (uint64_t)vma->addr && addr <= (uint64_t)vma->addr + vma->size)){
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

void append_shared_object(shared_object_t* shrd_obj){
    vector_append(&shm_obj_vec,(vector_data_t)shrd_obj);
}

uint64_t linear_phys_to_virt(uint64_t phys){
    return phys + HHDM; 
}
uint64_t linear_virt_to_phys(uint64_t virt){
    return virt - HHDM; 
}

uint64_t* get_lower_table(uint64_t table_entry){
    return (uint64_t*)linear_phys_to_virt(PTE_GET_ADDR(table_entry));
}

uint64_t virt_to_phys(uint64_t virt_addr){
    uint32_t pml4_idx = PML4E(virt_addr);
    uint32_t pdpt_idx = PDPTE(virt_addr);
    uint32_t pd_idx   = PDE(virt_addr);
    uint32_t pt_idx   = PTE(virt_addr);

    uint64_t* pml4 = (uint64_t*)mem_get_current_pml4_table();
    if (!(pml4[pml4_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pdpt = (uint64_t*)get_lower_table(pml4[pml4_idx]);
    if (!(pdpt[pdpt_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pd   = (uint64_t*)get_lower_table(pdpt[pdpt_idx]);
    if (!(pd[pd_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pt   = (uint64_t*)get_lower_table(pd[pd_idx]);
    if (!(pt[pt_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    return (CANONICALIZE(PTE_GET_ADDR(pt[pt_idx]))) | (virt_addr & 0xfff);
}

void pmm_init(uint64_t phys_alloc_start, uint64_t mem_high){
    mem_phys_alloc_start = phys_alloc_start;
    logf("phys alloc start: %x",phys_alloc_start);
    logf("available memory: %d bytes", mem_high - phys_alloc_start);
    total_pages = (mem_high - phys_alloc_start) / MEMORY_PAGE_SIZE;
    logf("Total pages: %d",total_pages);
    
    uint64_t bitmap_size = CEIL_DIV(total_pages, 8);
    total_alloced_pages = 0;

    memory_bitmap = (uint8_t*)linear_phys_to_virt(phys_alloc_start);
    memset(memory_bitmap,0,bitmap_size);
    // reserve the kernel pages
    uint64_t kernel_pages = (ALIGN_UP(bitmap_size,MEMORY_PAGE_SIZE)) / MEMORY_PAGE_SIZE;

    // maps all of the kernel pages / modules etc as unusable
    logf("Reserving %d kernel pages",kernel_pages);
    for (uint64_t i = 0; i < kernel_pages;i++){
        memory_bitmap[i / 8] |= (1 << (i % 8));
    }

}

uint64_t* mem_get_current_pml4_table() {
    //TODO: fix to make this return the correct pml4
    uint64_t phys = mem_get_current_pml4_phys();
    return (uint64_t*)linear_phys_to_virt(phys);
}

void mem_set_current_pml4_table(uint64_t* virt) {
    uint64_t phys = virt_to_phys((uint64_t)virt);
    mem_set_current_pml4_phys((uint64_t*)phys);
}

void pmm_free_page_frame(uint64_t phys_addr){
    uint64_t frame = (phys_addr - mem_phys_alloc_start) / MEMORY_PAGE_SIZE;
    uint64_t byte = frame / 8;
    uint64_t bit = frame % 8;
    memory_bitmap[byte] &= ~(1 << bit);
    if (total_alloced_pages > 0) total_alloced_pages--;
}

void free_table_entry(uint64_t* tbl,uint32_t tbl_idx){
    uint64_t phys_page = tbl[tbl_idx] & ~0xFFF;
    pmm_free_page_frame(phys_page);
    tbl[tbl_idx] = 0;
}

void free_user_pml4_table(uint64_t* user_pml4) {
    for (uint32_t pml4_idx = 0; pml4_idx < KERNEL_SHARED_MAPPING_IDX; pml4_idx++) {
        if (!(user_pml4[pml4_idx] & PAGE_FLAG_PRESENT)) continue;

        uint64_t* pdpt = (uint64_t*)(linear_phys_to_virt(user_pml4[pml4_idx] & ~0xFFF));

        for (uint32_t pdpt_idx = 0; pdpt_idx < ENTRIES_PER_TABLE; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PAGE_FLAG_PRESENT)) continue;

            uint64_t* pd = (uint64_t*)(linear_phys_to_virt(pdpt[pdpt_idx] & ~0xFFF));

            for (uint32_t pd_idx = 0; pd_idx < ENTRIES_PER_TABLE; pd_idx++) {
                if (!(pd[pd_idx] & PAGE_FLAG_PRESENT)) continue;

                uint64_t* pt = (uint64_t*)(linear_phys_to_virt(pd[pd_idx] & ~0xFFF));

                for (uint32_t pt_idx = 0; pt_idx < ENTRIES_PER_TABLE; pt_idx++) {
                    if (pt[pt_idx] & PAGE_FLAG_PRESENT) {
                        free_table_entry(pt,pt_idx);
                    }
                }
                free_table_entry(pd,pd_idx);
            }
            free_table_entry(pdpt,pdpt_idx);
        }
        free_table_entry(user_pml4,pml4_idx);
    }

    uint32_t bitmap_idx = ((uint64_t)user_pml4 - KERNEL_PML4_TABLES_START) / MEMORY_PAGE_SIZE;
    uint32_t byte = bitmap_idx / 8;
    uint32_t bit  = bitmap_idx % 8;
    pml4_bitmap[byte] &= ~(1 << bit);

    mem_unmap_page((uint64_t)user_pml4);
    uint64_t pml4_phys = virt_to_phys((uint64_t)user_pml4);
    pmm_free_page_frame(pml4_phys);
}

uint64_t* create_user_pml4_table(){
    for (uint32_t i = 0; i < (N_PML4_TABLES / 8);i++){
        uint8_t byte = pml4_bitmap[i];
        if (byte == 0xff) continue;

        for (uint32_t j = 0; j < 8;j++){
            uint8_t used = (byte >> j) & 0x1;
            if (used) continue;

            byte |= (1 << j); //set 
            pml4_bitmap[i] = byte;

            uint64_t pml4_phys = pmm_alloc_page_frame();
            uint64_t virt_addr = KERNEL_PML4_TABLES_START + (i * 8 + j) * MEMORY_PAGE_SIZE;
            mem_map_page(virt_addr,pml4_phys,PAGE_FLAG_WRITE); // we can't use kmalloc since that does not guarantee to be page aligned
            uint64_t* pml4 = (uint64_t*)virt_addr;
            memset(pml4,0,TABLE_SIZE);
            // Copy Kernel mapping
            for (uint32_t j = KERNEL_SHARED_MAPPING_IDX; j < ENTRIES_PER_TABLE;j++){
                pml4[j] = kernel_pml4[j];
            }
            return pml4;
        }
    }

    warn("Not enough pml4 tables");
    return 0;
        
}

void restore_kernel_pml4_table(){
    uint64_t* current_pml4 = mem_get_current_pml4_table();
    if (current_pml4 != kernel_pml4){
        mem_set_current_pml4_table(kernel_pml4);
    }
}

uint64_t* create_table_if_not_present(uint64_t* tbl,uint32_t tbl_idx,uint32_t flags){
    if ((tbl[tbl_idx] & PAGE_FLAG_PRESENT)) return get_lower_table(tbl[tbl_idx]);
        
    uint64_t phys_addr = pmm_alloc_page_frame();
    uint64_t virt_addr = linear_phys_to_virt(phys_addr);
    tbl[tbl_idx] = phys_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE  | (flags & PAGE_FLAG_USER);
    invalidate(virt_addr);

    memset((void*)virt_addr,0x0,MEMORY_PAGE_SIZE);
    return (uint64_t*)virt_addr;
}

void mem_map_page_in_pml4(uint64_t* pml4, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags) {

    uint64_t* old_pml4 = mem_get_current_pml4_table();
    uint8_t switched_pml4 = 0;
    if (pml4 != old_pml4) {
        mem_set_current_pml4_table(pml4);
        switched_pml4 = 1;
    }

    uint32_t pml4_idx = PML4E(virt_addr);
    uint32_t pdpt_idx = PDPTE(virt_addr);
    uint32_t pd_idx   = PDE(virt_addr);
    uint32_t pt_idx   = PTE(virt_addr);

    uint64_t* pdpt = create_table_if_not_present(pml4,pml4_idx,flags);
    
    uint64_t* pd =  create_table_if_not_present(pdpt,pdpt_idx,flags);

    uint64_t* pt =  create_table_if_not_present(pd,pd_idx,flags);

    pt[pt_idx] = (phys_addr & ~0xfff) | PAGE_FLAG_PRESENT | flags;
    mem_number_vpages++;
    invalidate(virt_addr);

    if (switched_pml4) {
        mem_set_current_pml4_table(old_pml4);
    }

}

void mem_map_page(uint64_t virt_addr, uint64_t phys_addr, uint32_t flags){
    uint64_t* prev_pml4_table = 0;

    if (virt_addr >= HHDM){ // if we are in kernel memory
        prev_pml4_table = mem_get_current_pml4_table(); // switch to the kernel memory page
        if (prev_pml4_table != kernel_pml4){
            mem_set_current_pml4_table(kernel_pml4);
        }
    }

    mem_map_page_in_pml4((uint64_t*)mem_get_current_pml4_table(), virt_addr, phys_addr, flags);

    if (prev_pml4_table != 0){
        mem_set_current_pml4_table(prev_pml4_table);
    }
}

void mem_unmap_page(uint64_t virt_addr){

    uint64_t* prev_pml4_table = 0;

    if (virt_addr >= HHDM){ // if we are in kernel memory
        prev_pml4_table = mem_get_current_pml4_table(); // switch to the kernel memory page
        if (prev_pml4_table != kernel_pml4){
            mem_set_current_pml4_table(kernel_pml4);
        }
    }
    
    uint32_t pml4_idx = PML4E(virt_addr);
    uint32_t pdpt_idx = PDPTE(virt_addr);
    uint32_t pd_idx   = PDE(virt_addr);
    uint32_t pt_idx   = PTE(virt_addr);
    
    uint64_t* pml4 = (uint64_t*)mem_get_current_pml4_table();
    
    if (!(pml4[pml4_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pdpt = (uint64_t*)get_lower_table(pml4[pml4_idx]);
    if (!(pdpt[pdpt_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pd = (uint64_t*)get_lower_table(pdpt[pdpt_idx]);
    if (!(pd[pd_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pt = (uint64_t*)get_lower_table(pd[pd_idx]);
    if (!(pt[pt_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    pt[pt_idx] = 0x0;
    mem_number_vpages--;
    invalidate(virt_addr);

    uint8_t pt_empty = 1;
    for (int i = 0; i < (ENTRIES_PER_TABLE); i++) {
        if (pt[i] & PAGE_FLAG_PRESENT) { pt_empty = 0; break; }
    }
    if (pt_empty) {
        uint64_t phys = linear_virt_to_phys((uint64_t)pt);
        pd[pd_idx] = 0;
        pmm_free_page_frame(phys);
    }
    //TODO: finish freeing for all layers
invalid_unmap:
    if (prev_pml4_table != 0){
        mem_set_current_pml4_table(prev_pml4_table);
    }
}

uint64_t pmm_alloc_page_frame() {
    uint32_t n_bytes = CEIL_DIV(total_pages,8);
    for (uint32_t b = 0; b < n_bytes;b++){
        
        uint8_t byte = memory_bitmap[b];
        if (byte == 0xff){ // all 8 frames are in use
            continue;
        }

        for(uint32_t i = 0; i < 8;i++){
            uint8_t used = (byte >> i) & 0x1;

            if(!used){
                byte |= (1 << i); //set 
                memory_bitmap[b] = byte;
                total_alloced_pages++;

                uint64_t phys_addr = (b * 8 + i) * MEMORY_PAGE_SIZE + mem_phys_alloc_start;
                
                return phys_addr;
            }
        }
    }

    panic("System ran out of physical memory");
    return 0;
}

void get_usable_memory_region(uint64_pair_t* pair){
    struct limine_memmap_entry **mmap = limine_data.mmap_data.memmap_entries; 
    uint64_t n_entries = limine_data.mmap_data.n_entries;
    uint64_t largest_size = 0;
    for(uint64_t i = 0; i < n_entries;i++){
        if (mmap[i]->type == LIMINE_MEMMAP_USABLE && mmap[i]->length > largest_size) {
            pair->first = mmap[i]->base;
            pair->second = mmap[i]->length;
            largest_size = mmap[i]->length;
        }
    }
}

void init_memory(){
    mem_number_vpages = 0;
    uint64_pair_t pair;
    get_usable_memory_region(&pair);
    
    uint64_t pml4_phys = mem_get_current_pml4_phys();
    kernel_pml4 = (uint64_t*)(pml4_phys + HHDM);
    
    pmm_init(pair.first,pair.first + pair.second);
    memset(pml4_bitmap,0,sizeof(pml4_bitmap));
}

void parse_and_log_limine_memory_mapping(){
    uint64_t n_entries = limine_data.mmap_data.n_entries;
    for (uint64_t i = 0; i < n_entries;i++){
        uint64_t region_start = limine_data.mmap_data.memmap_entries[i]->base;
        uint64_t region_size = limine_data.mmap_data.memmap_entries[i]->length;
        uint8_t region_type = limine_data.mmap_data.memmap_entries[i]->type;
        logf("Region %d: start: %x, len: %x, type: %d",i,region_start,region_size,region_type);
    }
}

