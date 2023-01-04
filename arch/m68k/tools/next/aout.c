/*
 *  linux/arch/m68k/tools/next/aout.c
 *
 *  Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
 *
 *  Add aout (COFF?) header to kernel binary to boot on the NeXT.
 *  We can instead add a mach-o header. See simpkern.c .
 *
 */

#include <sys/stat.h>	// fstat()
#include <fcntl.h>	// open()
#include <unistd.h>	// close()
#include <stdio.h>	// printf()
#include <stdlib.h>	// exit()
#include <arpa/inet.h>	// htonl()

#define LOAD_ADDR 0x4001000
// #define LOAD_ADDR 0x4380000

/*
 * Header prepended to each a.out file.
 */
struct exec {
	unsigned short  a_machtype;     /* machine type */
	unsigned short  a_magic;        /* magic number */
	unsigned int	a_text;		/* size of text segment */
	unsigned int	a_data;		/* size of initialized data */
	unsigned int	a_bss;		/* size of uninitialized data */
	unsigned int	a_syms;		/* size of symbol table */
	unsigned int	a_entry;	/* entry point */
	unsigned int	a_trsize;	/* size of text relocation */
	unsigned int	a_drsize;	/* size of data relocation */
};

#define	OMAGIC	0x0701		/* old impure format */

unsigned long get_file_size(int fd) {
	struct stat statbuf;
	int ret = fstat(fd, &statbuf);
	if (ret != 0 || (statbuf.st_mode & S_IFMT) != S_IFREG)
		return 0;
	return statbuf.st_size;
}

int main(int argc, char**argv) {
	if (argc<2) {
		printf("Error: Missing binary file parameter\n");
		printf("Usage:\n");
		printf("\t%s bare.bin output.aout [entrypoint address]\n", argv[0]);
		exit(1);
	}

	int fd_binary = open(argv[1], O_RDONLY);
	unsigned int text_size = get_file_size(fd_binary);
	if (text_size == 0) {
		printf("Error: Empty or missing binary file\n");
		exit(2);
	}

	unsigned short machine_type = 0x8700;
	unsigned int data_size = 0;
	// FIXME: should be 0x4000000 (first DIMM slot address) + the kernel image offset in the ld linker script.
	// Get it from the objdump? or from the linker script?
	// m68k-linux-gnu-objdump -D vmlinux|grep "<_stext>:"|cut -f1 -d' '
	unsigned int entrypoint = LOAD_ADDR;
	if (argc == 4) {
		entrypoint = (unsigned int)strtol(argv[3], NULL, 0);
	}

	printf("Entrypoint Address is 0x%x\n", entrypoint);

	struct exec aout_header = {
		.a_machtype = machine_type,
		.a_magic = OMAGIC,
		.a_text = htonl(text_size),
		.a_data = htonl(data_size),
		.a_entry = htonl(entrypoint)
	};

	if (argc >= 3) {
		int fd_aout = open(argv[2], O_CREAT|O_TRUNC|O_WRONLY|O_DSYNC, 0644);
		if (fd_aout < 0) {
			printf("Error: Unable to open output file for writing\n");
			exit(3);
		}
		int whbytes = write(fd_aout, &aout_header, sizeof(struct exec));
		if (whbytes != sizeof(struct exec)) {
			printf("Error: Unable to write a.out header to output file\n");
			exit(4);
		}
		printf("Info: Wrote header: %d bytes\n", whbytes);
		int bytes_written = 0;
		// FIXME: write all bytes at once. This is slow as hell.
		while (bytes_written < text_size) {
			char buf[512];
			int rbytes = read(fd_binary, &buf, 512);
			if (rbytes<=0) {
				printf("Warning: read() returned no data.\n");
				break;
			}
			int wbytes = write(fd_aout, &buf, rbytes);
			if (wbytes <= 0) {
				printf("Warning: write() returned no data.\n");
				break;
			}
			bytes_written += wbytes;
		}
		printf("Info: Wrote binary: %d bytes\n", bytes_written);
		int total_size = sizeof(struct exec) + bytes_written; // text_size+data_size; //
		#define MIN_SIZE 512
		if (total_size<MIN_SIZE) {
			printf("Info: Minimum size is %d bytes. Adding padding.\n", MIN_SIZE);
			// FIXME: write all bytes at once
			while (total_size < MIN_SIZE) {
				int pbytes = write(fd_aout, "\0", 1);
				total_size += pbytes;
				printf(".");
			}
		}
		close(fd_aout);
	} else {
		printf("Error: Missing output file\n");
	}
	close(fd_binary);
	return 0;
}
