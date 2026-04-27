#!/bin/bash -x
set -e

MOUNTP=/mnt/target
sudo mkdir -p $MOUNTP

DISK=$1
SIZE=$2

dd if=/dev/zero of=$DISK bs=256M count=1 conv=sparse
arch/m68k/tools/next/next-disklabel $DISK -c
arch/m68k/tools/next/next-disklabel $DISK -b arch/m68k/tools/next/netbsd-boot-next.aout

# boot partition
PARTITION=1
LABEL=boot
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mkfs.vfat -n $LABEL $LOOPDEV
sudo mount $LOOPDEV $MOUNTP
sudo cp vmlinux.stripped $MOUNTP/vmlinux
sudo umount $MOUNTP
sudo losetup -d $LOOPDEV

# swap partition
PARTITION=1
LABEL=swap
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mkswap -L $LABEL $LOOPDEV
sudo losetup -d $LOOPDEV

# root partition
PARTITION=2
LABEL=/
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mkfs.ext2 -m0 -L$LABEL -r0 $LOOPDEV
sudo losetup -d $LOOPDEV

tar zcvf $DISK.tar.gz --sparse $DISK
