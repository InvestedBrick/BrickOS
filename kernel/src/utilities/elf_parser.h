#ifndef INCLUDE_ELF_PARSER_H
#define INCLUDE_ELF_PARSER_H
#include <stdint.h>
#include <elf.h>
#include <stdbool.h>

#define PHDR_FLAG_EXEC 0x1
#define PHDR_FLAG_WRITE 0x2
#define PHDR_FLAG_READ 0x4
//debug & inspection function
void parse_elf(unsigned char* filepath);

/**
 * validate_elf:
 * Validates if an ELF file has the correct file type / formats etc, returns the ELF header in out_ehdr
 * @param filepath The filepath of the ELF file
 * @param out_ehdr The ELF header to extract the information to
 * @return 1 if valid otherwise 0
 */
bool validate_elf(unsigned char* filepath,Elf64_Ehdr* out_ehdr);

/**
 * extract_elf_phdrs:
 * Extracts the program headers from an ELF file and returns them in a kmalloc'd array. Assumes valid and extractable ELF file
 * @param  filepath The ELF file
 * @return The elf program headers in a kmalloced array (needs to be freed by caller upon process shutdown) on success, nullptr otherwise
 */
Elf64_Phdr* extract_elf_phdrs(unsigned char* filepath);

#endif