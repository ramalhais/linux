#!/bin/bash -x
set -e

MOUNTP=/mnt/target
FS_LABEL=/

# Build small empty bootable disk image
DISK=linux-next-20mb-sparse.disk

dd if=/dev/zero of=$DISK bs=20M count=1 conv=sparse
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$((160*1024)) $LOOPDEV $DISK
arch/m68k/tools/next/next-disklabel $DISK -c
arch/m68k/tools/next/next-disklabel $DISK -b arch/m68k/tools/next/netbsd-boot-next.aout
sudo mkfs.ext2 -m0 -L$FS_LABEL -r0 $LOOPDEV
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP
sudo cp vmlinux.stripped $MOUNTP/vmlinux
sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
tar zcvf $DISK.tar.gz --sparse $DISK
