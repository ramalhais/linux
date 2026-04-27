#!/bin/bash -x
set -e

DISK=linux-next-small.disk
./build-NeXT-disk-github.sh $DISK 256M
DISK=linux-next.disk
./build-NeXT-disk-github.sh $DISK 2G

MOUNTP=/mnt/target
sudo mkdir -p $MOUNTP

# Build debian disk image
ORIG_DISK=$DISK
DISK=linux-next-debian-systemd.disk
cp $ORIG_DISK $DISK

# Mount root partition
PARTITION=2
LOOPDEV=$(sudo losetup -f | head -1)
OFFSET=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A1 | grep cp_offset | sed 's/.*(\(.*\))/\1/g')
SECTORS=$(arch/m68k/tools/next/next-disklabel $DISK | grep "Partition $PARTITION" --text -A2 | grep cp_size | sed 's/.*(\(.*\))/\1/g')
sudo losetup --offset=$(( (160+$OFFSET)*1024 )) --sizelimit=$(( $SECTORS*1024 )) $LOOPDEV $DISK
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

tar zcvf $DISK.tar.gz --sparse $DISK



# sysvinit
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

ORIG_DISK=$DISK
DISK=linux-next-debian-sysvinit.disk
mv $ORIG_DISK $DISK

tar zcvf $DISK.tar.gz --sparse $DISK
