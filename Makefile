ENTRY = src/entry.asm
LD_SCRIPT = link.ld
LD_ARGS = -m elf_i386
ASM_O = entry.o
CPP_O = kernel.o
TARGET = kernel.elf
ISO = BrickOS.iso

CC = gcc
CFLAGS = -ffreestanding -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-exceptions

all: $(TARGET)

$(ASM_O): $(ENTRY)
	nasm -f elf32 $(ENTRY) -o $(ASM_O)

$(CPP_O): src/kernel.c
	$(CC) $(CFLAGS) -c src/kernel.c -o $(CPP_O)

$(TARGET): $(ASM_O) $(CPP_O)
	ld $(LD_ARGS) -T $(LD_SCRIPT) $(ASM_O) $(CPP_O) -o $(TARGET)


iso: $(TARGET)
	cp kernel.elf iso/boot
	bash stage3.sh

clean:
	rm -f $(ASM_O) $(TARGET) $(ISO)
