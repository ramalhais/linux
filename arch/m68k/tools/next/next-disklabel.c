// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define	NEXT68K_LABEL_MAXPARTITIONS	8	/* number of partitions in next68k_disklabel */
#define	NEXT68K_LABEL_CPULBLLEN		24
#define	NEXT68K_LABEL_MAXDNMLEN		24
#define	NEXT68K_LABEL_MAXTYPLEN		24
#define	NEXT68K_LABEL_MAXBFLEN		24
#define	NEXT68K_LABEL_MAXHNLEN		32
#define	NEXT68K_LABEL_MAXMPTLEN		16
#define	NEXT68K_LABEL_MAXFSTLEN		8
#define	NEXT68K_LABEL_NBAD		1670	/* sized to make label ~= 8KB */

struct __attribute__((packed)) next68k_partition {
	int32_t	cp_offset;		/* starting sector */
	int32_t	cp_size;		/* number of sectors in partition */
	int16_t	cp_bsize;		/* block size in bytes */
	int16_t	cp_fsize;		/* filesystem basic fragment size */
	char	cp_opt;			/* optimization type: 's'pace/'t'ime */
	char	cp_pad1;
	int16_t	cp_cpg;			/* filesystem cylinders per group */
	int16_t	cp_density;		/* bytes per inode density */
	int8_t	cp_minfree;		/* minfree (%) */
	int8_t	cp_newfs;		/* run newfs during init */
	char	cp_mountpt[NEXT68K_LABEL_MAXMPTLEN];
					/* default/standard mount point */
	int8_t	cp_automnt;		/* auto-mount when inserted */
	char	cp_type[NEXT68K_LABEL_MAXFSTLEN]; /* file system type name */
	char	cp_pad2;
};

/* The disklabel the way it is on the disk */
struct __attribute__((packed)) next68k_disklabel {
	int32_t	cd_version;		/* label version */
	int32_t	cd_label_blkno;		/* block # of this label */
	int32_t	cd_size;		/* size of media area (sectors) */
	char	cd_label[NEXT68K_LABEL_CPULBLLEN]; /* disk name (label) */
	uint32_t cd_flags;		/* flags */
	uint32_t cd_tag;		/* volume tag */
	char	cd_name[NEXT68K_LABEL_MAXDNMLEN]; /* drive (hardware) name */
	char	cd_type[NEXT68K_LABEL_MAXTYPLEN]; /* drive type */
	int32_t	cd_secsize;		/* # of bytes per sector */
	int32_t	cd_ntracks;		/* # of tracks per cylinder */
	int32_t	cd_nsectors;		/* # of data sectors per track */
	int32_t	cd_ncylinders;		/* # of data cylinders per unit */
	int32_t	cd_rpm;			/* rotational speed */
	int16_t	cd_front;		/* # of sectors in "front porch" */
	int16_t	cd_back;		/* # of sectors in "back porch" */
	int16_t	cd_ngroups;		/* # of alt groups */
	int16_t	cd_ag_size;		/* alt group size (sectors) */
	int16_t	cd_ag_alts;		/* alternate sectors / alt group */
	int16_t	cd_ag_off;		/* sector offset to first alternate */
	int32_t	cd_boot_blkno[2];	/* boot program locations */
	char	cd_kernel[NEXT68K_LABEL_MAXBFLEN]; /* default kernel name */
	char	cd_hostname[NEXT68K_LABEL_MAXHNLEN];
				/* host name (usu. where disk was labeled) */
	char	cd_rootpartition;	/* root partition letter e.g. 'a' */
	char	cd_rwpartition;		/* r/w partition letter e.g. 'b' */
	struct next68k_partition cd_partitions[NEXT68K_LABEL_MAXPARTITIONS];

	union {
		uint16_t CD_v3_checksum; /* label version 3 checksum */
		int32_t	CD_bad[NEXT68K_LABEL_NBAD];
					/* block number that is bad */
	} cd_un;
	uint16_t cd_checksum;		/* label version 1 or 2 checksum */
};

