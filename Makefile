
ASM_SOURCES = $(shell find kernel/src/ -type f -name '*.asm')
C_SOURCES   = $(shell find kernel/src/ -type f -name '*.c')

LD_SCRIPT = link.ld

ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS   = $(C_SOURCES:.c=.o)
TARGET   = kernel.elf
ISO      = BrickOS.iso

CC = clang
LLD = ld.lld
AS = nasm

HOST_CC := $(CC)
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

CFLAGS = -target x86_64-elf -ffreestanding -m64 -nostdlib \
         -fno-builtin -fno-stack-protector -fno-exceptions -g \
         -Wno-pointer-sign -fno-pic -fno-pie -mcmodel=kernel -O1
          # idgaf about casting const char* to char*
LDFLAGS = \
    -nostdlib \
	-static \
    -z max-page-size=0x1000 \
    --gc-sections \
	-T $(LD_SCRIPT)

all: $(ISO)

%.o: %.asm
	$(AS) -g -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): limine/limine $(ASM_OBJS) $(C_OBJS) 
	$(LLD) $(LDFLAGS)  $(ASM_OBJS) $(C_OBJS) -o $(TARGET)

limine/limine:
	rm -rf limine
	git clone https://codeberg.org/Limine/Limine.git limine --branch=v10.x-binary --depth=1
	$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

kernel/.deps-obtained:
	./kernel/get-deps


$(ISO): $(TARGET) kernel/.deps-obtained
	bash limine_conf_gen.sh
	make -C iso/modules
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v $(TARGET) iso_root/boot/
	mkdir -p iso_root/modules
	cp -v  $(wildcard iso/modules/*.bin) iso_root/modules
	mkdir -p iso_root/boot/limine
	cp -v limine.conf iso_root/boot/limine
	mkdir -p iso_root/EFI/BOOT

	cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/

	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(ISO)
	./limine/limine bios-install $(ISO)

	rm -rf iso_root

run: $(ISO)
	qemu-img create -f raw hdd.img 256M
	qemu-system-x86_64 -s \
    -drive file=hdd.img,format=raw,if=ide \
    -serial file:serial.log \
    -boot d \
    -cdrom $(ISO) \
    -m 512M \
    -vga std \
	-d int
clean:
	rm -f $(ASM_OBJS) $(C_OBJS) $(TARGET) $(ISO)
	rm -rf limine
	make -C iso/modules clean
	rm hdd.img
