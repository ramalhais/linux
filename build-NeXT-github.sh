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
arch/m68k/tools/next/next-disklabel linux-next-2gb-sparse.disk -c
arch/m68k/tools/next/next-disklabel linux-next-2gb-sparse.disk -b arch/m68k/tools/next/netbsd-boot-next.aout
sudo mkfs.ext2 -m0 -L/ -r0 $LOOPDEV
sudo mkdir -p /mnt/bla
sudo mount $LOOPDEV /mnt/bla
sudo cp vmlinux.stripped /mnt/bla/vmlinux
sudo umount /mnt/bla
sudo losetup -d $LOOPDEV
tar zcvf linux-next-2gb-sparse.disk.tar.gz --sparse linux-next-2gb-sparse.disk 

# Build debian disk image
DISK=linux-next-2gb-debian-sparse.disk
MOUNTP=/mnt/bla
dd if=/dev/zero of=$DISK bs=2G count=1 conv=sparse
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$((160*1024)) $LOOPDEV $DISK
arch/m68k/tools/next/next-disklabel $DISK -c
arch/m68k/tools/next/next-disklabel $DISK -b arch/m68k/tools/next/netbsd-boot-next.aout
sudo mkfs.ext2 -m0 -L/ -r0 $LOOPDEV
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP
sudo cp vmlinux.stripped $MOUNTP/vmlinux

sudo debootstrap --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNTP http://deb.debian.org/debian-ports
sudo cp $(which qemu-m68k-static ) $MOUNTP
sudo chroot /mnt /qemu-m68k-static /bin/sh -c '/debootstrap/debootstrap --second-stage; echo "none /proc proc defaults 0 0" >> /etc/fstab; echo "none /sys sysfs defaults 0 0" >> /etc/fstab'

sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
tar zcvf $DISK.tar.gz --sparse $DISK

