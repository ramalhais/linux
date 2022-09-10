/*
 *  linux/arch/m68k/next/ints.c
 *
 *  Copyright (C) 1998 Zach Brown <zab@zabbo.net>
 *
 *  NeXT interrupt goo.
 *
 *  alot of this was based on macints.c
 */

#include <linux/init.h>
//#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

//#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>

#include <asm/nextints.h>

#include "rtc.h"

#undef NEXT_DEBUGINT 

/*
 * Interrupt handler and parameter types
 */
struct irqhandler {
        void    (*handler)(int, void *, struct pt_regs *);
        void    *dev_id;
	unsigned long   flags;
	const char      *devname;
};

/*
 * Description of next int sources
 * (level ) + (offset << 3) + (shared flag << 7)
 */

#define INF_SHARED(x) (x&0x80)
#define INF_OFFS(x) ((x>>3)&15)
#define INF_LEVEL(x) (x&7)

#define INF(l,o,s) (l+(o<<3)+(s<<7))

#define NEXT_NRINTS 32

int irqinfo[NEXT_NRINTS]={
	INF(1,0,0),	/* NEXT_IRQ_SOFTINT0 */
	INF(2,0,0),	/* NEXT_IRQ_SOFTINT1 */
	INF(3,0,1),	/* NEXT_IRQ_POWER */
	INF(3,1,1),	/* NEXT_IRQ_ADB */
	INF(3,2,1),	/* NEXT_IRQ_MONITOR */
	INF(3,3,1),	/* NEXT_IRQ_VIDEO */
	INF(3,4,1),	/* NEXT_IRQ_DSP_3 */
	INF(3,5,1),	/* NEXT_IRQ_PHONE */
	INF(3,6,1),	/* NEXT_IRQ_SOUND_OVRUN */
	INF(3,7,1),	/* NEXT_IRQ_ENETR */
	INF(3,8,1),	/* NEXT_IRQ_ENETX */
	INF(3,9,1),	/* NEXT_IRQ_PRINTER */
	INF(3,10,1),	/* NEXT_IRQ_SCSI */
	INF(3,11,1),	/* NEXT_IRQ_C16_VIDEO */ /* used to be esdi? */
	INF(4,0,0),	/* NEXT_IRQ_DSP_4 */
	INF(5,0,1),	/* NEXT_IRQ_BUS */
	INF(5,1,1),	/* NEXT_IRQ_REMOTE */
	INF(5,2,1),	/* NEXT_IRQ_SCC */
	INF(6,0,1),	/* NEXT_IRQ_R2M_DMA */
	INF(6,1,1),	/* NEXT_IRQ_M2R_DMA */
	INF(6,2,1),	/* NEXT_IRQ_DSP_DMA */
	INF(6,3,1),	/* NEXT_IRQ_SCC_DMA */
	INF(6,4,1),	/* NEXT_IRQ_SOUND_IN_DMA */
	INF(6,5,1),	/* NEXT_IRQ_SOUND_OUT_DMA */
	INF(6,6,1),	/* NEXT_IRQ_PRINTER_DMA */
	INF(6,7,1),	/* NEXT_IRQ_DISK_DMA */
	INF(6,8,1),	/* NEXT_IRQ_SCSI_DMA */
	INF(6,9,1),	/* NEXT_IRQ_ENETR_DMA */
	INF(6,10,1),	/* NEXT_IRQ_ENETX_DMA */
	INF(6,11,1),	/* NEXT_IRQ_TIMER */
	INF(7,0,1),	/* NEXT_IRQ_PFAIL */
	INF(7,1,1),	/* NEXT_IRQ_NMI */
};

unsigned int levelinfo[8]={
	0,
	0,
	0,
	(NEXT_IPL_3MIN<<16)+NEXT_IPL_3NUM,
	0,
	(NEXT_IPL_5MIN<<16)+NEXT_IPL_5NUM,
	(NEXT_IPL_6MIN<<16)+NEXT_IPL_6NUM,
	(NEXT_IPL_7MIN<<16)+NEXT_IPL_7NUM
};

/*
 * Array of irq handlers / next interrupt..
 * if an interrupt isn't shared its handler
 * is installed in the trap table instead of the
 * shared handler that uses this array..
 */

static struct irqhandler handler_table[NEXT_NRINTS];

/*
 * number of ints for each..
 */

static unsigned long next_irqs[NEXT_NRINTS];

/*
 * funcs
 */
void next_default_handler(int vec, void *blah2, struct pt_regs *fp); 
void next_do_int(int vec, void *blah2, struct pt_regs *fp); 
void video_shutup(int irq, void *dev_id, struct pt_regs *regs);

