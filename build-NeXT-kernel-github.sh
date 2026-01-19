#!/bin/bash -x
set -e

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-10

NPROCS=$[$(nproc)*2]

# Compile NeXT tools for m68k
make CC=${CROSS_COMPILE}gcc${GCC_SUFFIX} -j$NPROCS -C arch/m68k/tools/next/
for BIN in aout macho simpkern next-disklabel; do
	mv arch/m68k/tools/next/$BIN arch/m68k/tools/next/$BIN.m68k
done

# Compile NeXT tools for amd64
make -j$NPROCS -C arch/m68k/tools/next/

# Get netbsd bootloader
curl -v -L -o arch/m68k/tools/next/netbsd-boot-next.aout https://github.com/ramalhais/netbsd-boot-NeXT/releases/latest/download/netbsd-boot-next.aout

# Compile Kernel
make next_defconfig
make -j$NPROCS

# DATE=$(date +%F-%H.%M.%S)
# KERNELVER=$(make kernelversion)

### Strip symbols
m68k-linux-gnu-strip --strip-unneeded vmlinux -o vmlinux.stripped

### Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary

ADD_OFFSET=1
if [ -z $KERN_LOADADDR ]; then
        MEM_BASE=4000000
        KERN_LOADADDR=$(m68k-linux-gnu-objdump -D vmlinux|grep '<_stext>:'|cut -f1 -d' ')

        IS_OFFSET=$(echo "ibase=16; ${KERN_LOADADDR} < ${MEM_BASE}" | bc)
        if [ $IS_OFFSET -eq 1 ] && [ $ADD_OFFSET -eq 1 ]; then
                KERN_LOADADDR=$(echo "obase=16; ibase=16; ${MEM_BASE}+${KERN_LOADADDR}" | bc)
        fi
fi

./arch/m68k/tools/next/aout vmlinux.binary vmlinux-NeXT.aout 0x${KERN_LOADADDR}
./arch/m68k/tools/next/simpkern vmlinux.binary vmlinux-NeXT.macho-simpkern
./arch/m68k/tools/next/macho vmlinux.binary vmlinux-NeXT.macho 0x${KERN_LOADADDR}
