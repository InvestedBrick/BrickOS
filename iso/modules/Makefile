ASM_SRCS := $(wildcard *.asm)
C_SRCS := $(wildcard *.c)

C_MODULES := $(basename $(C_SRCS))
C_OBJS := $(addsuffix .o, $(C_MODULES))
C_ELFS := $(addsuffix .elf, $(C_MODULES))
C_BINS := $(addsuffix .bin, $(C_MODULES))

ASM_BINS := $(ASM_SRCS:.asm=.bin)

NASM := nasm
NASMFLAGS := -f bin
NASMOBJFLAGS := -f elf32

CC := ../../tools/cross/bin/i386-elf-gcc
OBJCOPY := ../../tools/cross/bin/i386-elf-objcopy

CFLAGS := -m32 -O1 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -nostartfiles
LDFLAGS := -T link_c.ld

CSTDLIB_DIR := cstdlib
CSTDLIB_SRCS := $(wildcard $(CSTDLIB_DIR)/*.c)
CSTDLIB_OBJS := $(patsubst $(CSTDLIB_DIR)/%.c, $(CSTDLIB_DIR)/%.o, $(CSTDLIB_SRCS))

.PHONY: all clean

all: $(ASM_BINS) $(C_BINS)

# Compile cstdlib .c to .o
$(CSTDLIB_DIR)/%.o: $(CSTDLIB_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Link .o to .elf with code starting at 0x0, link with cstdlib objects
%.elf: %.o $(CSTDLIB_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ 

# Convert .elf to flat binary
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

# Pure ASM to flat binary
%.bin: %.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

clean:
	rm -f *.o *.elf *.bin $(CSTDLIB_DIR)/*.o
