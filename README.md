**Brick OS** is a work-in-progress 32-bit operating system created mainly for learning purposes. 

# Branches
The normal branch where I will be writing everything in C will be `main`

The branch `unstable` is mostly for testing and trying to write this in `Brick-Lang` 

# Building

## Dependancies
This project depends on you having installed the following binaries yourself:

- `grub-mkrescue`
- `qemu-system-i386`
- `qemu-img` 
- `make`
- `nasm` 
- `clang`

The rest of the needed binaries will be created once you build the toolchain using `build_toolchain.sh` in `tools/` (this is currently in a transition phase and not stable)

## Compilation
Once you have successfully created the cross-compiler, you can simply compile this project using `make iso`

You will also need to create a virtual disk for the filesystem. This disk can be created using the `make_disk.sh` bash script

## Running
To start the virtual machine with the OS run `bash start_vm.sh` 

# Notice
This project is still highly in development and kinda unstable, so if something breaks try to create a new disk or wait for a more stable version.