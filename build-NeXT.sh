#!/bin/bash -x
export ARCH=m68k CROSS_COMPILE=m68k-linux-gnu- GCC_SUFFIX=-9
# make tinyconfig
# make savedefconfig
# cp defconfig arch/m68k/configs/next_defconfig
make next_defconfig
make -j$[$(nproc)*2]
DATE=$(date +%Y%m%d_%H%M%S)
m68k-linux-gnu-objcopy --output-target=binary vmlinux vmlinux.binary_$DATE
make -C arch/m68k/tools/next/
#./arch/m68k/tools/next/simpkern vmlinux.binary_$DATE vmlinux.netimg_$DATE
#sudo cp vmlinux.netimg_$DATE /srv/tftp/
./arch/m68k/tools/next/aout vmlinux.binary_$DATE vmlinux.netimg_aout_$DATE
sudo cp vmlinux.netimg_aout_$DATE /srv/tftp/
sudo ln -sf vmlinux.netimg_aout_$DATE /srv/tftp/boot
git diff v6.0-rc3 > ../linux-v6.0-rc3-NeXT-$(date +%F-%H.%M.%S).patch
# m68k-linux-gnu-objdump -D vmlinux|less
