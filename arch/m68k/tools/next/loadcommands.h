
/* the bare minimum load command structure */
struct load_command {
	unsigned long	cmd;	/* type of command */
	unsigned long	cmdsize; /* total size of command in bytes */
};

/* cmd values */
#define	LC_SEGMENT	0x1	/* file segment to be mapped */
#define LC_SYMTAB	0x2	/* link-edit stab table symtable info (obsolete) */
#define LC_SYMSEG	0x3	/* link-edit gb symtable info */
#define	LC_THREAD	0x4	/* thread */
#define	LC_UNIXTHREAD	0x5	/* UNIX thread (includes a stack) */
#define LC_LOADFVMLIB	0x6	/* load a fixed VM shared library */
#define LC_IDFVMLIB	0x7	/* fixed VM shared library id */
#define	LC_IDENT	0x8	/* object id info (obsolete) */

/* lc_str, used to make variable length strings in load_commands */
union lc_str {
	unsigned long	offset;
	char		*ptr;
};

/* LC_SEGMENT load command structure */
struct segment_command {
	unsigned long	cmd;
	unsigned long	cmdsize; /* includes size of section structures */
	char		segname[16];	/* segment's name */
	unsigned long	vmaddr;		/* seg's memory addr */
	unsigned long	vmsize;		/* seg's memory size */
	unsigned long	fileoff;	/* seg's file offset */
	unsigned long	filesize;	/* amount to map from file */
	unsigned long	maxprot;	/* maximum VM protection */
	unsigned long	initprot;	/* initial VM protection */
	unsigned long	nsects;		/* number of sections */
	unsigned long	flags;	/* flags */
};

/* VM protection values: */
#define VM_PROT_NONE	0x00
#define VM_PROT_READ	0x01
#define VM_PROT_WRITE	0x02
#define VM_PROT_EXECUTE	0x04
#define VM_PROT_DEFAULT \
	(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)
#define VM_PROT_ALL \
	(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)

/* LC_SEGMENT flags */
#define SG_HIGHVM	0x1
#define	SG_FVMLIB	0x2
#define SG_NORELOC	0x3

/* struct section is a section as used in an LC_SEGMENT command */
struct section {
	char		sectname[16];
	char		segname[16];
	unsigned long	addr;
	unsigned long	size;
	unsigned long	offset; /* file offset */
	unsigned long	align;  /* segments should be page-aligned (8K ???) */
	unsigned long	reloff; /* file offset of relocs */
	unsigned long	nreloc;
	unsigned long	flags;
	unsigned long	reserved1;
	unsigned long	reserved2;
};

/* flags for section */
#define S_ZEROFILL	0x1
#define S_CSTRING_LITERALS	0x2
#define S_WRITE_LITERALS	0x2
#define S_4BYTE_LITERALS	0x2
#define S_8BYTE_LITERALS	0x2
#define S_LITERAL_POINTERS	0x2

/* segment and section names */
#define	SEG_PAGEZERO	"__PAGEZERO"
#define SEG_TEXT	"__TEXT"
#define SECT_TEXT	"__text"
#define SECT_FVMLIB_INIT0	"__fvmlib_init0"
#define SECT_FVMLIB_INIT1	"__fvmlib_init1"
#define SEG_DATA	"__DATA"
#define SECT_DATA	"__data"
#define SECT_BSS	"__bss"
#define SECT_COMMON	"__common"
#define SEG_OBJC	"__OBJC"
#define SECT_OBJC_SYMBOLS	"__symbol_table"
#define SECT_OBJC_MODULES	"__module_info"
#define SECT_OBJC_STRINGS	"__selector_strs"
#define SEG_ICON	"__ICON"
#define SECT_ICON_HEADER	"__header"
#define SECT_ICON_TIFF	"__tiff"

/* LC_SYMTAB load command */
struct symtab_command {
	unsigned long	cmd;
	unsigned long	cmdsize; /* sizeof(symtab_command) */
	unsigned long	symoff;
	unsigned long	nsyms;
	unsigned long	stroff;
	unsigned long	strsize;
};

struct nlist {
	union {
		char	*n_name;
		long	n_strx;
	} n_un;
	unsigned char	n_type; /* type flag */
	unsigned char	n_sect; /* section number or NO_SECT */
	unsigned char	n_desc; /* ?????????? stab.h tells more */
	unsigned char	n_value; /* value of this entry */
};

/* the n_type flag -- this is fucking absurdly complicated */

/* masks for the three parts of the n_type (the bitfields are (3, 4, 1) */

#define N_STAB	0xe0  /* if any of these are set, this is a symoblic debugging entry */
#define N_TYPE	0x1e	/* type bits */
#define N_EXT	0x01	/* external sym? */

/* n_type values */
#define N_UNDEF	0x0	/* undefined, n_sect == NO_SECT */
#define N_ABS	0x2	/* absolute, n_sect == NO_SECT */
#define N_SECT	0xe	/* edfined in section n_sect */
#define N_INDR	0xa	/* indirect */


/* for n_sect */
#define NO_SECT	0
#define MAX_SECT 255
