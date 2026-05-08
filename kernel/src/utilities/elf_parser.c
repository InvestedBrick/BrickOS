#include "elf_parser.h"
#include "../tables/syscalls.h"
#include "../processes/user_process.h"
#include "../io/log.h"
#include "../memory/kmalloc.h"
bool parse_elf(unsigned char* filepath){
    Elf64_Ehdr ehdr;
    user_process_t* proc = get_current_user_process();
    uint64_t fd = sys_open(proc,filepath,FILE_FLAG_READ);
    if (fd == SYSCALL_FAIL) return false;

    uint64_t n_bytes = sys_read(proc,fd,&ehdr,sizeof(ehdr));
    if (n_bytes == SYSCALL_FAIL) {
        sys_close(proc,fd);
        return false;
    }
    if (memcmp(ehdr.e_ident,ELFMAG,4) != 0) {
        sys_close(proc,fd);
        return false;
    }
    log("ELF header parsed successfully");
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
        return false;
    }
    void* addr = (void*)sys_mmap(proc,MMAP_UNSPEC_ADDR,total_phdr_size,PROT_READ, MAP_ANON,fd,ehdr.e_phoff);
    memcpy(phdrs,addr,total_phdr_size);
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

    return true;

}