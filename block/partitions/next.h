// SPDX-License-Identifier: GPL-2.0

// fs/partitions/next.h
// 2022-11-07 Pedro Ramalhais <ramalhais@gmail.com>

#define	NEXT_LABEL_MAXPARTITIONS	8
#define	NEXT_LABEL_CPULBLLEN		24
#define	NEXT_LABEL_MAXDNMLEN		24
#define	NEXT_LABEL_MAXTYPLEN		24
#define	NEXT_LABEL_MAXBFLEN		24
#define	NEXT_LABEL_MAXHNLEN		32
#define	NEXT_LABEL_MAXMPTLEN		16
#define	NEXT_LABEL_MAXFSTLEN		8
#define	NEXT_LABEL_NBADBLOCKS		1670

struct __attribute__ ((packed)) next_partition {
	__be32	offset;	// starting sector
	__be32	size;	// number of sectors in partition
	__be16	bsize;	// block size in bytes
	__be16	fsize;	// filesystem basic fragment size
	char	opt;	// optimization type: 's'pace/'t'ime
	char	pad1;
	__be16	cpg;		// filesystem cylinders per group
	__be16	density;	// bytes per inode density
	int8_t	minfree;	// minfree (%)
	int8_t	newfs;		// run newfs during init
	char	mountpt[NEXT_LABEL_MAXMPTLEN]; // default/standard mount point
	int8_t	automnt;	// auto-mount when inserted
	char	type[NEXT_LABEL_MAXFSTLEN]; // file system type name
	char	pad2;
};

struct __attribute__ ((packed)) next_disklabel {
	__be32	version;
	__be32	block;	// this disklabel's start block number (there seem to be more disklabel copies)
	__be32	size;	// size of data (sectors)
	char	label[NEXT_LABEL_CPULBLLEN];
	__be32	flags;
	__be32	tag;
	char	name[NEXT_LABEL_MAXDNMLEN]; // SCSI drive name (written at format time?)
	char	type[NEXT_LABEL_MAXTYPLEN]; // drive type (written at format time?). ex: fixed_rw_scsi
	__be32	secsize;	// bytes per sector
	__be32	ntracks;	// tracks per cylinder
	__be32	nsectors;	// sectors per track
	__be32	ncylinders;	// cylinders
	__be32	rpm;		// RPM speed
	__be16	front;		// offset sectors for start of data
	__be16	back;		// offset sectors for end?? of data
	__be16	ngroups;	// alt groups??
	__be16	ag_size;	// alt group size in sectors
	__be16	ag_alts;	// alt group alternate sectors
	__be16	ag_off;		// alt group offset to first alternate in sectors
	__be32	boot_blkno[2];	// boot programs' offset in sectors
	char	kernel[NEXT_LABEL_MAXBFLEN];	// default kernel to load? ex: sdmach
	char	hostname[NEXT_LABEL_MAXHNLEN];	// host name (written at format time?). ex: localhost

	char	rootpartition;	// root partition letter. ex: a
	char	rwpartition;	// r/w partition letter. ex: b
	struct next_partition partitions[NEXT_LABEL_MAXPARTITIONS];
	union {
		__be16	v3_checksum; // disklabel version 3 checksum
		__be32	badblocks[NEXT_LABEL_NBADBLOCKS]; // block number that is bad
	} un;
	__be16	checksum; // label version 1 or 2 checksum
};

#define	NEXT_LABEL_checksum	checksum
#define	NEXT_LABEL_v3_checksum	un.v3_checksum
#define	NEXT_LABEL_badblocks	un.badblocks

#define	NEXT_LABEL_SECTOR		0		// disklabel sector
#define	NEXT_LABEL_OFFSET		0		// disklabel offset in sector
#define	NEXT_LABEL_SIZE			8192
#define	NEXT_LABEL_V1			0x4e655854	// NeXT
#define	NEXT_LABEL_V2			0x646c5632	// dlV2
#define	NEXT_LABEL_V3			0x646c5633	// dlV3
#define	NEXT_LABEL_DEFAULTFRONTPORCH	(160 * 2)	// Not needed. Info should be in label.front
#define	NEXT_LABEL_DEFAULTBOOT0_1	(32 * 2)	// Not needed. Info should be in label.boot_blkno[0]
#define	NEXT_LABEL_DEFAULTBOOT0_2	(96 * 2)	// Not needed. Info should be in label.boot_blkno[1]

#define NEXT_LABEL_BOOTSIZE_K	64
