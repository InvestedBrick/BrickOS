ASM_SRCS := $(wildcard *.asm)
C_SRCS   := $(wildcard *.c)

C_MODULES := $(basename $(C_SRCS))
C_OBJS    := $(addsuffix .o, $(C_MODULES))
C_ELFS    := $(addsuffix .elf, $(C_MODULES))

ASM_ELFS := $(ASM_SRCS:.asm=.elf)

NASM        := nasm
NASMELFFLAGS := -f elf64

CC        := clang
LLD       := ld.lld
ELF_STRIP := llvm-strip

# Directories containing code any module may pull in: stdlib, shared, qoi.
CSTDLIB_DIR := cstdlib
SHARED_DIR  := ../../shared/src
QOI_DIR     := ../qoi
SHARED_INCLUDE := ../../shared/include

COMMON_DIRS := $(CSTDLIB_DIR) $(SHARED_DIR) $(QOI_DIR)

COMMON_C_SRCS   := $(foreach d,$(COMMON_DIRS),$(wildcard $(d)/*.c))
COMMON_ASM_SRCS := $(foreach d,$(COMMON_DIRS),$(wildcard $(d)/*.asm))
COMMON_OBJS     := $(COMMON_C_SRCS:.c=.o) $(COMMON_ASM_SRCS:.asm=.o)

CFLAGS = -target x86_64-elf -ffreestanding -m64 -nostdlib \
         -fno-builtin -fno-stack-protector -fno-exceptions -g \
         -Wno-pointer-sign -fno-pic -fno-pie \
         -I$(SHARED_INCLUDE)

LDFLAGS := --no-pie -T link_c.ld

.PHONY: all clean

all: $(ASM_ELFS) $(C_ELFS)

# Generic compile rules — apply to *.c/*.asm in any directory
# (cstdlib, shared, qoi, or the module dir itself), since '%'
# matches the whole path including any directory prefix.
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.o: %.asm
	$(NASM) $(NASMELFFLAGS) -o $@ $^

# Link a module's .o against all common objects (stdlib + shared + qoi)
%.elf: %.o $(COMMON_OBJS)
	$(LLD) $(LDFLAGS) -o $@ $^
	$(ELF_STRIP) $@

# Pure ASM module straight to elf
%.elf: %.asm
	$(NASM) $(NASMELFFLAGS) -o $@ $^
	$(ELF_STRIP) $@

clean:
	rm -f *.o *.elf *.bin $(COMMON_OBJS)