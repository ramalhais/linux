// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/arch/m68k/next/config.c
 *
 *  This file contains the NeXT-specific initialisation code.  It gets
 *  called by setup_mm.c .
 */

//#include <linux/module.h>
//#include <linux/string.h>
//#include <linux/rtc.h>

//#include <linux/config.h>
//#include <linux/types.h>
#include <linux/kernel.h>
//#include <linux/mm.h>
//#include <linux/kd.h>
//#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>

//#include <asm/byteorder.h>
//#include <asm/blinken.h>
//#include <asm/io.h>                               /* readb() and writeb() */
//#include <asm/config.h>

#include <asm/bootinfo.h>
#include <asm/bootinfo-next.h>
//#include <asm/setup.h>
//#include <asm/system.h>
//#include <asm/pgtable.h>
//#include <asm/irq.h>
#include <asm/machdep.h>

#include <asm/nextints.h> 
#include <asm/nexthw.h> 
#include "rtc.h"

/* supplied by the boot prom software */
struct prom_info prom_info;
/* yanked from the eprom chip */
struct eprom_info eprom_info;

extern int next_kbd_init(void);
extern int next_get_irq_list(char *buf);
// extern void next_clock_init(void);
// extern void next_gettod(int *yearp, int *monp, int *dayp, int *hourp, int *minp, int *secp);
// extern unsigned long next_gettimeoffset(void);
// extern void rtc_init(void);

// void __init next_sched_init(void)
// {
//        next_clock_init();
// }

void next_get_model(char *model) {
       strcpy(model,"Generic NeXT");
}

void __init next_meminit(void) {
	int i;
	unsigned int len;

	m68k_num_memory=0;
	for(i=0;i<4;i++){
		printk("SIMM bank %d: ",i);
		if(prom_info.simm_info[i].start==0) {
			printk("unused\n");
			continue;
		}
		m68k_memory[m68k_num_memory].addr=prom_info.simm_info[i].start;

		/*
		* round len up to the nearest meg as it seems
		* the end addr sometimes doesn't include the
		* last page 
		*/

		len=prom_info.simm_info[i].end-prom_info.simm_info[i].start;
		if(len&0xfffff) len+=0x100000-(len&0xfffff); /* silly */
		m68k_memory[m68k_num_memory].size=len;
		
		printk("%2lxMB at 0x%0lx\n",
			m68k_memory[m68k_num_memory].size>>20,
			m68k_memory[m68k_num_memory].addr);

		m68k_num_memory++;
	}
}

/*
#ifdef CONFIG_SERIAL_8250_CONSOLE
extern int next_setup_serial_console(void) __init;
#endif
*/

void __init next_init_IRQ(void)
{
	// Disable NeXT interrupts
	// (*((volatile u_int *)NEXT_INTMASK)) = 0;
	next_get_intmask() = 0;
	// next_intmask_enable(NEXT_IRQ_NMI-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_PFAIL-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_BUS-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_POWER-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_SOFTINT1-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_SOFTINT0-NEXT_IRQ_BASE);
}

void __init config_next(void)
{
	*(volatile unsigned char *)(0xff110000)=0x7A;
	next_meminit();

	mach_init_IRQ		= next_init_IRQ;
//	mach_request_irq	= next_request_irq;
//	mach_free_irq		= next_free_irq;
//	enable_irq			= next_enable_irq;
//	disable_irq			= next_disable_irq;
//	mach_process_int	= next_process_int;
//	mach_default_handler	= &next_default_handlers;

//	mach_keyb_init		= next_kbd_init;

	mach_sched_init		= next_sched_init;
	// mach_gettimeoffset	= next_gettimeoffset; // not used anymore?
	// mach_gettod			= next_gettod; // not used anymore?
	mach_get_model		= next_get_model;
//	mach_get_irq_list 	= next_get_irq_list;


//void (*mach_get_hardware_list) (struct seq_file *m);
//unsigned int (*mach_get_ss)(void);
//void (*mach_reset)( void );
//void (*mach_halt)( void );
//void (*mach_power_off)( void );
//#ifdef CONFIG_HEARTBEAT
//void (*mach_heartbeat) (int);
//#ifdef CONFIG_M68K_L2_CACHE
//void (*mach_l2_flush) (int);

//extern int (*mach_hwclk)(int, struct rtc_time*);
//extern int (*mach_get_rtc_pll)(struct rtc_pll_info *);
//extern int (*mach_set_rtc_pll)(struct rtc_pll_info *);
//extern unsigned long (*mach_hd_init) (unsigned long, unsigned long);
//extern void (*mach_hd_setup)(char *, int *);
//extern void (*mach_beep) (unsigned int, unsigned int);


#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp          = &dummy_con;
#endif

//	mach_max_dma_address = 0xffffffff;

/*
#ifdef CONFIG_NEXT_CONSOLE
	next_setup_serial_console();
#endif
*/
}
