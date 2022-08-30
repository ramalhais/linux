/*
** asm/nexthw.h -- This header defines NeXT specific macros and defines
**                    for playing with NeXT hardware.
**
** Copyright 1998 by Zach Brown <zab@zabbo.net>
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
*/

#ifndef _ASM_NEXTHW_H_
#define _ASM_NEXTHW_H_

#include <linux/types.h>

/* this needs to deal with the bmap on 
	non turbo 040s.. */

/* this is set up by head.S */
#define NEXT_IO_BASE (0xff000000)

#define NEXT_BMAP 0x100000

#define NEXT_SCR2_BASE (NEXT_IO_BASE+0x00d000)

#define NEXT_TIMER_BASE (NEXT_IO_BASE+NEXT_BMAP+0x016000)

#define NEXT_SCSI_BASE (NEXT_IO_BASE+NEXT_BMAP+0x014000)

#define NEXT_ETHER_BASE (NEXT_IO_BASE+NEXT_BMAP+0x006000)
#define NEXT_ETHER_RXDMA_BASE (NEXT_IO_BASE+0x000150)
#define NEXT_ETHER_TXDMA_BASE (NEXT_IO_BASE+0x000110)

#define NEXT_MON_BASE (NEXT_IO_BASE+0x00e000)

	/* magical scsi register */

#define NSCSI_RESET		0x02	/* ? */
#define NSCSI_INTMASK		0x20	/* pass the 90a int pin to the int chip */

#define _sctl_reg ((volatile __u8 *)(NEXT_SCSI_BASE+0x20))
#define write_sctl(x) *(_sctl_reg)=x

	/* common dma csr bits */

/* status bits for reading */
#define DMA_BUSERR	0x10000000
#define DMA_CINT	0x08000000
#define DMA_ENABLED	0x01000000

/* control bits for writing */
#define DMA_INITDMA	0x00200000
#define DMA_RESET	0x00100000
#define DMA_CLEARCHAINI	0x00080000
#define DMA_SETTMEM	0x00040000
#define DMA_SETCHAIN	0x00020000
#define DMA_SETENABLE	0x00010000

/* this is copied from memory that is initialized by the next prom at boot */

#define NUMSIMMS 4

struct prom_info {
	__u8	simm_something[NUMSIMMS];
	__u8	prom_flags;
	__u32	stuff[4];

	struct nvram_settings {
		__u32	flags;
		__u8	eaddr[6];  /* and passwd !? */
		__u16	simm_something;  /* 4 bits / simm */
		__u8	whoknows[2];
		__u8	bigmystery[3];
		__u8	moreflags;
	
		__u8	bootcmdline[12];
		__u16	checksum;
	} nvram;

	__u8 inetntoa[18],inputline[128];  /* ??? */
	struct  mem_layout {
        	__u32 start,end;
	} simm_info[NUMSIMMS];

	__u32	base_ptr,brk_ptr;
	__u32	bootdev_ptr,bootarg_ptr,bootinfo_ptr,bootfile_ptr;
	__u8	bootfile[64];
/*enum*/	__u32	boot_mode;
	__u8	km_mon_stuff[4+4+2+2+2+2+2+2+4+4+4+2+4+3*2+2];
	__u32	moninit; /* ? */
	__u32	sio_ptr;
	__u32	time;
	__u32	sddp_ptr,dgp_ptr,s5cp_ptr,odc_ptr,odd_ptr;
	__u8	radix;  /* ? */

	__u32	dmachip,diskchip;
	__u32	stat_ptr,mask_ptr;

	__u32	nofault_func;
	__u8	fmt;
	__u32	addr[8],na;  /* ??? */
	__u32	mousex,mousey;
	__u32	cursor[2][32];

	__u32	getc_func,try_getc_func,putc_func,
		alert_func,alert_conf_func,alloc_func,
		boot_slider_func;
	__u32	event_ptr;
	
	__u32	event_high; /* ? */
	
	/* the port is not complete until we have a boot loader
		that uses this to have a dancing penguin! */
	struct anim {
		__u16	x,y;
		__u32	icon_ptr;
		__u8	next;
		__u8	time;
	} anim;
	__u32	anim_ptr;
	__u32	anim_time;

	__u32	scsi_intr_func;		/* void () */
	__u32	scsi_intr_arg;

	__u16	minor,seq; /* ? */

	__u32	anim_run_func;		/* int () */
	
	__u16	major;

	__u32	client_ether_addr_ptr;
	
	__u32	cons_i,cons_o;

	__u32	test_msg_ptr;

	struct nextfb_prominfo {
		uint32_t        pixels_pword;
		uint32_t        line_length;  /* bytes / scan */
		uint32_t        vispixx;  /* pixels displayed */
		uint32_t        realpixx;  /* real pixel width  */
		uint32_t        height;
		uint32_t        flags;
		uint32_t        whitebits[4];
		/* not sure what these do */
		uint8_t         slot;
		uint8_t         fbnum;
		uint8_t         laneid; /* ?? */
		uint32_t        before_code; /* ?? */
		uint32_t        after_code; /* ?? */
		struct nextfb_businfo {
			uint32_t        phys;
			uint32_t        virt;
			uint32_t        len;
		} frames[6];  /* [1] == real */
		uint32_t        stack; /* ?? */
	} fbinfo;

	__u32	fdgp_ptr;  /* ? */
	__u8	mach_type,next_board_rev;

	__u32	as_tune_func;	/* int () */
	__u32	moreflags;
};      

extern struct prom_info prom_info;

struct eprom_info {
	unsigned char eaddr[6];
};
extern struct eprom_info eprom_info;

#endif /* asm/nexthw.h */
