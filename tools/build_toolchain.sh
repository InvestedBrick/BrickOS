# usage: "bash build_toolchain.sh <num-cores>""
# Example: "bash build_toolchain.sh 8"

sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo -y

export TARGET=i386-elf
export PREFIX="$PWD/cross"
export PATH="$PREFIX/bin:$PATH"

# Download sources only if they don't exist
[ -f binutils-2.45.tar.xz ] || wget https://ftp.gnu.org/gnu/binutils/binutils-2.45.tar.xz
[ -f gcc-15.2.0.tar.xz ]   || wget https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.xz
[ -f gdb-16.3.tar.xz ]     || wget https://ftp.gnu.org/gnu/gdb/gdb-16.3.tar.xz

# Extract archives only if the folders don't exist
[ -d binutils-2.45 ] || tar -xf binutils-2.45.tar.xz
[ -d gcc-15.2.0 ]    || tar -xf gcc-15.2.0.tar.xz
[ -d gdb-16.3 ]      || tar -xf gdb-16.3.tar.xz

# Build Binutils
mkdir build-binutils 
cd build-binutils
../binutils-2.45/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --disable-werror
make -j$1
make install
cd ..

# Build GCC 
mkdir build-gcc 
cd build-gcc
../gcc-15.2.0/contrib/download_prerequisites
../gcc-15.2.0/configure \
  --target="$TARGET" --prefix="$PREFIX" \
  --disable-nls --enable-languages=c \
  --without-headers --disable-shared --disable-libssp
make all-gcc -j$1
make all-target-libgcc -j$1
make install-gcc
make install-target-libgcc
cd ..

# Build GDB
mkdir build-gdb 
cd build-gdb
../gdb-16.3/configure \
  --target="$TARGET" \
  --program-prefix=i386-elf- \
  --disable-werror \
  --prefix="$PREFIX"
make -j$1
make install
cd ..
