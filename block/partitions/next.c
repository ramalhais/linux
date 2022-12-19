// SPDX-License-Identifier: GPL-2.0
/*
 *  block/partitions/next.c
 *
 *  based on ultrix.c
 *
 *  2022-11-07 Pedro Ramalhais <ramalhais@gmail.com>
 */

#include "check.h"
#include "next.h"

#define LINUX_BLOCK_SIZE 512

int next_partition(struct parsed_partitions *state)
{
	Sector sect;
	unsigned char *data;
	struct next_disklabel *label;
	u32 version;
	u32 bytes_per_sector;
	u32 front_porch;

	data = read_part_sector(state, NEXT_LABEL_SECTOR, &sect);
	if (!data)
		return -1;

	label = (struct next_disklabel *)data;
	version = be32_to_cpu(label->version);
	bytes_per_sector = be32_to_cpu(label->secsize);
	front_porch = be32_to_cpu(label->front);

	if (version != NEXT_LABEL_V3 &&
	    version != NEXT_LABEL_V2 &&
	    version != NEXT_LABEL_V1) {
		put_dev_sector(sect);
		return 0;
	}

	pr_info("Found NextStep disklabel: Label:%s, Version:%c%c%c%c, Bytes per Sector:%d",
		label->label,
		version>>24&0xff, version>>16&0xff, version>>8&0xff, version&0xff,
		bytes_per_sector);

	for (int pn = 0; pn < NEXT_LABEL_MAXPARTITIONS; pn++) {
		struct next_partition part;
		u32 offset;
		u32 size;

		part = label->partitions[pn];
		offset = be32_to_cpu(part.offset);
		if (offset == -1)
			continue;

		size = be32_to_cpu(part.size);

		pr_info("Found NextStep partition [%d]: Offset:%ub(%us) Size:%ub(%us) Mount:%s, Filesystem: %s", pn,  (front_porch+offset)*bytes_per_sector, front_porch+offset, size*bytes_per_sector, size, part.mountpt, part.type);

		put_partition(state, pn+1, (front_porch+offset)*bytes_per_sector/LINUX_BLOCK_SIZE, size*bytes_per_sector/LINUX_BLOCK_SIZE);
	}

	put_dev_sector(sect);
	strlcat(state->pp_buf, "\n", PAGE_SIZE);
	return 1;
}
