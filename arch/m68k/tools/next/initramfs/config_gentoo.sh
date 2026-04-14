set -e
set +x

echo
echo "Setting up networking using DHCP"
./config_net.sh

echo
echo "List of available disks:"
cd /sys/block
SDS=$(ls -d1 sd*)
COUNT=0
for SD in $SDS; do
	VENDOR=$(cat $SD/device/vendor)
	MODEL=$(cat $SD/device/model)
	SECTORS=$(cat $SD/size)
	BYTES=$(($SECTORS*512))
	SIZE_GB=$(echo "scale=1;${BYTES}/1024/1024/1024" | bc)
	echo "$SD: $VENDOR $MODEL ${SIZE_GB}GB"
	COUNT=$[$COUNT+1]
done
cd - >/dev/null

if [ $COUNT == 0 ]; then
	echo "No disks found. Exiting"
	exit 1
fi

if [ $COUNT == 1 ]; then
	echo
	echo "Auto-selecting disk $SD"
else
	echo
	read -p "Select disk to erase and install to: " SD
fi

echo
echo "$SD disk details:"
echo

DEVICE=/dev/$SD
fdisk -l $DEVICE

echo
echo 
read -p "Are you sure you want to use and erase disk $SD ? " CONFIRM
if [ ${CONFIRM} != "y" ] && [ ${CONFIRM} != "yes" ]; then
	echo "Cancelled. Exiting."
	exit 0
fi

echo
echo "Creating NeXTSTEP disklabel and partition in disk $SD and installing NetBSD bootloader onto the bootsector"
next-disklabel $DEVICE -c -b netbsd-boot-next.aout

ROOT_PARTITION=${DEVICE}1
ROOT_PARTITION_LABEL=/
MOUNT_DIR=/mnt/target
umount $MOUNT_DIR &>/dev/null || true

echo
echo "Creating ext2 filesystem in $ROOT_PARTITION with LABEL=$ROOT_PARTITION_LABEL"
mke2fs -m0 -L"$ROOT_PARTITION_LABEL" -r0 $ROOT_PARTITION

echo
echo "Mounting $ROOT_PARTITION in $MOUNT_DIR"
mkdir -p $MOUNT_DIR
mount $ROOT_PARTITION $MOUNT_DIR

echo
echo "Downloading NeXT linux kernel"
VMLINUX_URL=https://github.com/ramalhais/linux/releases/latest/download/vmlinux
time wget $VMLINUX_URL -O $MOUNT_DIR/vmlinux

echo
echo "Downloading Gentoo stage3 to $MOUNT_DIR"
# TAR_OPTS="--exclude /usr/lib/python3.12/test"
# TAR_OPTS+=" --exclude /usr/share/sgml"
# STAGE3_URL=https://mirrors.xmission.com/gentoo/releases/m68k/autobuilds/20240501T163628Z/stage3-m68k-openrc-20240501T163628Z.tar.xz
# STAGE3_URL=https://mirror.cs.odu.edu/gentoo-distfiles/releases/m68k/autobuilds/20250322T105044Z/stage3-m68k-openrc-20250322T105044Z.tar.xz
# https://web.archive.org/web/*/https://distfiles.gentoo.org/releases/m68k/autobuilds/*
STAGE3_URL=https://web.archive.org/web/20250726145816/https://distfiles.gentoo.org/releases/m68k/autobuilds/20250716T155236Z/stage3-m68k-openrc-20250716T155236Z.tar.xz
# wget $STAGE3_URL -O - | tar Jxf - -C $MOUNT_DIR
time wget $STAGE3_URL -O $MOUNT_DIR/stage3

echo
echo "Extracting Gentoo stage3 to $MOUNT_DIR"
time tar $TAR_OPTS Jxf $MOUNT_DIR/stage3 -C $MOUNT_DIR
rm $MOUNT_DIR/stage3

echo
echo "Fixing login timeout"
sed -i 's/\(LOGIN_TIMEOUT\).*/\1\t120/g' $MOUNT_DIR/etc/login.defs

echo
echo "Switching root to $MOUNT_DIR using $INIT"
INIT=/sbin/init
exec switch_root $MOUNT_DIR $INIT
