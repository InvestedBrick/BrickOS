#include "memory.h"
#include "../util.h"
#include "../io/log.h"
#include "../vector.h"

static uint32_t total_alloced_pages;
static uint64_t total_pages;
static uint32_t mem_number_vpages;

static uint8_t *physical_memory_bitmap;
vector_t shm_obj_vec;
static uint64_t* kernel_pml4;
static uint64_t pml4_tables[N_PML4_TABLES][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint8_t pml4_table_used[N_PML4_TABLES];

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
    return phys + KERNEL_START; // does not work for phys user pages
}
uint64_t linear_virt_to_phys(uint64_t virt){
    return virt - KERNEL_START; 
}

uint64_t virt_to_phys(uint64_t virt_addr){
    uint32_t pml4_idx = PML4E(virt_addr);
    uint32_t pdpt_idx = PDPTE(virt_addr);
    uint32_t pd_idx   = PDE(virt_addr);
    uint32_t pt_idx   = PTE(virt_addr);

    uint64_t* pml4 = (uint64_t*)REC_PML4_VIRT;
    if (!(pml4[pml4_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pdpt = (uint64_t*)(REC_PDPT_VIRT(pml4_idx));
    if (!(pdpt[pdpt_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pd   = (uint64_t*)(REC_PD_VIRT(pml4_idx,pdpt_idx));
    if (!(pd[pd_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    uint64_t* pt   = (uint64_t*)(REC_PT_VIRT(pml4_idx,pdpt_idx,pd_idx));
    if (!(pt[pt_idx] & PAGE_FLAG_PRESENT)) return INVALID_PHYS_ADDR;

    return (CANONICALIZE(PTE_GET_ADDR(pt[pt_idx]))) | (virt_addr & 0xfff);
}

void pmm_init(uint64_t phys_alloc_start, uint64_t mem_high){
    total_pages = mem_high / MEMORY_PAGE_SIZE;
    uint64_t bitmap_size = CEIL_DIV(total_pages, 8);
    total_alloced_pages = 0;

    physical_memory_bitmap = (uint8_t*)linear_phys_to_virt(phys_alloc_start);
    memset(physical_memory_bitmap,0,bitmap_size);

    // reserve the kernel pages
    uint64_t kernel_pages = ALIGN_UP(phys_alloc_start + bitmap_size,MEMORY_PAGE_SIZE) / MEMORY_PAGE_SIZE;
    for (uint64_t i = 0; i < kernel_pages;i++){
        physical_memory_bitmap[i / 8] |= (1 << (i % 8));
    }

}

uint64_t* mem_get_current_pml4_table() {
    uint64_t phys = mem_get_current_pml4_phys();
    return (uint64_t*)linear_phys_to_virt(phys);
}

void mem_set_current_pml4_table(uint64_t* virt) {
    uint64_t phys = linear_virt_to_phys((uint64_t)virt);
    mem_set_current_pml4_phys((uint64_t*)phys);
}

void pmm_free_page_frame(uint64_t phys_addr){
    uint64_t frame = phys_addr / MEMORY_PAGE_SIZE;
    uint64_t byte = frame / 8;
    uint64_t bit = frame % 8;
    physical_memory_bitmap[byte] &= ~(1 << bit);
}

void free_table_entry(uint64_t* tbl,uint32_t tbl_idx){
    uint64_t phys_page = tbl[tbl_idx] & ~0xFFF;
    pmm_free_page_frame(phys_page);
    tbl[tbl_idx] = 0;
}

void free_user_pml4_table(uint64_t* user_pml4) {
    for (uint32_t pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
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

    uint64_t pml4_phys = linear_virt_to_phys((uint64_t)user_pml4);
    pmm_free_page_frame(pml4_phys);
}

uint64_t* create_user_pml4_table(){
    for (uint32_t i = 0; i < N_PML4_TABLES;i++){
        if(!pml4_table_used[i]){
            pml4_table_used[i] = 1;
            uint64_t* pml4 = pml4_tables[i];
            memset(pml4,0,sizeof(uint64_t) * ENTRIES_PER_TABLE);
            // Copy Kernel mapping
            for (uint32_t j = 256; j < ENTRIES_PER_TABLE;j++){
                pml4[j] = kernel_pml4[j];
            }

            uint64_t pml4_phys = linear_virt_to_phys((uint64_t)pml4);
            pml4[RECURSIVE_SLOT] = pml4_phys | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
            
            return pml4;
        }
    }
    return (uint64_t*)-1;
}

void restore_kernel_pml4_table(){
    uint64_t* current_pml4 = mem_get_current_pml4_table();
    if (current_pml4 != kernel_pml4){
        mem_set_current_pml4_table(kernel_pml4);
    }
}

void create_table_if_not_present(uint64_t* tbl,uint32_t tbl_idx,uint32_t flags,uint64_t addr){
    if ((tbl[tbl_idx] & PAGE_FLAG_PRESENT)) return;
        
    uint64_t phys_addr = pmm_alloc_page_frame();
    tbl[tbl_idx] = phys_addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE  | (flags & PAGE_FLAG_USER);
    invalidate(addr);

    memset((void*)addr,0x0,MEMORY_PAGE_SIZE);
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

    uint64_t pdpt_addr = REC_PDPT_VIRT(pml4_idx);
    uint64_t pd_addr   = REC_PD_VIRT(pml4_idx,pdpt_idx);
    uint64_t pt_addr   = REC_PT_VIRT(pml4_idx,pdpt_idx,pd_idx);

    create_table_if_not_present(pml4,pml4_idx,flags,pdpt_addr);
    uint64_t* pdpt = (uint64_t*)(pdpt_addr);
    
    create_table_if_not_present(pdpt,pdpt_idx,flags,pd_addr);
    uint64_t* pd   = (uint64_t*)(pd_addr);

    create_table_if_not_present(pd,pd_idx,flags,pt_addr);
    uint64_t* pt   = (uint64_t*)(pt_addr);

    pt[pt_idx] = (phys_addr & ~0xfff) | PAGE_FLAG_PRESENT | flags;
    mem_number_vpages++;
    invalidate(virt_addr);

    if (switched_pml4) {
        mem_set_current_pml4_table(old_pml4);
    }

}

void mem_map_page(uint64_t virt_addr, uint64_t phys_addr, uint32_t flags){
    uint64_t* prev_pml4_table = 0;

    if (virt_addr >= KERNEL_START){ // if we are in kernel memory
        prev_pml4_table = mem_get_current_pml4_table(); // switch to the kernel memory page
        if (prev_pml4_table != kernel_pml4){
            mem_set_current_pml4_table(kernel_pml4);
        }
    }

    mem_map_page_in_pml4((uint64_t*)mem_get_current_pml4_table(), virt_addr, phys_addr, flags);

    if (prev_pml4_table != 0 && prev_pml4_table != kernel_pml4){
        mem_set_current_pml4_table(prev_pml4_table);
    }
}

void mem_unmap_page(uint64_t virt_addr){

    uint64_t* prev_pml4_table = 0;

    if (virt_addr >= KERNEL_MALLOC_START){ // if we are in kernel memory
        prev_pml4_table = mem_get_current_pml4_table(); // switch to the kernel memory page
        if (prev_pml4_table != kernel_pml4){
            mem_set_current_pml4_table(kernel_pml4);
        }
    }
    
    uint32_t pml4_idx = PML4E(virt_addr);
    uint32_t pdpt_idx = PDPTE(virt_addr);
    uint32_t pd_idx   = PDE(virt_addr);
    uint32_t pt_idx   = PTE(virt_addr);
    
    uint64_t* pml4 = (uint64_t*)REC_PML4_VIRT;
    
    if (!(pml4[pml4_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pdpt = (uint64_t*)(REC_PDPT_VIRT(pml4_idx));
    if (!(pdpt[pdpt_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pd = (uint64_t*)(REC_PD_VIRT(pml4_idx,pdpt_idx));
    if (!(pd[pd_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    uint64_t* pt = (uint64_t*)(REC_PT_VIRT(pml4_idx,pdpt_idx,pd_idx));
    if (!(pt[pt_idx] & PAGE_FLAG_PRESENT)) goto invalid_unmap;

    pt[pt_idx] = 0x0;
    mem_number_vpages--;
    invalidate(virt_addr);

    uint8_t pt_empty = 1;
    for (int i = 0; i < (ENTRIES_PER_TABLE); i++) {
        if (pt[i] & PAGE_FLAG_PRESENT) { pt_empty = 0; break; }
    }
    if (pt_empty) {
        uint64_t phys = virt_to_phys((uint64_t)pt);
        pd[pd_idx] = 0;
        pmm_free_page_frame(phys);
    }
    //TODO: finish freeing for all layers
invalid_unmap:
    if (prev_pml4_table != 0 && prev_pml4_table != kernel_pml4){
        mem_set_current_pml4_table(kernel_pml4);
    }
}

uint64_t pmm_alloc_page_frame() {
    uint32_t n_bytes = CEIL_DIV(total_pages,8);
    for (uint32_t b = 0; b < n_bytes;b++){
        
        uint8_t byte = physical_memory_bitmap[b];
        if (byte == 0xff){ // all 8 frames are in use
            continue;
        }

        for(uint32_t i = 0; i < 8;i++){
            uint8_t used = (byte >> i) & 0x1;

            if(!used){
                byte |= (1 << i); //set 
                physical_memory_bitmap[b] = byte;
                total_alloced_pages++;

                uint64_t phys_addr = (b * 8 + i) * MEMORY_PAGE_SIZE;
                
                return phys_addr;
            }
        }
    }

    panic("System ran out of physical memory");
    return 0;
}

void un_identity_map_first_page_table(){
    kernel_pml4[0] = 0;
    invalidate(0);
}

void init_memory(uint64_t physical_alloc_start, uint64_t mem_high){
    mem_number_vpages = 0;
    
    int64_t pml4_phys = (uint64_t)&initial_pml4_table; // initial pml4 table is identity-mapped physical address
    kernel_pml4 = (uint64_t*)(pml4_phys + KERNEL_START);
    
    initial_pml4_table[RECURSIVE_SLOT] = (uint64_t)pml4_phys | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
    invalidate(REC_PML4_VIRT);
    
    pmm_init(physical_alloc_start,mem_high);
    memset(pml4_tables,0,sizeof(pml4_tables));
    memset(pml4_table_used,0,sizeof(pml4_table_used));
}


