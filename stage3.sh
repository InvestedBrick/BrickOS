genisoimage -R                                  \
            -b boot/grub/stage2_eltorito        \
            -no-emul-boot                       \
            -boot-load-size 4                   \
            -A BrickOS                          \
            -input-charset utf8                 \
            -quiet                              \
            -boot-info-table                    \
            -o BrickOS.iso                      \
            iso