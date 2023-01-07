#!/bin/bash -x
set -e

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-10

NPROCS=$[$(nproc)*2]

# Compile NeXT tools for m68k
make CC=$CROSS_COMPILEgcc$GCC_SUFFIX -j$NPROCS -C arch/m68k/tools/next/
for BIN in aout macho next-disklabel; do
	cp arch/m68k/tools/next/$BIN arch/m68k/tools/next/$BIN.m68k
done

# Compile NeXT tools for amd64
make -j$NPROCS -C arch/m68k/tools/next/

# Get netbsd bootloader
curl -v -L -o arch/m68k/tools/next/netbsd-boot-next.aout https://github.com/ramalhais/netbsd-src/releases/latest/download/netbsd-boot-next.aout

# Compile Kernel
make next_fb_net_nfs_iscsi_fs_scsi_part_defconfig
make -j$NPROCS

# DATE=$(date +%F-%H.%M.%S)
# KERNELVER=$(make kernelversion)

### Strip symbols
m68k-linux-gnu-strip --strip-unneeded vmlinux -o vmlinux.stripped

### Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary

./arch/m68k/tools/next/aout vmlinux.binary vmlinux-NeXT.aout
./arch/m68k/tools/next/simpkern vmlinux.binary vmlinux-NeXT.macho-simpkern
./arch/m68k/tools/next/macho vmlinux.binary vmlinux-NeXT.macho

# Build disk image
dd if=/dev/zero of=linux-next-2gb-sparse.disk bs=2G count=1 conv=sparse
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$((160*1024)) $LOOPDEV linux-next-2gb-sparse.disk
sudo mkfs.ext2 -m0 -Lroot -r0 $LOOPDEV
sudo mkdir -p /mnt/bla
sudo mount $LOOPDEV /mnt/bla
sudo cp vmlinux.stripped /mnt/bla/
sudo umount /mnt/bla; sudo losetup -d $LOOPDEV
arch/m68k/tools/next/next-disklabel linux-next-2gb-sparse.disk -b arch/m68k/tools/next/netbsd-boot-next.aout
tar zcvf linux-next-2gb-sparse.disk.tar.gz --sparse linux-next-2gb-sparse.disk 
