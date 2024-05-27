LOOPDEV=$(sudo losetup -f | head -1)
DISK=/media/ramalhais/SD32GB/HD10-20mb-sparse.hda
sudo losetup --offset=$((160*1024)) $LOOPDEV $DISK

MOUNTP=/mnt/target
sudo mkdir -p $MOUNTP
sudo mount $LOOPDEV $MOUNTP

sudo cp vmlinux.stripped $MOUNTP/vmlinux

sudo umount $MOUNTP
sudo losetup -d $LOOPDEV
sudo eject /media/ramalhais/SD32GB/
