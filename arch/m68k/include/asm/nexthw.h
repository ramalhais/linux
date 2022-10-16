// asm/nexthw.h
// This header defines NeXT specific macros and defines for playing with NeXT hardware.
// Copyright 1998 by Zach Brown <zab@zabbo.net>
// This file is subject to the terms and conditions of the GNU General Public
// License.  See the file COPYING in the main directory of this archive
// for more details.

#ifndef _ASM_NEXTHW_H_
#define _ASM_NEXTHW_H_

#include <linux/types.h>

//	PROM Info data from running Previous emulator:
//					mach_type	next_board_rev	dmachip	diskchip
//	NeXT Computer:			0x0		0x0		0x139	0x136
//	NeXTstation:			0x1		0x1		0x139	0x136
//	NeXTcube:			0x2		0x0		0x139	0x136
//	NeXTstation Color:		0x3		0x0		0x139	0x136
//	NeXTstation Turbo:		0x4		0xf		0x0	0xfe66
//	NeXTstation Turbo Color:	0x5		0xf		0x0	0x266
//	NeXTcube Turbo			0x8		0xf		0x0	0xfe66

enum NEXT_MACHINE_TYPE {
	NEXT_MACHINE_COMPUTER,
	NEXT_MACHINE_STATION,
	NEXT_MACHINE_CUBE,
	NEXT_MACHINE_STATION_COLOR,
	NEXT_MACHINE_STATION_TURBO,
	NEXT_MACHINE_STATION_TURBO_COLOR,
	NEXT_MACHINE_CUBE_TURBO=8
};
extern char *next_machine_names[];

/* this needs to deal with the bmap on non turbo 040s.. */

// MMU mapped in head.S to physical 0x2000.0000
#define NEXT_IO_BASE (0xff000000)

#define NEXT_SLOT 0x0

// #define NEXT_SLOT_BMAP 0x0	// FIXME NeXT: 030
// #define NEXT_SLOT_BMAP 0x100000	// 040
#define NEXT_SLOT_BMAP (prom_info.mach_type == NEXT_MACHINE_COMPUTER ? 0x0 : 0x100000)

#define NEXT_ETHER_RXDMA_BASE	(NEXT_IO_BASE+NEXT_SLOT+0x000150)
#define NEXT_ETHER_TXDMA_BASE	(NEXT_IO_BASE+NEXT_SLOT+0x000110)
#define NEXT_SCR1_BASE		(NEXT_IO_BASE+NEXT_SLOT+0x00c000)
#define NEXT_SCR2_BASE		(NEXT_IO_BASE+NEXT_SLOT+0x00d000)
#define NEXT_MON_BASE		(NEXT_IO_BASE+NEXT_SLOT+0x00e000)

#define NEXT_ETHER_BASE		(NEXT_IO_BASE+NEXT_SLOT_BMAP+0x006000)
#define NEXT_SCSI_BASE		(NEXT_IO_BASE+NEXT_SLOT_BMAP+0x014000)
#define NEXT_TIMER_BASE		(NEXT_IO_BASE+NEXT_SLOT_BMAP+0x016000)

// magical scsi register
#define NSCSI_RESET	0x02	// ?
#define NSCSI_INTMASK	0x20	// pass the 90a int pin to the int chip

#define _sctl_reg ((volatile __u8 *)(NEXT_SCSI_BASE+0x20))
#define write_sctl(x) *(_sctl_reg)=x

// common dma csr bits

// status bits for reading
#define DMA_BUSERR	0x10000000
#define DMA_CINT	0x08000000
#define DMA_ENABLED	0x01000000

// control bits for writing
#define DMA_SETTDEV		0x00000000 // DMA from memory to device
#define DMA_INITDMA		0x00200000
#define DMA_RESET		0x00100000
#define DMA_CLEARCHAINI		0x00080000
#define DMA_SETTMEM		0x00040000 // DMA from device to memory
#define DMA_SETCHAIN		0x00020000
#define DMA_SETENABLE		0x00010000
#define DMA_INITDMA_TURBO	0x00800000 // Turbo-only?

// this is copied from memory that is initialized by the next prom at boot

#define NUMSIMMS 4