extern void next_poweroff(int vec, void *blah2, struct pt_regs *fp);


/* it seems that sometimes you can't
	disable some ints.  uh.  so
	we keep our own mask :( */
int ourmask;

void next_set_int_mask(int mask) {
	*(next_intmask_ptr)=ourmask=mask;
#ifdef NEXT_DEBUGINT
	printk("ourmask: %x realmask: %x\n",ourmask,next_get_intmask());
#endif
	
}	

void next_disable_irq (unsigned int irq) {
#ifdef NEXT_DEBUGINT
	printk("Disable: %d\n",irq);
#endif
	*(next_intmask_ptr)=ourmask&=~(1<<(irq-NEXT_IRQ_BASE));
}

void next_enable_irq (unsigned int irq) {
#ifdef NEXT_DEBUGINT
	printk("Enable: %d\n",irq);
#endif
	*(next_intmask_ptr)=ourmask|=1<<(irq-NEXT_IRQ_BASE);
#ifdef NEXT_DEBUGINT
	printk("ourmask: %x realmask: %x\n",ourmask,next_get_intmask());
#endif
}

void next_turnoff_irq (unsigned int irq) {
#ifdef NEXT_DEBUGINT
	printk("Disable: %d\n",irq);
#endif
	*(next_intmask_ptr)=ourmask&=~(1<<(irq-NEXT_IRQ_BASE));
}

void next_turnon_irq (unsigned int irq) {
#ifdef NEXT_DEBUGINT
	printk("Enable: %d\n",irq);
#endif
	*(next_intmask_ptr)=ourmask|=1<<(irq-NEXT_IRQ_BASE);
}


/*static */void __init next_init_IRQ(void)
{
}

/*void __init next_init_IRQOLD(void)
{
	int i;
	struct irqhandler *hand=&handler_table[0];


#ifdef NEXT_DEBUGINT
	printk("Initializing NeXT interrupts\n");
	
#endif
	next_set_int_mask(0);

	if(next_get_intmask()) {
		printk("Uh oh, you're intrmask appears to be on crack: 0x%0x isn't 0.\n",
			next_get_intmask());
	}
*/
	/* XXX use for int cursor in fbcon instead
		of just disabling it *//*
#define C16_INTCLEAR	1
#define C16_INTENABLE	2
#define C16_UNBLANK	4
	*(volatile unsigned char *)0xff018180=
		(C16_UNBLANK|C16_INTCLEAR)&~(C16_INTENABLE);
*/
        /* init the auto vector demultiplexor handler thingees *//*
        sys_request_irq(3, next_do_int, IRQ_FLG_LOCK, "ipl3", next_do_int);
        sys_request_irq(5, next_do_int, IRQ_FLG_LOCK, "ipl5", next_do_int);
        sys_request_irq(6, next_do_int, IRQ_FLG_LOCK, "ipl6", next_do_int);
        sys_request_irq(7, next_do_int, IRQ_FLG_LOCK, "ipl7", next_do_int);
*/
	/* init handler arrays */
/*
	for(i=0;i<NEXT_NRINTS;i++) {

		hand->handler=next_default_handler;
		hand->dev_id=NULL;
		hand->flags=IRQ_FLG_STD;
		hand->devname=NULL;

		hand++;
	}
*/
	/* should put checks around this in prod kernel :) *//*
	request_irq(NEXT_IRQ_POWER,next_poweroff,IRQ_FLG_LOCK,"poweroff",next_poweroff);


#ifdef NEXT_DEBUGINT
	printk("Done initing interrupt fun. mask: %x stat: %x\n",next_get_intmask(),next_get_intstat());
#endif
}
*/

/*
 * Register our machine pseudo irqs through the vector unmungers, 
 * or in place if they are the only int in their ipl
 */
/*
int next_request_irq (unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
	unsigned long flags, const char *devname, void *dev_id)
{
	int info;
#ifdef NEXT_DEBUGINT
	printk("%s: IRQ %d requested from %s\n",__FUNCTION__,irq,devname);
#endif
*/
	/* XXX fixme, por favor */
#if 0
	if (flags < IRQ_TYPE_SLOW || flags > IRQ_TYPE_PRIO) {
		printk ("%s: Bad irq type %ld requested from %s\n",
			__FUNCTION__, flags, devname);
		return -EINVAL;
	}
