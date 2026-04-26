#!/bin/bash -x
set -e

MOUNTP=/mnt/target
FS_LABEL=/

# Build small empty bootable disk image
DISK=linux-next-tiny-sparse.disk

dd if=/dev/zero of=$DISK bs=256M count=1 conv=sparse
arch/m68k/tools/next/next-disklabel $DISK -c
arch/m68k/tools/next/next-disklabel $DISK -b arch/m68k/tools/next/netbsd-boot-next.aout

LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( 160*1024 )) --sizelimit=$(( 65536*1024 )) $LOOPDEV $DISK
sudo mkfs.vfat -n boot $LOOPDEV
sudo mkdir -p $MOUNTP/boot
sudo mount $LOOPDEV $MOUNTP/boot
sudo cp vmlinux.stripped $MOUNTP/boot/vmlinux
sudo umount $MOUNTP/boot
sudo losetup -d $LOOPDEV

LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( (160+65536)*1024 )) --sizelimit=$(( 131072*1024 )) $LOOPDEV $DISK
sudo mkswap -L swap $LOOPDEV
sudo losetup -d $LOOPDEV

LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( (160+65536+131072)*1024 )) $LOOPDEV $DISK
sudo mkfs.ext2 -m0 -L$FS_LABEL -r0 $LOOPDEV
sudo losetup -d $LOOPDEV

tar zcvf $DISK.tar.gz --sparse $DISK
