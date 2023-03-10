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
make next_defconfig
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

MOUNTP=/mnt/target
FS_LABEL=/

# Build empty bootable disk image
DISK=linux-next-2gb-sparse.disk

dd if=/dev/zero of=$DISK bs=2G count=1 conv=sparse
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

# Build debian disk image
ORIG_DISK=$DISK
DISK=linux-next-2gb-debian-sparse.disk
cp $ORIG_DISK $DISK

LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$((160*1024)) $LOOPDEV $DISK
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP

#sudo debootstrap --variant=minbase --include sysvinit-core,libpam-elogind --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNTP http://deb.debian.org/debian-ports
#sudo sed -i -e 's/systemd systemd-sysv //g' $MOUNTP/debootstrap/required
sudo debootstrap --include debian-ports-archive-keyring --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNTP http://deb.debian.org/debian-ports
sudo cp $(which qemu-m68k-static ) $MOUNTP

export _USER=user
export _PASSWORD=jobssucks
export _HOST=next

#sudo mount --make-rslave --rbind /proc $MOUNTP/proc
#sudo mount --make-rslave --rbind /sys $MOUNTP/sys
#sudo mount --make-rslave --rbind /dev $MOUNTP/dev
#sudo mount --make-rslave --rbind /run $MOUNTP/run

sudo chroot $MOUNTP /qemu-m68k-static /bin/sh -i <<EOF

echo "proc /proc proc defaults 0 0" >> /etc/fstab
echo "devtmpfs /dev devtmpfs defaults 0 0" >> /etc/fstab
echo "sysfs /sys sysfs defaults 0 0" >> /etc/fstab

/debootstrap/debootstrap --second-stage
apt --fix-broken -y install
apt-get update
apt-get -y upgrade
#apt-get dist-upgrade

echo $_HOST > /etc/hostname
passwd <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

useradd -m $_USER
passwd $_USER <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

apt -y install sudo
usermod -aG sudo $_USER

apt -y install strace wget curl locales xserver-xorg
#apt -y install openssh-server
#apt -y install console-setup console-setup-linux
#tasksel install standard
#dpkg-reconfigure tzdata
#apt -y install locales
#dpkg-reconfigure locales
#dpkg-reconfigure keyboard-configuration

EOF

#sudo umount $MOUNTP/run
#sudo umount $MOUNTP/dev
#sudo umount $MOUNTP/sys
#sudo umount $MOUNTP/proc

sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
tar zcvf $DISK.tar.gz --sparse $DISK
