qemu-system-i386 -s \
    -drive file=hdd.img,format=raw,if=ide \
    -serial file:serial.log \
    -boot d \
    -cdrom BrickOS.iso \
    -m 512 \
    -vga std 

# add "-d int" as argument for more debug info 
# -s for gdb remote target
# -S to stop execution at start