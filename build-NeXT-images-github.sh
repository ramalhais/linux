#!/bin/bash -x
set -e

export _USER=user
export _PASSWORD=jobssucks
export _HOST=next

DISK_BASE_SMALL=linux-next-small.disk
./build-NeXT-disk-github.sh $DISK_BASE_SMALL 256M
DISK_BASE=linux-next.disk
./build-NeXT-disk-github.sh $DISK_BASE 2G

MOUNT_DIR=/mnt/target
sudo mkdir -p $MOUNT_DIR



#
# Build debian systemd (default) disk image
#
DISK=linux-next-debian-systemd.disk
cp $DISK_BASE $DISK

# Mount root partition
PARTITION=2
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mount $LOOPDEV $MOUNT_DIR

#sudo debootstrap --variant=minbase --include sysvinit-core,libpam-elogind --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNT_DIR http://deb.debian.org/debian-ports
#sudo sed -i -e 's/systemd systemd-sysv //g' $MOUNT_DIR/debootstrap/required
sudo debootstrap --include debian-ports-archive-keyring,debian-archive-keyring --verbose --no-check-gpg --arch=m68k --foreign unstable $MOUNT_DIR http://deb.debian.org/debian-ports
sudo cp $(which qemu-m68k-static ) $MOUNT_DIR

# sudo mount --make-rslave --rbind /proc $MOUNT_DIR/proc
# sudo mount --make-rslave --rbind /sys $MOUNT_DIR/sys
# sudo mount --make-rslave --rbind /dev $MOUNT_DIR/dev
# sudo mount --make-rslave --rbind /run $MOUNT_DIR/run

#sudo chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x <<EOF
sudo script -qc "chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x" /dev/null <<EOF

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

#sudo umount $MOUNT_DIR/run
#sudo umount $MOUNT_DIR/dev
#sudo umount $MOUNT_DIR/sys
#sudo umount $MOUNT_DIR/proc

sudo umount $MOUNT_DIR
sudo losetup -d $LOOPDEV
sudo sync

tar zcvf $DISK.tar.gz --sparse $DISK



#
# debian sysvinit based on systemd image
#
ORIG_DISK=$DISK
DISK=linux-next-debian-sysvinit.disk
mv $ORIG_DISK $DISK

# Mount root partition
PARTITION=2
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mount $LOOPDEV $MOUNT_DIR

sudo chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i <<EOF

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

sudo umount $MOUNT_DIR
sudo losetup -d $LOOPDEV
sudo sync

tar zcvf $DISK.tar.gz --sparse $DISK



#
# gentoo openrc image
#
DISK=linux-next-gentoo-openrc.disk
cp $DISK_BASE $DISK

# Mount root partition
PARTITION=2
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mount $LOOPDEV $MOUNT_DIR

sudo cp $(which qemu-m68k-static ) $MOUNT_DIR

# sudo mount --make-rslave --rbind /proc $MOUNT_DIR/proc
# sudo mount --make-rslave --rbind /sys $MOUNT_DIR/sys
# sudo mount --make-rslave --rbind /dev $MOUNT_DIR/dev
# sudo mount --make-rslave --rbind /run $MOUNT_DIR/run

echo
echo "Downloading Gentoo stage3 to $MOUNT_DIR"
# TAR_OPTS="--exclude /usr/lib/python3.12/test"
# TAR_OPTS+=" --exclude /usr/share/sgml"
# STAGE3_URL=https://mirrors.xmission.com/gentoo/releases/m68k/autobuilds/20240501T163628Z/stage3-m68k-openrc-20240501T163628Z.tar.xz
# STAGE3_URL=https://mirror.cs.odu.edu/gentoo-distfiles/releases/m68k/autobuilds/20250322T105044Z/stage3-m68k-openrc-20250322T105044Z.tar.xz
# https://web.archive.org/web/*/https://distfiles.gentoo.org/releases/m68k/autobuilds/*
# STAGE3_URL=https://web.archive.org/web/20250726145816/https://distfiles.gentoo.org/releases/m68k/autobuilds/20250716T155236Z/stage3-m68k-openrc-20250716T155236Z.tar.xz
STAGE3_URL=https://distfiles.gentoo.org/releases/m68k/autobuilds/20260424T160106Z/stage3-m68k_a32-t64-openrc-20260424T160106Z.tar.xz
# wget $STAGE3_URL -O - | tar Jxf - -C $MOUNT_DIR
sudo time wget $STAGE3_URL -O $MOUNT_DIR/stage3

