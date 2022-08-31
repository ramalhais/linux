

struct mach_header {
	unsigned long	magic;
	unsigned long	cputype;
	unsigned long	cpusubtype;
	unsigned long	filetype;
	unsigned long	ncmds;
	unsigned long	sizeofcmds;
	unsigned long	flags;
};


/* magic */
#define	MH_MAGIC	0xfeedface

/* cputype */
#define CPU_TYPE_MC680x0	6

/* cpusubtype */
#define CPU_TYPE_MC68030	1
#define CPU_TYPE_MC68040	2

/* filetype */
#define MH_OBJECT	0x1	/* relocateable object file */
#define MH_EXECUTE	0x2	/* executable object file */
#define	MH_FVMLIB	0x3	/* fixed vm shared library file */
#define	MH_CORE		0x4	/* core file */
#define MH_PRELOAD	0x5	/* preloaded executable file */

/* ncmds is the total number of load_command's in this Mach-O file */

/* sizeofcmds is the total size in bytes of all commands */

/* flags */
#define MH_NOUNDEFS	0x1	/* no undefined references (can be executed) */
#define MH_INCRLINK	0x2	/* is incremental link output, can't be link edited */
