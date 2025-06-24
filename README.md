# Dependencies
You need: `ld`, `gcc`, `genisoimage`, `nasm`, `qemu-system-i386` and `make`

# Branches
The normal branch where I will be writing everything in C will be `main`

The branch `unstable` is mostly for testing and trying to write this in `Brick-Lang` 

# Compiling && Running
To compile this project simply run `make iso` in this directory.

To start the virtual machine with the OS run `bash start_vm.sh` 

To create a 256 MB disk for the filesystem, run `bash make_disk.sh` (requires `qemu-img`)

# Notice
The filesystem and the shell are both not very complex and probably not 100% bug free, if something breaks, try removing the disk and making a new one