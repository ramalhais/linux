#!/bin/bash -x
rm mg_putc.bin mg_putc mg_putc.o

#GCC_SUFFIX="-12"
#KERN_LOADADDR=4001000
KERN_LOADADDR=4400000

#make CC=m68k-linux-gnu-gcc-9 CFLAGS="-Wl,-T'text ${KERN_LOADADDR}'-m68040 -static -nostdlib -ffreestanding -nostdinc" mg_putc

#make CC=m68k-linux-gnu-gcc${GCC_SUFFIX} CFLAGS="-m68040 -ffreestanding -static -nostdlib -nostdinc -nostartfiles -nodefaultlibs" mg_putc.o
#make CC=m68k-linux-gnu-gcc${GCC_SUFFIX} CFLAGS="-m68040 -ffreestanding -mpcrel -fpic -fPIC" mg_putc.o && \
make CC=m68k-linux-gnu-gcc${GCC_SUFFIX} CFLAGS="-m68040 -ffreestanding -pipe -ffixed-a2 -fomit-frame-pointer" mg_putc.o && \
m68k-linux-gnu-ld -N -Ttext $KERN_LOADADDR -e _start mg_putc.o -o mg_putc && \
m68k-linux-gnu-objcopy --output-target=binary mg_putc mg_putc.bin && \
../aout mg_putc.bin mg_putc.bin.aout 0x${KERN_LOADADDR}; \
../simpkern mg_putc.bin mg_putc.bin.simpk; \
../macho mg_putc.bin mg_putc.bin.macho 0x${KERN_LOADADDR}; \
AWKPROG='function x(v) { printf "\\0\\%o\\%o\\%o", (v / 65536) % 256, (v / 256) % 256, v % 256 } {printf "\047\\0\\207\\01\\07"; x($$1); x($$2); x($$3); printf "\\0\\0\\0\\0\\04\\070\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\047"}'; \
(size mg_putc | tail -n +2 | awk "$AWKPROG" | xargs printf ; cat mg_putc.bin) > mg_putc.bin.awkh
#ln -sf $(pwd)/mg_putc.bin.aout ~/next/tftp/private/tftpboot/boot
#ln -sf $(pwd)/mg_putc.bin.awkh ~/next/tftp/private/tftpboot/boot
#ln -sf $(pwd)/mg_putc.bin.simpk ~/next/tftp/private/tftpboot/boot
ln -sf $(pwd)/mg_putc.bin.macho ~/next/tftp/private/tftpboot/boot