struct prom_info {
	u8	simm_something[NUMSIMMS];
	u8	prom_flags;
	u32	sid;
	u32	pagesize;
	u32	mon_stack;
	u32	vbr;

	struct nvram_settings {
		u32	flags;
		#define	NV_RESET	0xF0000000
		#define	NV_ALT_CONS	0x08000000
		#define	NV_ALLOW_EJECT	0x04000000
		#define	NV_VOL_R	0x03F00000
		#define	NV_BRIGHTNESS	0x000FC000
		#define	NV_HW_PWD	0x00003C00
		#define	NV_VOL_L	0x000003F0
		#define	NV_SPKREN	0x00000008
		#define	NV_LOWPASS	0x00000004
		#define	NV_BOOT_ANY	0x00000002
		#define	NV_ANY_CMD	0x00000001
		u8	eaddr[6];	// and passwd !?
		u16	simm_something;	// 4 bits / simm
		u8	adobe[2];
		u8	pot[3];
		u8	moreflags;
		#define NV_NEW_CLOCK_CHIP	0x80
		#define NV_AUTO_POWERON		0x40
		#define NV_USE_CONSOLE_SLORT	0x20
		#define NV_CONSOLE_SLOT		0x18
		u8	bootcmdline[12];
		u16	checksum;
	} nvram;

	u8 inetntoa[18],inputline[128];
	struct  mem_layout {
		u32 start;
		u32 end;
	} simm_info[NUMSIMMS];

	u32	base_ptr;
	u32	brk_ptr;
	u32	bootdev_ptr;
	u32	bootarg_ptr;
	u32	bootinfo_ptr;
	u32	bootfile_ptr;
	u8	bootfile[64];
	u32	boot_mode; // enum
	u8	km_mon_stuff[4+4+2+2+2+2+2+2+4+4+4+2+4+3*2+2]; // 46
	u32	moninit;
	u32	sio_ptr;
	u32	time;
	u32	sddp_ptr;
	u32	dgp_ptr;
	u32	s5cp_ptr;
	u32	odc_ptr;
	u32	odd_ptr;
	u8	radix;

	u32	dmachip;
	u32	diskchip;
	u32	stat_ptr;
	u32	mask_ptr;

	u32	nofault_func;
	u8	fmt;
	u32	addr[8],na;
	u32	mousex;
	u32	mousey;
	u32	cursor[2][32];

	u32	getc_func;
	u32	try_getc_func;
	u32	putc_func;
	u32	alert_func;
	u32	alert_conf_func;
	u32	alloc_func;
	u32	boot_slider_func;
	u32	event_ptr;
	u32	event_high;

	// the port is not complete until we have a boot loader
	// that uses this to have a dancing penguin!
	struct anim {
		u16	x;
		u16	y;
		u32	icon_ptr;
		u8	next;
		u8	time;
	} *anim_ptr;
	u32	anim_time;

	u32	scsi_intr_func;	// void ()
	u32	scsi_intr_arg;
	u16	minor;
	u16	seq;
	u32	anim_run_func;	// int ()
	u16	major;
	u32	client_ether_addr_ptr;
	u32	cons_i;
	u32	cons_o;
	u32	test_msg_ptr;

	struct nextfb_prominfo {
		u32	pixels_pword;
		u32	line_length;	// bytes / scan
		u32	vispixx;	// pixels displayed
		u32	realpixx;	// real pixel width
		u32	height;
		u32	flags;
		u32	white;
		u32	light_grey;
		u32	dark_grey;
		u32	black;
		u8	slot;
		u8	fbnum;
		u8	laneid;
		u32	before_code;
		u32	after_code;
		struct nextfb_businfo {
			u32	phys;
			u32	virt;
			u32	len;
		} frames[6];	// [1] is framebuffer, [2] is backing store
		u32	stack;
	} fbinfo;

	u32	fdgp_ptr;
	u8	mach_type;
	u8	next_board_rev;

	u32	as_tune_func;	// int ()
	u32	moreflags;
};

extern struct prom_info prom_info;

struct eprom_info {
	unsigned char eaddr[6];
};
extern struct eprom_info eprom_info;

#endif // _ASM_NEXTHW_H_
