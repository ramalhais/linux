// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/arch/m68k/next/config.c
 *
 *  Copyright (C) 1998 Zach Brown <zab@zabbo.net>
 *  Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
 *
 *  This file contains the NeXT-specific initialisation code.  It gets
 *  called by setup_mm.c .
 */

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>
#include <asm/machdep.h>

#include <asm/nextints.h> 
#include <asm/nexthw.h> 
#include "rtc.h"

/* supplied by the boot prom software */
struct prom_info prom_info;
/* yanked from the eprom chip */
struct eprom_info eprom_info;

void next_get_model(char *model) {
       strcpy(model,"Generic NeXT");
}

void __init next_meminit(void) {
	int i;
	unsigned int len;

	*(volatile unsigned long *)(0xff00f004)=0x10;
	m68k_num_memory=0;
	for(i=0;i<4;i++){
	*(volatile unsigned long *)(0xff00f004)=i;
		if(prom_info.simm_info[i].start==0) {
	*(volatile unsigned long *)(0xff00f004)=0x0;
			printk("SIMM bank %d: Empty", i);
			continue;
		}
		m68k_memory[m68k_num_memory].addr=prom_info.simm_info[i].start;

		/*
		* round len up to the nearest meg as it seems
		* the end addr sometimes doesn't include the
		* last page 
		*/
		// (PR) The NeXT PROM sets up data at the end of the last filled memory bank and reserves that.
		// It's OK, we can reuse it, as long as we don't expect vbr and monitor global to be there.
		// The amount of memory lost would be very low, 1024+960 page aligned, sooooo, 8KB page as setup by PROM.

		len=prom_info.simm_info[i].end-prom_info.simm_info[i].start;
		if(len&0xfffff) len+=0x100000-(len&0xfffff); /* silly */
		m68k_memory[m68k_num_memory].size=len;

	*(volatile unsigned int *)(0xff00f004)=m68k_memory[m68k_num_memory].addr;
	*(volatile unsigned int *)(0xff00f004)=prom_info.simm_info[i].end;
		printk("SIMM bank %d: %d MB at 0x%0lx\n",
			i,
			(int)m68k_memory[m68k_num_memory].size>>20,
			m68k_memory[m68k_num_memory].addr);

		m68k_num_memory++;
	}
}

void __init next_init_IRQ(void)
{
	// Disable all interrupts on NeXT IRQ controller
	next_get_intmask() = 0;
}

void __init config_next(void)
{
	// *(volatile unsigned long *)(0xff00f004)=0x7A;
	next_meminit();

	mach_sched_init		= next_sched_init;
	mach_init_IRQ		= next_init_IRQ;
	mach_get_model		= next_get_model;
//void (*mach_get_hardware_list) (struct seq_file *m);
//unsigned int (*mach_get_ss)(void);
//void (*mach_reset)( void );
//void (*mach_halt)( void );
//void (*mach_power_off)( void );
//#ifdef CONFIG_HEARTBEAT // Example: LED as heartbeat
//void (*mach_heartbeat) (int);
//#ifdef CONFIG_M68K_L2_CACHE
//void (*mach_l2_flush) (int);

//extern int (*mach_hwclk)(int, struct rtc_time*);
//extern int (*mach_get_rtc_pll)(struct rtc_pll_info *);
//extern int (*mach_set_rtc_pll)(struct rtc_pll_info *);
//extern unsigned long (*mach_hd_init) (unsigned long, unsigned long);
//extern void (*mach_hd_setup)(char *, int *);
//extern void (*mach_beep) (unsigned int, unsigned int);


// #ifdef CONFIG_DUMMY_CONSOLE
// 	conswitchp          = &dummy_con;
// #endif

//	mach_max_dma_address = 0xffffffff;
}
