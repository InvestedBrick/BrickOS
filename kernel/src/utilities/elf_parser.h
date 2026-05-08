#ifndef INCLUDE_ELF_PARSER_H
#define INCLUDE_ELF_PARSER_H
#include <stdint.h>
#include <elf.h>
#include <stdbool.h>

bool parse_elf(unsigned char* filepath);

#endif