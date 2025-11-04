
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
         -Wno-pointer-sign -fno-pic -fno-pie -mcmodel=large -O1
          # idgaf about casting const char* to char*
LDFLAGS =  --no-pie

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
	grub-mkrescue -o BrickOS.iso iso --modules="vbe vga video gfxterm"


run: iso
	qemu-img create -f raw hdd.img 256M
	qemu-system-x86_64 -s \
    -drive file=hdd.img,format=raw,if=ide \
    -serial file:serial.log \
    -boot d \
    -cdrom BrickOS.iso \
    -m 512 \
    -vga std 
clean:
	rm -f $(ASM_OBJS) $(C_OBJS) $(TARGET) $(ISO)
	rm hdd.img
	make -C iso/modules clean
