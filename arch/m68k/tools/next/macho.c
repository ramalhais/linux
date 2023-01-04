/*
 *  linux/arch/m68k/tools/next/macho.c
 *
 *  Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
 *
 *  Add Mach-O header to kernel binary to boot on the NeXT.
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

struct mach_header {
        uint32_t	magic;          /* mach magic number identifier */
        uint32_t	cputype;        /* cpu specifier */
        uint32_t	cpusubtype;     /* machine specifier */
        uint32_t	filetype;       /* type of file */
        uint32_t	ncmds;          /* number of load commands */
        uint32_t	sizeofcmds;     /* the size of all the load commands */
        uint32_t	flags;          /* flags */
};

#define MH_MAGIC        0xfeedface      /* the mach magic number */

#define CPU_TYPE_MC680x0	6
#define CPU_SUBTYPE_MC68030	1
#define CPU_SUBTYPE_MC68040	2

#define MH_OBJECT       0x1             /* relocatable object file */
#define MH_EXECUTE      0x2             /* demand paged executable file */
#define MH_FVMLIB       0x3             /* fixed VM shared library file */
#define MH_CORE         0x4             /* core file */
#define MH_PRELOAD      0x5             /* preloaded executable file */

#define MH_NOUNDEFS     0x1             /* the object file has no undefined
                                           references, can be executed */
#define MH_INCRLINK     0x2             /* the object file is the output of an
                                           incremental link against a base file
                                           and can't be link edited again */

struct load_command {
        uint32_t	cmd;              /* type of load command */
        uint32_t	cmdsize;          /* total size of command in bytes */
};

/* Constants for the cmd field of all load commands, the type */
#define LC_SEGMENT      0x1     /* segment of this file to be mapped */
#define LC_SYMTAB       0x2     /* link-edit stab symbol table info */
#define LC_SYMSEG       0x3     /* link-edit gdb symbol table info (obsolete) */
#define LC_THREAD       0x4     /* thread */
#define LC_UNIXTHREAD   0x5     /* unix thread (includes a stack) */
#define LC_LOADFVMLIB   0x6     /* load a specified fixed VM shared library */
#define LC_IDFVMLIB     0x7     /* fixed VM shared library identification */
#define LC_IDENT        0x8     /* object identification information(obsolete)*/

struct segment_command {
        uint32_t	cmd;            /* LC_SEGMENT */
        uint32_t	cmdsize;        /* includes sizeof section structures */
        char            segname[16];    /* segment name */
        uint32_t	vmaddr;         /* memory address of this segment */
        uint32_t	vmsize;         /* memory size of this segment */
        uint32_t	fileoff;        /* file offset of this segment */
        uint32_t        filesize;       /* amount to map from the file */
        uint32_t	maxprot;        /* maximum VM protection */
        uint32_t	initprot;       /* initial VM protection */
        uint32_t	nsects;         /* number of sections in segment */
        uint32_t	flags;          /* flags */
};

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
		printf("\t%s bare.bin [output.macho] [entrypoint address]\n", argv[0]);
		exit(1);
	}

	int fd_binary = open(argv[1], O_RDONLY);
	unsigned int text_size=get_file_size(fd_binary);
	if (text_size == 0) {
		printf("Error: Empty or missing binary file\n");
		exit(2);
	}

	struct segment_command sc2 = {
		.cmd = htonl(0),
		.cmdsize = htonl(sizeof(struct segment_command))
	};

	unsigned int entrypoint = LOAD_ADDR;
	if (argc == 4) {
		entrypoint = (unsigned int)strtol(argv[3], NULL, 0);
	}

	struct segment_command sc1 = {
		.cmd = htonl(LC_SEGMENT),
		.cmdsize = htonl(sizeof(struct segment_command)),
		.vmaddr = htonl(entrypoint),
		.fileoff = htonl(sizeof(struct mach_header) + sizeof(sc1) + sizeof(sc2)), // offset from start of payload (mach-o header)
		.filesize = htonl(text_size)
	};
	struct mach_header mh = {
		.magic = htonl(MH_MAGIC),
		.cputype = htonl(CPU_TYPE_MC680x0),
		.cpusubtype = htonl(CPU_SUBTYPE_MC68030),
		.filetype = htonl(MH_PRELOAD),
		.ncmds = htonl(2),
		.sizeofcmds = htonl(sizeof(sc1) + sizeof(sc2)),
		.flags = htonl(MH_NOUNDEFS)
	};

	printf("Entrypoint Address is 0x%x\n", entrypoint);
	if (argc >= 3) {
		int fd_aout = open(argv[2], O_CREAT|O_TRUNC|O_WRONLY|O_DSYNC, 0644);
		if (fd_aout < 0) {
			printf("Error: Unable to open output file for writing\n");
			exit(3);
		}

		int whbytes = write(fd_aout, &mh, sizeof(mh));
		if (whbytes != sizeof(mh)) {
			printf("Error: Unable to write mach-o header to output file\n");
			exit(4);
		}
		printf("Info: Wrote mach-o header: %d bytes\n", whbytes);

		whbytes = write(fd_aout, &sc1, sizeof(sc1));
		if (whbytes != sizeof(sc1)) {
			printf("Error: Unable to write mach-o sc1 header to output file\n");
			exit(4);
		}
		printf("Info: Wrote mach-o sc1 header: %d bytes\n", whbytes);

		whbytes = write(fd_aout, &sc2, sizeof(sc2));
		if (whbytes != sizeof(sc2)) {
			printf("Error: Unable to write mach-o sc2 header to output file\n");
			exit(4);
		}
		printf("Info: Wrote mach-o sc2 header: %d bytes\n", whbytes);

		int bytes_written = 0;
		// FIXME: write all bytes at once. This is slow as hell.
		while (bytes_written < text_size) {
			char buf[512];
			int rbytes = read(fd_binary, &buf, 512);
			if (rbytes <= 0) {
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
/*
		int total_size = sizeof(mh) + sizeof(sc1) + sizeof(sc2) + bytes_written; // text_size+data_size; //
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
*/
		close(fd_aout);
	}
	close(fd_binary);
	return 0;
}
