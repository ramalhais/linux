#!/bin/bash -x
set -e

export ARCH=m68k
export CROSS_COMPILE=m68k-linux-gnu-
export GCC_SUFFIX=-14

NPROCS=$[$(nproc)*2]

# Compile NeXT tools for m68k
make CC=${CROSS_COMPILE}gcc${GCC_SUFFIX} -j$NPROCS -C arch/m68k/tools/next/
for BIN in aout macho simpkern next-disklabel; do
	mv arch/m68k/tools/next/$BIN arch/m68k/tools/next/$BIN.m68k
done

# Compile NeXT tools for amd64
make -j$NPROCS -C arch/m68k/tools/next/

# Get netbsd bootloader
curl -v -L -o arch/m68k/tools/next/netbsd-boot-next.aout https://github.com/ramalhais/netbsd-boot-NeXT/releases/latest/download/netbsd-boot-next.aout

# DATE=$(date +%F-%H.%M.%S)
# KERNELVER=$(make kernelversion)

build-kernel() {
	VARIANT=$1

	time make -j$NPROCS

	LINUX_BINARY_ELF_VARIANT=vmlinux-${VARIANT}
	LINUX_NEXT_VARIANT=vmlinux-NeXT-${VARIANT}

	# Save original
	cp vmlinux $LINUX_BINARY_ELF_VARIANT

	### Strip symbols
	m68k-linux-gnu-strip --strip-unneeded $LINUX_BINARY_ELF_VARIANT -o $LINUX_BINARY_ELF_VARIANT.stripped

	### Extract binary from ELF kernel image
	LINUX_BINARY=$LINUX_BINARY_ELF_VARIANT.binary
	m68k-linux-gnu-objcopy --strip-unneeded --output-target=binary $LINUX_BINARY_ELF_VARIANT $LINUX_BINARY

	ADD_OFFSET=1
	if [ -z $KERN_LOADADDR ]; then
		MEM_BASE=4000000
		# KERN_LOADADDR=$(m68k-linux-gnu-objdump -D $LINUX_BINARY_ELF_VARIANT | grep '<_stext>:' | cut -f1 -d' ')
		KERN_LOADADDR=$(m68k-linux-gnu-objdump --all-headers $LINUX_BINARY_ELF_VARIANT | grep _stext | cut -f1 -d' ')

		IS_OFFSET=$(echo "ibase=16; ${KERN_LOADADDR} < ${MEM_BASE}" | bc)
		if [ $IS_OFFSET -eq 1 ] && [ $ADD_OFFSET -eq 1 ]; then
			KERN_LOADADDR=$(echo "obase=16; ibase=16; ${MEM_BASE}+${KERN_LOADADDR}" | bc)
		fi
	fi

	./arch/m68k/tools/next/aout $LINUX_BINARY $LINUX_NEXT_VARIANT.aout 0x${KERN_LOADADDR}
	./arch/m68k/tools/next/simpkern $LINUX_BINARY $LINUX_NEXT_VARIANT.macho-simpkern
	./arch/m68k/tools/next/macho $LINUX_BINARY $LINUX_NEXT_VARIANT.macho 0x${KERN_LOADADDR}
}

# Compile Kernel
make next_defconfig
build-kernel defconfig

scripts/config --enable NEXT_DEBUG
build-kernel debug

scripts/config --enable NEXT_SCSI_DEBUG
build-kernel debug_scsidebug

scripts/config --disable NEXT_SCSI
build-kernel debug_noscsi
