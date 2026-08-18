ASM_SRCS := $(wildcard *.asm)
C_SRCS := $(wildcard *.c)

C_MODULES := $(basename $(C_SRCS))
C_OBJS := $(addsuffix .o, $(C_MODULES))
C_ELFS := $(addsuffix .elf, $(C_MODULES))

ASM_ELFS := $(ASM_SRCS:.asm=.elf)

NASM := nasm
NASMELFFLAGS := -f elf64

CC := clang
LLD := ld.lld
ELF_STRIP := llvm-strip

CFLAGS = -target x86_64-elf -ffreestanding -m64 -nostdlib \
         -fno-builtin -fno-stack-protector -fno-exceptions -g \
         -Wno-pointer-sign -fno-pic -fno-pie
LDFLAGS := --no-pie -T link_c.ld

CSTDLIB_DIR := cstdlib
CSTDLIB_C_SRCS := $(wildcard $(CSTDLIB_DIR)/*.c)
CSTDLIB_ASM_SRCS := $(wildcard $(CSTDLIB_DIR)/*.asm)
CSTDLIB_OBJS := $(patsubst $(CSTDLIB_DIR)/%.c, $(CSTDLIB_DIR)/%.o, $(CSTDLIB_C_SRCS)) \
			$(patsubst $(CSTDLIB_DIR)/%.asm, $(CSTDLIB_DIR)/%.o, $(CSTDLIB_ASM_SRCS))

QOI_DIR := ../qoi
QOI_C_SRCS := $(wildcard $(QOI_DIR)/*.c)
QOI_OBJS := $(patsubst $(QOI_DIR)/%.c, $(QOI_DIR)/%.o, $(QOI_C_SRCS))

.PHONY: all clean

all: $(ASM_ELFS) $(C_ELFS)

# Compile cstdlib .c to .o
$(CSTDLIB_DIR)/%.o: $(CSTDLIB_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile cstdlib .asm to .o
$(CSTDLIB_DIR)/%.o: $(CSTDLIB_DIR)/%.asm
	$(NASM) $(NASMELFFLAGS) -o $@ $<

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(QOI_DIR)/%.o: $(QOI_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.elf: %.o $(CSTDLIB_OBJS) $(QOI_OBJS)
	$(LLD) $(LDFLAGS)  -o $@ $^
	$(ELF_STRIP) $@ 

# Pure ASM to flat binary
%.elf: %.asm
	$(NASM) $(NASMELFFLAGS) -o $@ $<
	$(ELF_STRIP) $@

clean:
	rm -f *.o *.elf *.bin $(CSTDLIB_DIR)/*.o $(QOI_DIR)/*.o
