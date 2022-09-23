rm test test.o

#make CC=m68k-linux-gnu-gcc-9 CFLAGS="-Wl,-T'text 4380000'-m68040 -static -nostdlib -ffreestanding -nostdinc" test
#make CC=m68k-linux-gnu-gcc-9 CFLAGS="-m68040 -static -nostdlib -ffreestanding -nostdinc -nostartfiles -nodefaultlibs" test.o
#make CC=m68k-linux-gnu-gcc-9 CFLAGS="-m68040 -ffreestanding -mpcrel -fpic -fPIC" test.o && \
make CC=m68k-linux-gnu-gcc-9 CFLAGS="-pipe -ffixed-a2 -fomit-frame-pointer -m68040 -ffreestanding" test.o && \
#m68k-linux-gnu-ld -N -Ttext 4380000 -e _start test.o -o test && \
#m68k-linux-gnu-ld -N -Ttext 4380000 -e _start test.o -o test && \
m68k-linux-gnu-ld -N -Ttext 4001000 -e _start test.o -o test && \

m68k-linux-gnu-objcopy --output-target=binary test test.bin && \
./next/simpkern test.bin test.bin.macho; \
./aout test.bin test.bin.aout; \
AWKPROG='function x(v) { printf "\\0\\%o\\%o\\%o", (v / 65536) % 256, (v / 256) % 256, v % 256 } {printf "\047\\0\\207\\01\\07"; x($$1); x($$2); x($$3); printf "\\0\\0\\0\\0\\04\\070\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\047"}'; \
(size test | tail +2 | awk "$AWKPROG" | xargs printf ; cat test.bin) > test.bin.awkh
sudo cp test.bin.aout /srv/tftp/
sudo cp test.bin.macho /srv/tftp/
sudo cp test.bin.awkh /srv/tftp/
sudo ln -sf test.bin.aout /srv/tftp/boot
