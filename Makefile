ASM_SOURCES = \
    src/*.asm \
    src/io/*.asm \
    src/tables/*.asm \
    src/memory/*.asm \
    src/processes/*.asm

C_SOURCES = \
    src/*.c \
    src/io/*.c \
    src/tables/*.c \
    src/drivers/keyboard/*.c \
    src/drivers/timer/*.c \
    src/memory/*.c \
    src/processes/*.c \
    src/modules/*.c \
	src/drivers/ATA_PIO/*.c \
    src/filesystem/*c \
    src/filesystem/vfs/*.c \
    src/screen/*.c 


ASM_SOURCES := $(wildcard $(ASM_SOURCES))
C_SOURCES := $(wildcard $(C_SOURCES))

LD_SCRIPT = link.ld
LD_ARGS = -m elf_i386 -g 

ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS = $(C_SOURCES:.c=.o)
TARGET = kernel.elf
ISO = BrickOS.iso

CC = gcc
CFLAGS = -ffreestanding -m32 -nostdlib -fno-builtin -fno-stack-protector -fno-exceptions -g

all: $(TARGET)

%.o: %.asm
	nasm -g -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


$(TARGET): $(ASM_OBJS) $(C_OBJS)
	ld $(LD_ARGS) -T $(LD_SCRIPT) $(ASM_OBJS) $(C_OBJS) -o $(TARGET)


iso: $(TARGET)
	cp kernel.elf iso/boot
	make -C iso/modules
	bash stage3.sh

clean:
	rm -f $(ASM_OBJS) $(TARGET) $(ISO) $(C_OBJS)
	make -C iso/modules clean
