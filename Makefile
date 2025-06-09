ASM_SOURCES = $(wildcard src/*.asm) $(wildcard src/io/*.asm) $(wildcard src/tables/*.asm) $(wildcard src/memory/*.asm)
C_SOURCES = $(wildcard src/*.c) $(wildcard src/io/*.c) $(wildcard src/tables/*.c) $(wildcard src/drivers/keyboard/*.c) $(wildcard src/memory/*.c)

LD_SCRIPT = link.ld
LD_ARGS = -m elf_i386

ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS = $(C_SOURCES:.c=.o)
TARGET = kernel.elf
ISO = BrickOS.iso

CC = gcc
CFLAGS = -ffreestanding -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-exceptions

all: $(TARGET)

%.o: %.asm
	nasm -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


$(TARGET): $(ASM_OBJS) $(C_OBJS)
	ld $(LD_ARGS) -T $(LD_SCRIPT) $(ASM_OBJS) $(C_OBJS) -o $(TARGET)


iso: $(TARGET)
	cp kernel.elf iso/boot
	bash stage3.sh

clean:
	rm -f $(ASM_OBJS) $(TARGET) $(ISO) $(C_OBJS)
