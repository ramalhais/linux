#!/bin/bash -x

export ARCH=m68k CROSS_COMPILE=m68k-linux-gnu- GCC_SUFFIX=-12

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

### Build kernel
make -j$[$(nproc)*2] || exit 1

#DATE=$(date +%Y%m%d_%H%M%S)
DATE=$(date +%F-%H.%M.%S)
KERNELVER=$(make kernelversion)

### Extract binary from ELF kernel image
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary_$DATE

### Compile NeXT tools
make -C arch/m68k/tools/next/

### Wrap kernel binary code in Mach-O header (bigger than aout (COFF?) header)
#./arch/m68k/tools/next/simpkern vmlinux.binary_$DATE vmlinux.macho_$DATE
#sudo cp vmlinux.macho_$DATE /srv/tftp/
#sudo ln -sf vmlinux.macho_$DATE /srv/tftp/boot

### Wrap kernel binary code in aout header (old UNIX COFF format?)
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
