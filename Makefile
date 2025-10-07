
ASM_SOURCES = $(shell find src/ -type f -name '*.asm')
C_SOURCES   = $(shell find src/ -type f -name '*.c')

LD_SCRIPT = link.ld

ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS   = $(C_SOURCES:.c=.o)
TARGET   = kernel.elf
ISO      = BrickOS.iso

CC = clang
LLD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -ffreestanding -m64 -nostdlib \
         -fno-builtin -fno-stack-protector -fno-exceptions -g \
         -Wno-pointer-sign -fno-pic -fno-pie
          # idgaf about casting const char* to char*
LDFLAGS =  --no-pie elf_x86_64

all: $(TARGET)

%.o: %.asm
	$(AS) -g -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(ASM_OBJS) $(C_OBJS)
	$(LLD) $(LDFLAGS) -T $(LD_SCRIPT) $(ASM_OBJS) $(C_OBJS) -o $(TARGET)

iso: $(TARGET)
	cp kernel.elf iso/boot
	make -C iso/modules
	bash stage3.sh

clean:
	rm -f $(ASM_OBJS) $(C_OBJS) $(TARGET) $(ISO)
	make -C iso/modules clean