#define	NEXT68K_LABEL_cd_checksum	cd_checksum
#define	NEXT68K_LABEL_cd_v3_checksum	cd_un.CD_v3_checksum
#define	NEXT68K_LABEL_cd_bad		cd_un.CD_bad

#define	NEXT68K_LABEL_SECTOR		0	/* sector containing label */
#define	NEXT68K_LABEL_OFFSET		0	/* offset of label in sector */
#define	NEXT68K_LABEL_SIZE		8192	/* size of label */
#define	NEXT68K_LABEL_CD_V1		0x4e655854 /* version #1: "NeXT" */
#define	NEXT68K_LABEL_CD_V2		0x646c5632 /* version #2: "dlV2" */
#define	NEXT68K_LABEL_CD_V3		0x646c5633 /* version #3: "dlV3" */
#define	NEXT68K_LABEL_DEFAULTFRONTPORCH	(160 * 2) // 160*2*512byte based. it's *2 to match 1024 blocksize. a bit crazy.
#define	NEXT68K_LABEL_DEFAULTBOOT0_1	(32 * 2)
#define	NEXT68K_LABEL_DEFAULTBOOT0_2	(96 * 2)

void print_part(struct next68k_partition *part)
{
	printf("cp_offset\t\t0x%x (%d)\n",	ntohl(part->cp_offset), ntohl(part->cp_offset));
	printf("cp_size\t\t0x%x (%d)\n",	ntohl(part->cp_size), ntohl(part->cp_size));
	printf("cp_bsize\t\t0x%x (%hd)\n",	ntohs(part->cp_bsize), ntohs(part->cp_bsize));
	printf("cp_fsize\t\t0x%x (%hd)\n",	ntohs(part->cp_fsize), ntohs(part->cp_fsize));
	printf("cp_opt\t\t%c\n",		part->cp_opt);
	printf("cp_pad1\t\t%c\n",		part->cp_pad1);
	printf("cp_cpg\t\t0x%x (%hd)\n",	ntohs(part->cp_cpg), ntohs(part->cp_cpg));
	printf("cp_density\t\t0x%x (%hd)\n",	ntohs(part->cp_density), ntohs(part->cp_density));
	printf("cp_minfree\t\t0x%x (%hhd)\n",	part->cp_minfree, part->cp_minfree);
	printf("cp_newfs\t\t0x%x (%hhd)\n",	part->cp_newfs, part->cp_newfs);
	printf("cp_mountpt\t\t%s\n",		part->cp_mountpt);
	printf("cp_automnt\t\t0x%x (%hhd)\n",	part->cp_automnt, part->cp_automnt);
	printf("cp_type\t\t%s\n",		part->cp_type);
	printf("cp_pad2\t\t%c\n",		part->cp_pad2);
}

