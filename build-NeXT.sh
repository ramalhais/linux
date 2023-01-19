#!/bin/bash -x

GCC_SUFFIX="-12"

# Needed for network boot, because PROM uses the addr on the aout or macho header.
# Not needed for the netbsd bootloader because it adds 0x4000000 to the entrypoint if lower than 0x4000000
ADD_OFFSET=1

# If not defined here, we'll generate the entrypoint address from the compiled kernel by adding 0x4000000 (NeXT first memory slot address)
# WARNING: this doesn't seem to work. Maybe head.S needs to be fixed to use relative addressing?
#KERN_LOADADDR="4001000" # linux m68k default: 0x4000000(memory address base first slot) + 4k stack/heap?
#KERN_LOADADDR="4390000" # 0x4380000 (netbsd standalone boot) + 64k boot sector
#KERN_LOADADDR="4020000" # 0x4000000 + 2*64k boot sector.
#KERN_LOADADDR="4013000" # This seems to be the minimum address that works with the netbsd bootloader

if (grep fedora /etc/os-release); then
  GCC_SUFFIX=""
fi

export ARCH=m68k CROSS_COMPILE=m68k-linux-gnu- GCC_SUFFIX=$GCC_SUFFIX
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

# Compile NeXT tools for m68k (disabled. not working on redhat. missing m68k glibc headers and libraries)
#make CC=${CROSS_COMPILE}gcc${GCC_SUFFIX} -j$NPROCS -C arch/m68k/tools/next/
#for BIN in aout macho simpkern next-disklabel; do
#        mv arch/m68k/tools/next/$BIN arch/m68k/tools/next/$BIN.m68k
#done
touch arch/m68k/tools/next/aout.m68k
touch arch/m68k/tools/next/macho.m68k

# Compile NeXT tools
make -C arch/m68k/tools/next/

# Copy netbsd bootloader
cp ~/next/netbsd-obj/netbsd-boot-next.aout arch/m68k/tools/next/

#
# Kernel image
#

make -j$[$(nproc)*2] || exit 1

#DATE=$(date +%Y%m%d_%H%M%S)
DATE=$(date +%F-%H.%M.%S)
KERNEL_VERSION=$(make kernelversion)
KERNEL_RELEASE=$(make kernelrelease)

# Strip symbols
m68k-linux-gnu-strip --strip-unneeded vmlinux -o vmlinux.stripped

# Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --strip-unneeded --output-target=binary vmlinux vmlinux.binary_$DATE

if [ -z $KERN_LOADADDR ]; then
	MEM_BASE=4000000
	KERN_LOADADDR=$(m68k-linux-gnu-objdump -D vmlinux|grep '<_stext>:'|cut -f1 -d' ')

	IS_OFFSET=$(echo "ibase=16; ${KERN_LOADADDR} < ${MEM_BASE}" | bc)
	if [ $IS_OFFSET -eq 1 ] && [ $ADD_OFFSET -eq 1 ]; then
		KERN_LOADADDR=$(echo "obase=16; ibase=16; ${MEM_BASE}+${KERN_LOADADDR}" | bc)
	fi
fi

# NOTE:
# - NeXT PROM can boot aout and macho executables from network.
# - NeXT PROM boots the bootloader from the disk at 32k(copy at 96k)
# - NeXT disk bootloader supports macho only?
# - NetBSD disk bootloader supports aout and elf32 (and elf64 and ecoff)

# Wrap kernel binary code in Mach-O header (bigger than aout header)
#./arch/m68k/tools/next/simpkern vmlinux.binary_$DATE vmlinux.simpk_$DATE
./arch/m68k/tools/next/macho vmlinux.binary_$DATE vmlinux.macho_$DATE 0x${KERN_LOADADDR}
#sudo cp vmlinux.macho_$DATE /srv/tftp/
#sudo ln -sf vmlinux.macho_$DATE /srv/tftp/boot
#ln -sf ~/next/linux/vmlinux.simpk_$DATE ~/next/tftp/private/tftpboot/boot
#ln -sf ~/next/linux/vmlinux.macho_$DATE ~/next/tftp/private/tftpboot/boot

# Wrap kernel binary code in aout header (old UNIX COFF format?)
./arch/m68k/tools/next/aout vmlinux.binary_$DATE vmlinux.aout_$DATE 0x${KERN_LOADADDR}
#sudo cp vmlinux.aout_$DATE /srv/tftp/
#sudo ln -sf vmlinux.aout_$DATE /srv/tftp/boot
ln -sf ~/next/linux/vmlinux.aout_$DATE ~/next/tftp/private/tftpboot/boot

### Save patch
git diff master > ../linux-NeXT-$DATE.patch
# git tag NeXT-$(date +%F-%H.%M.%S)
# git push --tags

### Save .config
cp .config ../.config-NeXT-$DATE

### Show generated assembly code
# m68k-linux-gnu-objdump -D vmlinux|less
