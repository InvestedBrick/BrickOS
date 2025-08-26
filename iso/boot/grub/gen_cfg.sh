#!/bin/bash
cat <<EOF
menuentry "BrickOS" {
    insmod gfxterm
    insmod vbe
    set gfxmode=1024x768x32
    set gfxpayload=keep

    terminal_output gfxterm

    search --file /boot/kernel.elf --set=root
    multiboot /boot/kernel.elf
EOF

for f in modules/*.bin; do
    echo "    module /$f /$f"
done

echo "}"
