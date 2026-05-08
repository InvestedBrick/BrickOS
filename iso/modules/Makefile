ASM_SRCS := $(wildcard *.asm)
C_SRCS := $(wildcard *.c)

C_MODULES := $(basename $(C_SRCS))
C_OBJS := $(addsuffix .o, $(C_MODULES))
C_ELFS := $(addsuffix .elf, $(C_MODULES))
#C_BINS := $(addsuffix .bin, $(C_MODULES))

ASM_ELFS := $(ASM_SRCS:.asm=.elf)

NASM := nasm
NASMELFFLAGS := -f elf64

CC := clang
LLD := ld.lld
ELF_STRIP := llvm-strip

CFLAGS = -target x86_64-elf -ffreestanding -m64 -nostdlib \
         -fno-builtin -fno-stack-protector -fno-exceptions -g \
         -Wno-pointer-sign -fno-pic -fno-pie -O1
LDFLAGS := --no-pie -T link_c.ld

CSTDLIB_DIR := cstdlib
CSTDLIB_C_SRCS := $(wildcard $(CSTDLIB_DIR)/*.c)
CSTDLIB_ASM_SRCS := $(wildcard $(CSTDLIB_DIR)/*.asm)
CSTDLIB_OBJS := $(patsubst $(CSTDLIB_DIR)/%.c, $(CSTDLIB_DIR)/%.o, $(CSTDLIB_C_SRCS)) \
			    $(patsubst $(CSTDLIB_DIR)/%.asm, $(CSTDLIB_DIR)/%.o, $(CSTDLIB_ASM_SRCS))

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

# Link .o to .elf with code starting at 0x0, link with cstdlib objects
%.elf: %.o $(CSTDLIB_OBJS)
	$(LLD) $(LDFLAGS)  -o $@ $^
	$(ELF_STRIP) $@ 

# Convert .elf to flat binary
#%.bin: %.elf
#	$(OBJCOPY) -O binary $< $@

# Pure ASM to flat binary
%.elf: %.asm
	$(NASM) $(NASMELFFLAGS) -o $@ $<
	$(ELF_STRIP) $@

clean:
	rm -f *.o *.elf *.bin $(CSTDLIB_DIR)/*.o
