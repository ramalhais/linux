#!/bin/bash -x

export ARCH=m68k CROSS_COMPILE=m68k-linux-gnu- GCC_SUFFIX=-12
export _MODULES_DIR=~/next/modules
export _BUILDROOT_DIR=~/git/buildroot
export _BUILDROOT_OVERLAY_DIR=$_BUILDROOT_DIR/linux-modules/

# sync tags with upstream linux repo
# initial: git remote add --fetch --tags upstream https://github.com/torvalds/linux.git
#
# git fetch --tags upstream
# git push --tags

### Generate NeXT defconfig
# make tinyconfig
# make savedefconfig
# cp defconfig arch/m68k/configs/next_defconfig

### Setup NeXT config
#make next_defconfig
#make next_fb_defconfig
#make next_fb_net_defconfig
#make next_fb_net_nfs_defconfig
#make next_fb_net_nfs_iscsi_fs_scsi_part_defconfig

#
# Kernel Modules
#

if false; then
# Cleanup and build modules
rm -rf $_MODULES_DIR
mkdir $_MODULES_DIR
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$_MODULES_DIR modules_install

# Copy modules to buildroot overlay
rm -rf $_BUILDROOT_OVERLAY_DIR
rsync -av --mkpath $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/modules.dep $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/
rsync -av --mkpath $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/kernel/fs/nfs/ $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/kernel/fs/nfs/
rsync -av --mkpath $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/kernel/net/sunrpc/ $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/kernel/net/sunrpc/

rsync -av --mkpath $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/kernel/drivers/target/ $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/kernel/drivers/target/
rsync -av --mkpath $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/kernel/drivers/scsi/ $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/kernel/drivers/scsi/
rsync -av --mkpath --include='**crc**' --exclude='*' $_MODULES_DIR/lib/modules/$KERNEL_RELEASE/kernel/lib/ $_BUILDROOT_OVERLAY_DIR/lib/modules/$KERNEL_RELEASE/kernel/lib/

#
# buildroot image
#

# Clean old target dir
rm -rf $_BUILDROOT_DIR/output/target
find $_BUILDROOT_DIR/output/ -name ".stamp_target_installed" -delete
rm -f $_BUILDROOT_DIR/output/build/host-gcc-final-*/.stamp_host_installed

# Build buildroot
make -C $_BUILDROOT_DIR
fi

#
# Kernel image
#

make -j$[$(nproc)*2] || exit 1

#DATE=$(date +%Y%m%d_%H%M%S)
DATE=$(date +%F-%H.%M.%S)
KERNEL_VERSION=$(make kernelversion)
KERNEL_RELEASE=$(make kernelrelease)

# Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary_$DATE

# Compile NeXT tools
make -C arch/m68k/tools/next/

# Wrap kernel binary code in Mach-O header (bigger than aout (COFF?) header)
#./arch/m68k/tools/next/simpkern vmlinux.binary_$DATE vmlinux.macho_$DATE
#sudo cp vmlinux.macho_$DATE /srv/tftp/
#sudo ln -sf vmlinux.macho_$DATE /srv/tftp/boot

# Wrap kernel binary code in aout header (old UNIX COFF format?)
./arch/m68k/tools/next/aout vmlinux.binary_$DATE vmlinux.netimg_aout_$DATE
#sudo cp vmlinux.netimg_aout_$DATE /srv/tftp/
#sudo ln -sf vmlinux.netimg_aout_$DATE /srv/tftp/boot
ln -sf ~/next/linux/vmlinux.netimg_aout_$DATE ~/next/tftp/private/tftpboot/boot

### Save patch
git diff master > ../linux-NeXT-$DATE.patch
# git tag NeXT-$(date +%F-%H.%M.%S)
# git push --tags

### Save .config
cp .config ../.config-NeXT-$DATE

### Show generated assembly code
# m68k-linux-gnu-objdump -D vmlinux|less
