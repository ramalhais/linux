#!/bin/bash -x

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-10

make -j$[$(nproc)*2] || exit 1

# DATE=$(date +%F-%H.%M.%S)
# KERNELVER=$(make kernelversion)

### Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary

### Compile NeXT tools
make -C arch/m68k/tools/next/

./arch/m68k/tools/next/aout vmlinux.binary vmlinux-NeXT
