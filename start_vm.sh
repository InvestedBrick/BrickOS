qemu-system-i386 -d int -drive file=hdd.img,format=raw,if=ide -serial file:serial.log -boot d -cdrom BrickOS.iso -m 512 

# add "-d int" as argument for more debug info 