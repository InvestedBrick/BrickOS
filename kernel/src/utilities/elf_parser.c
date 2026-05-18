#include "elf_parser.h"
#include "../tables/syscalls.h"
#include "../processes/user_process.h"
#include "../io/log.h"
#include "../memory/kmalloc.h"
void parse_elf(unsigned char* filepath){
    Elf64_Ehdr ehdr;
    user_process_t* proc = get_current_user_process();
    uint64_t fd = sys_open(proc,filepath,FILE_FLAG_READ);
    if (fd == SYSCALL_FAIL) return;

    uint64_t n_bytes = sys_read(proc,fd,&ehdr,sizeof(ehdr));
    if (n_bytes == SYSCALL_FAIL) {
        sys_close(proc,fd);
        return;
    }
    if (memcmp(ehdr.e_ident,ELFMAG,4) != 0) {
        sys_close(proc,fd);
        return;
    }
    logf("ELF header of %s parsed successfully", filepath);
    logf("format: %d-bit",ehdr.e_ident[EI_CLASS] == ELFCLASS64 ? 64 : 32);
    logf("endianness: %s",ehdr.e_ident[EI_DATA] == ELFDATA2LSB ? "little endian" : "big endian");
    logf("object file type: %d",ehdr.e_type);
    logf("machine type: %x",ehdr.e_machine);
    logf("entry point: %x",ehdr.e_entry);
    logf("program header entries: %d",ehdr.e_phnum);
    logf("section header entries: %d",ehdr.e_shnum);
    logf("program header offset: %x",ehdr.e_phoff);
    logf("section header offset: %x",ehdr.e_shoff);
    logf("size of program header entry: %d",ehdr.e_phentsize);
    logf("size of section header entry: %d",ehdr.e_shentsize);

    uint32_t total_phdr_size = sizeof(Elf64_Phdr) * ehdr.e_phnum;
    logf("total program headers size: %d",total_phdr_size);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)kmalloc(total_phdr_size);
    if (!phdrs) {
        sys_close(proc,fd);
        return;
    }

    sys_seek(proc,fd,ehdr.e_phoff);
    n_bytes = sys_read(proc,fd,phdrs,total_phdr_size);

    //TODO: munmap
    for (uint32_t i = 0; i < ehdr.e_phnum;i++){
        logf("Program header %d:",i);
        logf("  type: %d",phdrs[i].p_type);
        logf("  offset: %x",phdrs[i].p_offset);
        logf("  virtual address: %x",phdrs[i].p_vaddr);
        logf("  physical address: %x",phdrs[i].p_paddr);
        logf("  file size: %x",phdrs[i].p_filesz);
        logf("  memory size: %x",phdrs[i].p_memsz);
        logf("  flags: %d",phdrs[i].p_flags);
        logf("  alignment: %x",phdrs[i].p_align);
    }
    kfree(phdrs);
    sys_close(proc,fd);

}

bool validate_elf(unsigned char* filepath,Elf64_Ehdr* out_ehdr){
    user_process_t* proc = get_current_user_process();
    uint64_t fd = sys_open(proc,filepath,FILE_FLAG_READ);
    if (fd == SYSCALL_FAIL) return false;

    uint64_t n_bytes = sys_read(proc,fd,out_ehdr,sizeof(Elf64_Ehdr));
    if (n_bytes == SYSCALL_FAIL) {
        sys_close(proc,fd);
        return false;
    }
    if (memcmp(out_ehdr->e_ident,ELFMAG,4) != 0) {
        sys_close(proc,fd);
        return false;
    }
    sys_close(proc,fd);
    
    if(out_ehdr->e_ident[EI_CLASS] != ELFCLASS64) return false; // 64 bit
    if(out_ehdr->e_ident[EI_DATA] != ELFDATA2LSB) return false; // little endian
    if(out_ehdr->e_type != ET_EXEC) return false; // executable
    if(out_ehdr->e_machine != EM_X86_64) return false; // for x86-64 architecture
    if(out_ehdr->e_phoff == 0) return false; // must have program headers
    if(out_ehdr->e_phnum == 0) return false; // must have at least one program header
    
    return true;
}

Elf64_Phdr* extract_elf_phdrs(unsigned char* filepath){
    user_process_t* proc = get_current_user_process();
    uint64_t fd = sys_open(proc,filepath,FILE_FLAG_READ);
    if (fd == SYSCALL_FAIL) return nullptr;
    
    Elf64_Ehdr ehdr;
    uint64_t n_bytes = sys_read(proc,fd,(unsigned char*)&ehdr,sizeof(Elf64_Ehdr));
    if (n_bytes == SYSCALL_FAIL) {
        sys_close(proc,fd);
        return nullptr;
    }

    if (ehdr.e_phnum == 0 || ehdr.e_phoff == 0){
        sys_close(proc,fd);
        return nullptr;
    }
    uint32_t total_phdrs_size = ehdr.e_phentsize * ehdr.e_phnum;
    Elf64_Phdr* phdrs = (Elf64_Phdr*)kmalloc(total_phdrs_size);
    if (!phdrs){
        sys_close(proc,fd);
        return nullptr;
    }
    sys_seek(proc,fd,ehdr.e_phoff);
    n_bytes = sys_read(proc,fd,phdrs,total_phdrs_size);
    sys_close(proc,fd);
    if (n_bytes < total_phdrs_size) {
        kfree(phdrs);
        return nullptr;
    }    
    return phdrs;
}