echo
echo "Extracting Gentoo stage3 to $MOUNT_DIR"
sudo time tar $TAR_OPTS Jxf $MOUNT_DIR/stage3 -C $MOUNT_DIR
sudo rm $MOUNT_DIR/stage3

echo
echo "Fixing login timeout"
sudo sed -i 's/\(LOGIN_TIMEOUT\).*/\1\t120/g' $MOUNT_DIR/etc/login.defs

#sudo chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x <<EOF
sudo script -qc "chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x" /dev/null <<EOF

passwd <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

useradd --create-home --no-user-group --shell /bin/bash $_USER
passwd $_USER <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

cat >> /etc/fstab <<EOF2
LABEL=/     /      auto defaults  0 1
LABEL=swap  none   swap sw        0 0
LABEL=boot  /boot  auto defaults  0 0
EOF2

EOF

sudo umount $MOUNT_DIR
sudo losetup -d $LOOPDEV
sudo sync

tar zcvf $DISK.tar.gz --sparse $DISK


#
# gentoo systemd image
#
DISK=linux-next-gentoo-systemd.disk
cp $DISK_BASE $DISK

# Mount root partition
PARTITION=2
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
sudo mount $LOOPDEV $MOUNT_DIR

sudo cp $(which qemu-m68k-static ) $MOUNT_DIR

# sudo mount --make-rslave --rbind /proc $MOUNT_DIR/proc
# sudo mount --make-rslave --rbind /sys $MOUNT_DIR/sys
# sudo mount --make-rslave --rbind /dev $MOUNT_DIR/dev
# sudo mount --make-rslave --rbind /run $MOUNT_DIR/run

echo
echo "Downloading Gentoo stage3 to $MOUNT_DIR"
# https://web.archive.org/web/*/https://distfiles.gentoo.org/releases/m68k/autobuilds/*
#STAGE3_URL=https://web.archive.org/web/20250329201041/https://distfiles.gentoo.org/releases/m68k/autobuilds/20250322T105044Z/stage3-m68k-systemd-20250322T105044Z.tar.xz
STAGE3_URL=https://distfiles.gentoo.org/releases/m68k/autobuilds/20260424T160106Z/stage3-m68k_a32-t64-systemd-20260424T160106Z.tar.xz
# wget $STAGE3_URL -O - | tar Jxf - -C $MOUNT_DIR
sudo time wget $STAGE3_URL -O $MOUNT_DIR/stage3

echo
echo "Extracting Gentoo stage3 to $MOUNT_DIR"
sudo time tar $TAR_OPTS Jxf $MOUNT_DIR/stage3 -C $MOUNT_DIR
sudo rm $MOUNT_DIR/stage3

echo
echo "Fixing login timeout"
sudo sed -i 's/\(LOGIN_TIMEOUT\).*/\1\t120/g' $MOUNT_DIR/etc/login.defs

#sudo chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x <<EOF
sudo script -qc "chroot $MOUNT_DIR /qemu-m68k-static /bin/sh -i -x" /dev/null <<EOF

passwd <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

useradd --create-home --no-user-group --shell /bin/bash $_USER
passwd $_USER <<EOF2
${_PASSWORD}
${_PASSWORD}
EOF2

cat >> /etc/fstab <<EOF2
LABEL=/     /      auto defaults  0 1
LABEL=swap  none   swap sw        0 0
LABEL=boot  /boot  auto defaults  0 0
EOF2

EOF

sudo umount $MOUNT_DIR
sudo losetup -d $LOOPDEV
sudo sync

tar zcvf $DISK.tar.gz --sparse $DISK