#endif

	/* make sure its in range *//*
        if (irq < NEXT_IRQ_SOFTINT0 || irq > NEXT_IRQ_NMI ) {
                printk ("%s: Bad irq source %d requested from %s\n",
                        __FUNCTION__, irq,  devname);
                return -EINVAL;
        }
	
	info=irqinfo[irq-NEXT_IRQ_BASE];

	if(INF_SHARED(info)) {
		struct irqhandler *hand;
*/
		/* really should check for conflicts *//*
		hand=&handler_table[irq-NEXT_IRQ_BASE];
		
		hand->handler=handler;
		hand->dev_id=dev_id;
		hand->flags=flags;
		hand->devname=devname;
#ifdef NEXT_DEBUGINT
		printk("%s: installed shared irq %d from %s\n",__FUNCTION__,irq,devname);
#endif
	} else {*/
		/* its the only on its ipl *//*
		sys_request_irq(INF_LEVEL(info),handler,flags,devname,dev_id);
	}
*/
	/* enable it *//*
	next_enable_irq(irq);

	return 0;
}
*/
/*
void next_free_irq (unsigned int irq, void *dev_id)
{
	unsigned long flags;
	struct irqhandler *hand;
	int info;

#ifdef NEXT_DEBUGINT
        printk ("%s: IRQ %d freed\n", __FUNCTION__, irq);
#endif
*/
	/* figure out what VIA is handling this irq *//*
	if (irq < NEXT_IRQ_SOFTINT0 || irq > NEXT_IRQ_NMI ) {
#ifdef NEXT_DEBUGINT
	        printk ("%s :trying to free a non mach irq\n",__FUNCTION__);
#endif
		return;
	}

	info=irqinfo[irq-NEXT_IRQ_BASE];

	save_flags(flags);
	cli();

        if(INF_SHARED(info)) {*/
		/* really should check for conflicts *//*
		hand=&handler_table[irq-NEXT_IRQ_BASE];
	
		if(hand->dev_id != dev_id) {
			restore_flags(flags);
			printk("%s: tried to remove invalid irq\n", __FUNCTION__);
			return;
		}
		hand->handler=next_default_handler;
		hand->dev_id=NULL;
		hand->flags=IRQ_FLG_STD;
		hand->devname=NULL;
	} else {*/
		/* its the only on its ipl *//*
		sys_free_irq(INF_LEVEL(info),dev_id);
	}
*/
	/* mask it off  *//*
	next_disable_irq(irq-NEXT_IRQ_BASE);

	restore_flags(flags);
        return;
}
*/

/*
 * This is called for ints on the NeXT that share single
 * priority levels.  We walk through the intr stat for the
 * level and call handlers for pending ints
 */
/*
void next_do_int(int irq, void *dev_id, struct pt_regs *regs)
{
	register unsigned int i,bit,index,info;
	int stat=next_get_intstat();
	struct irqhandler *hand;
	*/
	/*
	 * use cool bit field instructions
	 */
/*
	index=levelinfo[irq]>>16;
	bit=1<<index;
	for(i=levelinfo[irq]&0xff;i;i--,bit<<=1,index++) {
		if(!(stat & bit & ourmask)) continue; 

		info=irqinfo[index];
		hand=&handler_table[index];

		next_irqs[index]++;

#ifdef NEXT_DEBUGINT
		if(hand->dev_id==NULL) {
			printk("no handler?\n");
			continue;
		}
#endif
			
		(hand->handler)(irq, hand->dev_id, regs);
*/
		/* retrigger timer int, should be elsewhere *//*
		if(index == NEXT_IRQ_TIMER-NEXT_IRQ_BASE)  {
*/
			/* only touch the update bit *//*
		        set_timer_csr_bits(TIM_RESTART); 
		}
	}

}
*/
void next_default_handler(int vec, void *blah2, struct pt_regs *fp)
{
	printk("Unexpected interrupt %d (mask: 0x%0X stat: 0x%0X\n",
		vec,next_get_intmask(),next_get_intstat());
}
/*
void (*next_default_handlers[SYS_IRQS])(int, void *, struct pt_regs *) = {
	next_default_handler, next_default_handler, 
	next_default_handler, next_default_handler,
	next_default_handler, next_default_handler, 
	next_default_handler, next_default_handler
};
*/
void next_process_int(int vec, struct pt_regs *fp)
{
	printk("next_process_int()ing int from vec %d\n",vec);
}

int next_get_irq_list(char *buf) 
{
	int i,len=0;

	int *info=&irqinfo[0];
	struct irqhandler *hand=&handler_table[0];

	for(i=0;i<NEXT_NRINTS;i++,hand++,info++) {

		if(!INF_SHARED(*info)) continue;
		if(hand->handler==next_default_handler) continue;

		len+=sprintf(buf+len, "next %2d: %10lu %s\n",
			i+1,next_irqs[i],hand->devname);
	} 

	return len;
}
