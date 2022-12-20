#!/bin/bash -x

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-10

make next_fb_net_nfs_iscsi_fs_scsi_part_defconfig && make -j$[$(nproc)*2] || exit 1

# DATE=$(date +%F-%H.%M.%S)
# KERNELVER=$(make kernelversion)

### Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary

### Compile NeXT tools
make -C arch/m68k/tools/next/

./arch/m68k/tools/next/aout vmlinux.binary vmlinux-NeXT.aout
./arch/m68k/tools/next/simpkern vmlinux.binary vmlinux-NeXT.macho-simpkern
./arch/m68k/tools/next/macho vmlinux.binary vmlinux-NeXT.macho
