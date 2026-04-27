#!/bin/bash -x
set -e

MOUNTP=/mnt/target
FS_LABEL=/

# Build 2GB empty bootable disk image
DISK=linux-next-2gb-sparse.disk

dd if=/dev/zero of=$DISK bs=2G count=1 conv=sparse

# Create disklabel/partitions and install boot sector
arch/m68k/tools/next/next-disklabel $DISK -c
arch/m68k/tools/next/next-disklabel $DISK -b arch/m68k/tools/next/netbsd-boot-next.aout

# boot partition
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( 160*1024 )) --sizelimit=$(( 65536*1024 )) $LOOPDEV $DISK
sudo mkfs.vfat -n boot $LOOPDEV
sudo mkdir -p $MOUNTP/boot
sudo mount $LOOPDEV $MOUNTP/boot
sudo cp vmlinux.stripped $MOUNTP/boot/vmlinux
sudo umount $MOUNTP/boot
sudo losetup -d $LOOPDEV

# swap partition
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( (160+65536)*1024 )) --sizelimit=$(( 131072*1024 )) $LOOPDEV $DISK
sudo mkswap -L swap $LOOPDEV
sudo losetup -d $LOOPDEV

# root partition
LOOPDEV=$(sudo losetup -f | head -1)
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition 2" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+65536+131072)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mkfs.ext2 -m0 -L$FS_LABEL -r0 $LOOPDEV
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP
sudo cp vmlinux.stripped $MOUNTP/vmlinux
sudo umount $MOUNTP
sudo losetup -d $LOOPDEV

tar zcvf $DISK.tar.gz --sparse $DISK

# Build debian disk image
ORIG_DISK=$DISK
DISK=linux-next-2gb-debian-systemd.disk
mv $ORIG_DISK $DISK

# Mount root partition
LOOPDEV=$(sudo losetup -f | head -1)
sudo losetup --offset=$(( (160+65536+131072)*1024 )) $LOOPDEV $DISK
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP

#sudo debootstrap --variant=minbase --include sysvinit-core,libpam-elogind --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNTP http://deb.debian.org/debian-ports
#sudo sed -i -e 's/systemd systemd-sysv //g' $MOUNTP/debootstrap/required
sudo debootstrap --include debian-ports-archive-keyring,debian-archive-keyring --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNTP http://deb.debian.org/debian-ports
sudo cp $(which qemu-m68k-static ) $MOUNTP

export _USER=user
export _PASSWORD=jobssucks
export _HOST=next

# sudo mount --make-rslave --rbind /proc $MOUNTP/proc
# sudo mount --make-rslave --rbind /sys $MOUNTP/sys
# sudo mount --make-rslave --rbind /dev $MOUNTP/dev
# sudo mount --make-rslave --rbind /run $MOUNTP/run

#sudo chroot $MOUNTP /qemu-m68k-static /bin/sh -i -x <<EOF
sudo script -qc "chroot $MOUNTP /qemu-m68k-static /bin/sh -i -x" /dev/null <<EOF

echo "proc /proc proc defaults 0 0" >> /etc/fstab
echo "devtmpfs /dev devtmpfs defaults 0 0" >> /etc/fstab
echo "sysfs /sys sysfs defaults 0 0" >> /etc/fstab

apt install -y debian-ports-archive-keyring lsof

/debootstrap/debootstrap --second-stage

echo "### LOG /debootstrap/debootstrap.log ###"
cat /debootstrap/debootstrap.log
echo "### LOG END /debootstrap/debootstrap.log ###"

#systemd-machine-id-setup
mount /proc
mount
apt --fix-broken -y install
apt-get update
apt-get -y upgrade
#apt-get dist-upgrade

echo $_HOST > /etc/hostname

passwd <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

useradd --create-home --shell /bin/bash $_USER
passwd $_USER <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

apt -y install sudo
usermod --append --groups sudo $_USER

apt -y install \
systemd-sysv \
ntpsec-ntpdate \
wget \
curl \
locales \
xserver-xorg \
x11-utils xinit xterm \
twm \
openssh-server \
strace \
netcat-traditional \
gpm \
evtest

#xserver-xorg-input-evdev
#mesa-utils \
#wmaker wmaker-data wmaker-utils \

#apt -y install console-setup console-setup-linux
#tasksel install standard
#dpkg-reconfigure tzdata
#dpkg-reconfigure locales
#dpkg-reconfigure keyboard-configuration

apt clean

cp /etc/pam.d/common-auth /etc/pam.d/common-auth.ORIG
sed -i 's/pam_unix.so nullok/pam_debug.so creds=success/g' /etc/pam.d/common-auth

mkdir -p /etc/network/interfaces.d
cat > /etc/network/interfaces.d/eth0 <<EOF2
auto eth0
iface eth0 inet dhcp
#hwaddress ether 00:00:0f:12:34:56
EOF2

cat > /root/.xinitrc <<EOF2
xev &
EOF2

apt clean

umount /proc
echo "### BUILD $DISK END ###"
EOF

#sudo umount $MOUNTP/run
#sudo umount $MOUNTP/dev
#sudo umount $MOUNTP/sys
#sudo umount $MOUNTP/proc

sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
tar zcvf $DISK.tar.gz --sparse $DISK

ORIG_DISK=$DISK
DISK=linux-next-2gb-debian-sysvinit.disk
mv $ORIG_DISK $DISK

LOOPDEV=$(sudo losetup -f | head -1)
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition 2" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+65536+131072)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP

sudo chroot $MOUNTP /qemu-m68k-static /bin/sh -i <<EOF

mount /proc
mount
# apt -y libpam-elogind
# apt -y install sysvinit-core rsyslog
apt -y --purge --allow-remove-essential install sysvinit-core libpam-elogind dbus-x11 systemd-sysv-
# apt-mark hold systemd systemd-sysv

apt clean
umount /proc

echo "### BUILD $DISK END ###"
EOF

sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
tar zcvf $DISK.tar.gz --sparse $DISK