void print_dl(struct next68k_disklabel *dl)
{
	printf("cd_version\t\t0x%x (%d)\t%c%c%c%c\n",	ntohl(dl->cd_version), ntohl(dl->cd_version), dl->cd_version&0xff, dl->cd_version>>8&0xff, dl->cd_version>>16&0xff, dl->cd_version>>24&0xff);
	printf("cd_label_blkno\t\t0x%x (%d)\n",		ntohl(dl->cd_label_blkno), ntohl(dl->cd_label_blkno));
	printf("cd_size\t\t0x%x (%d)\n",		ntohl(dl->cd_size), ntohl(dl->cd_size));
	printf("cd_label\t\t%s\n",			dl->cd_label);
	printf("cd_flags\t\t0x%x (%u)\n",		ntohl(dl->cd_flags), ntohl(dl->cd_flags));
	printf("cd_tag\t\t0x%x (%u)\n",			ntohl(dl->cd_tag), ntohl(dl->cd_tag));
	printf("cd_name\t\t%s\n",			dl->cd_name);
	printf("cd_type\t\t%s\n",			dl->cd_type);
	printf("cd_secsize\t\t0x%x (%d)\n",		ntohl(dl->cd_secsize), ntohl(dl->cd_secsize));
	printf("cd_ntracks\t\t0x%x (%d)\n",		ntohl(dl->cd_ntracks), ntohl(dl->cd_ntracks));
	printf("cd_nsectors\t\t0x%x (%d)\n",		ntohl(dl->cd_nsectors), ntohl(dl->cd_nsectors));
	printf("cd_ncylinders\t\t0x%x (%d)\n",		ntohl(dl->cd_ncylinders), ntohl(dl->cd_ncylinders));
	printf("cd_rpm\t\t0x%x (%d)\n",			ntohl(dl->cd_rpm), ntohl(dl->cd_rpm));
	printf("cd_front\t\t0x%x (%hd)\n",		ntohs(dl->cd_front), ntohs(dl->cd_front));
	printf("cd_back\t\t0x%x (%hd)\n",		ntohs(dl->cd_back), ntohs(dl->cd_back));
	printf("cd_ngroups\t\t0x%x (%hd)\n",		ntohs(dl->cd_ngroups), ntohs(dl->cd_ngroups));
	printf("cd_ag_size\t\t0x%x (%hd)\n",		ntohs(dl->cd_ag_size), ntohs(dl->cd_ag_size));
	printf("cd_ag_alts\t\t0x%x (%hd)\n",		ntohs(dl->cd_ag_alts), ntohs(dl->cd_ag_alts));
	printf("cd_ag_off\t\t0x%x (%hd)\n",		ntohs(dl->cd_ag_off), ntohs(dl->cd_ag_off));
	printf("cd_boot_blkno[0]\t\t0x%x (%d)\n",	ntohl(dl->cd_boot_blkno[0]), ntohl(dl->cd_boot_blkno[0]));
	printf("cd_boot_blkno[1]\t\t0x%x (%d)\n",	ntohl(dl->cd_boot_blkno[1]), ntohl(dl->cd_boot_blkno[1]));
	printf("cd_kernel\t\t%s\n",			dl->cd_kernel);
	printf("cd_hostname\t\t%s\n",			dl->cd_hostname);
	printf("cd_rootpartition\t\t%c\n",		dl->cd_rootpartition);
	printf("cd_rwpartition\t\t%c\n",		dl->cd_rwpartition);
	printf("\n");
	printf("CD_v3_checksum\t\t0x%x (%hu)\n",	ntohs(dl->cd_un.CD_v3_checksum), ntohs(dl->cd_un.CD_v3_checksum));
	printf("cd_checksum\t\t0x%x (%hu)\n",		ntohs(dl->cd_checksum), ntohs(dl->cd_checksum));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
static uint16_t checksum(struct next68k_disklabel *dl, bool validate)
{
	uint16_t *buf = (uint16_t *)dl;
	uint16_t *csump;
	uint32_t sum = 0;

	if (ntohl(dl->cd_version) == NEXT68K_LABEL_CD_V3)
		csump = &dl->cd_un.CD_v3_checksum;
	else
		csump = &dl->cd_checksum;

	while (buf < csump)
		sum += ntohs(*buf++);

	sum = (sum + (sum >> 16)) & 0xffff;

	if (validate)
		return ntohs(*csump) - sum;
	else
		return sum;
}
#pragma GCC diagnostic pop

int write_dl(FILE *disk) {
	return -1;
}

#define	MAX_BOOT_SECTORS	64
#define MAX_BOOT_FILES		2
int write_boot(FILE *diskf, char *boot) {
	FILE *bootf = fopen(boot, "r");
	char *buf;
	struct next68k_disklabel dl;
	uint32_t secsize;
	int boot_offset[MAX_BOOT_FILES];

	if (bootf == NULL) {
		printf("Error: Unable to read boot binary");
		return -1;
	}

	rewind(diskf);
	if (fread(&dl, sizeof(dl), 1, diskf) != 1) {
		printf("Error: Unable to read NeXT disklabel/boot");
		return -2;
	}

	secsize = ntohl(dl.cd_secsize);
	buf = malloc(secsize);
	int bytes = 0;
	int total_bytes = 0;

	for (int boot_nr = 0; boot_nr <= 1; boot_nr++) {
		boot_offset[boot_nr] = ntohl(dl.cd_boot_blkno[boot_nr]);
		if (boot_offset[boot_nr] <= 0) {
			printf("Error: missing boot sector pointer[%d]=%d\n", boot_nr, boot_offset[boot_nr]);
			return -3;
		}
		boot_offset[boot_nr] *= secsize;	// Convert offset to bytes
		printf("boot[%d] offset=0x%x (%d)\n", boot_nr, boot_offset[boot_nr], boot_offset[boot_nr]);
	}

	while (total_bytes < MAX_BOOT_SECTORS * secsize && (bytes = fread(buf, 1, secsize, bootf)) > 0) {
		for (int boot_nr = 0; boot_nr <= 1; boot_nr++) {
			fseek(diskf, boot_offset[boot_nr] + total_bytes, SEEK_SET);
			int wbytes = fwrite(buf, 1, bytes, diskf);
			if (bytes != wbytes) {
				printf("Error: Writing boot[%d]: Read bytes (%d) != (%d) Written bytes\n", boot_nr, bytes, wbytes);
				return -4;
			} else {
				printf("Wrote boot[%d]: bytes=%d offset=%d -> %ld\n", boot_nr, bytes, boot_offset[boot_nr] + total_bytes, ftell(diskf));
			}
		}
		total_bytes += bytes;
	}

	free(buf);
	return total_bytes;
}

int main(int argc, char **argv)
{
	if (argc <= 1 || !strcmp("--help",argv[1])) {
		printf("Usage:\n\t%s DISK_FILE [-c | -b bootfile]\n", argv[0]);
		printf("Example:\n\t%s /dev/sdc\n", argv[0]);
		exit(1);
	}

	struct next68k_disklabel dl;
	uint16_t csumv;
	char *disk = argv[1];
	FILE *diskf = fopen(disk, "r+"); // use w+ for create
	char *boot;

	if (diskf == NULL) {
		printf("Error: Unable to open disk %s\n", disk);
		exit(2);
	}

	if (argc > 2 && !strcmp("-c", argv[2])) {
		if (write_dl(diskf)) {
			printf("Error: Unable to write disklabel\n");
			exit(3);
		}
	} else if (argc > 3 && !strcmp("-b", argv[2])) {
		boot = argv[3];
		int ret = 0;
		if ((ret = write_boot(diskf, boot)) <= 0) {
			printf("Error: Unable to write boot blocks. ret=%d\n", ret);
			exit(4);
		} else {
			printf("Wrote %d bytes to boot blocks\n", ret);
		}
	} else {
		printf("No recognized options. Just print disklabel.\n");
	}

	printf("\n\nsizeof(struct next68k_disklabel)=%ld\n", sizeof(struct next68k_disklabel));

	rewind(diskf);
	if (fread(&dl, sizeof(dl), 1, diskf) != 1) {
		printf("Error: Unable to read NeXT disklabel\n");
		exit(5);
	}

	print_dl(&dl);


	csumv = checksum(&dl, true);
	if (csumv != 0) {
		printf("\nFailed Checksum: %d %d. Exiting.\n", csumv,checksum(&dl, false));
		exit(6);
	}

	for (int part = 0; part < NEXT68K_LABEL_MAXPARTITIONS; part++) {
		if (ntohl(dl.cd_partitions[part].cp_offset) == -1) {
			// printf("\n### Skipping Partition %d ###\n", part);
			// print_part(&dl.cd_partitions[part]);
			continue;
		}
		printf("\n### Partition %d ###\n", part);
		print_part(&dl.cd_partitions[part]);
	}

	printf("\n");
	printf("Run the following command to mount the first partition:\n");
	printf("\tLOOPDEV=$(sudo losetup -f | head -1); sudo losetup --offset=$((160*1024)) $LOOPDEV %s; sudo umount /mnt/bla; sudo mkdir -p /mnt/bla; sudo mount -t ufs -o ufstype=nextstep $LOOPDEV /mnt/bla\n", disk);
	printf("\nTo unmount and free the loop device:\n");
	printf("\tsudo umount /mnt/bla; sudo losetup -d $LOOPDEV\n");

	exit(0);
}
