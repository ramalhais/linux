#!/bin/bash -x

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-9

### Generate NeXT defconfig
# make tinyconfig
# make savedefconfig
# cp defconfig arch/m68k/configs/next_defconfig

### Setup NeXT config
#make next_defconfig

### Build kernel
make -j$[$(nproc)*2] || exit 1

#DATE=$(date +%Y%m%d_%H%M%S)
DATE=$(date +%F-%H.%M.%S)

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
sudo cp vmlinux.netimg_aout_$DATE /srv/tftp/
sudo ln -sf vmlinux.netimg_aout_$DATE /srv/tftp/boot

### Save patch
git diff v6.0-rc3 > ../linux-v6.0-rc3-NeXT-$DATE.patch

### Save .config
cp .config ../.config-NeXT-$DATE

### Show generated assembly code
# m68k-linux-gnu-objdump -D vmlinux|less
