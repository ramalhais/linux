/*
** asm/nextings.h -- This header defines NeXT specific macros and defines
**                    for playing with NeXT hardware.
**
** Copyright 1998 by Zach Brown <zab@zabbo.net>
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
*/


#ifndef _ASM_NEXTINTS_H_
#define _ASM_NEXTINTS_H_

#include <linux/types.h>
#include <asm/nexthw.h>

#define NEXT_INTSTAT (NEXT_IO_BASE+0x7000)
#define NEXT_INTMASK (NEXT_IO_BASE+0x7800)

#define NEXT_IRQ_BASE 8

#define next_intmask_ptr ((volatile u_int *)NEXT_INTMASK)
#define next_get_intmask() (*next_intmask_ptr)

#define next_intstat_ptr ((volatile u_int *)NEXT_INTSTAT)
#define next_get_intstat() (*next_intstat_ptr)

#define next_irq_pending(x) \
	((*next_intstat_ptr)&(1<<(x-NEXT_IRQ_BASE)))

#define next_intmask_disable(x) \
	((*next_intmask_ptr) &= ~(1<<(x)))
#define next_intmask_enable(x) \
	((*next_intmask_ptr) |= (1<<(x)))

// hard wired interrupts
#define NEXT_IRQ_NMI		(NEXT_IRQ_BASE+31)
#define NEXT_IRQ_PFAIL		(NEXT_IRQ_BASE+30)

#define NEXT_IPL_7MIN		30
#define NEXT_IPL_7NUM		2

#define NEXT_IRQ_TIMER		(NEXT_IRQ_BASE+29)
#define NEXT_IRQ_ENETX_DMA	(NEXT_IRQ_BASE+28)
#define NEXT_IRQ_ENETR_DMA	(NEXT_IRQ_BASE+27)
#define NEXT_IRQ_SCSI_DMA	(NEXT_IRQ_BASE+26)
#define NEXT_IRQ_DISK_DMA	(NEXT_IRQ_BASE+25)
#define NEXT_IRQ_PRINTER_DMA	(NEXT_IRQ_BASE+24)
#define NEXT_IRQ_SOUND_OUT_DMA	(NEXT_IRQ_BASE+23)
#define NEXT_IRQ_SOUND_IN_DMA	(NEXT_IRQ_BASE+22)
#define NEXT_IRQ_SCC_DMA	(NEXT_IRQ_BASE+21)
#define NEXT_IRQ_DSP_DMA	(NEXT_IRQ_BASE+20)
#define NEXT_IRQ_M2R_DMA	(NEXT_IRQ_BASE+19)
#define NEXT_IRQ_R2M_DMA	(NEXT_IRQ_BASE+18)

#define NEXT_IPL_6MIN		18
#define NEXT_IPL_6NUM		12

#define NEXT_IRQ_SCC		(NEXT_IRQ_BASE+17)
#define NEXT_IRQ_REMOTE		(NEXT_IRQ_BASE+16)
#define NEXT_IRQ_BUS		(NEXT_IRQ_BASE+15)

#define NEXT_IPL_5MIN		15
#define NEXT_IPL_5NUM		3

#define NEXT_IRQ_DSP_4		(NEXT_IRQ_BASE+14)

#define NEXT_IRQ_C16_VIDEO	(NEXT_IRQ_BASE+13) // old esdi?
#define NEXT_IRQ_SCSI		(NEXT_IRQ_BASE+12) 
#define NEXT_IRQ_PRINTER	(NEXT_IRQ_BASE+11)
#define NEXT_IRQ_ENETX		(NEXT_IRQ_BASE+10)
#define NEXT_IRQ_ENETR		(NEXT_IRQ_BASE+9)
#define NEXT_IRQ_SOUND_OVRUN	(NEXT_IRQ_BASE+8)
#define NEXT_IRQ_PHONE		(NEXT_IRQ_BASE+7)
#define NEXT_IRQ_DSP_3		(NEXT_IRQ_BASE+6)
#define NEXT_IRQ_VIDEO		(NEXT_IRQ_BASE+5)
#define NEXT_IRQ_MONITOR	(NEXT_IRQ_BASE+4)
#define NEXT_IRQ_KYBD_MOUSE	(NEXT_IRQ_BASE+3)
#define NEXT_IRQ_POWER		(NEXT_IRQ_BASE+2)

#define NEXT_IPL_3MIN		2
#define NEXT_IPL_3NUM		12

#define NEXT_IRQ_SOFTINT1	(NEXT_IRQ_BASE+1)

#define NEXT_IRQ_SOFTINT0	(NEXT_IRQ_BASE+0)

extern void next_init_IRQ(void);
extern void next_process_int(int, struct pt_regs *);
extern void (*next_default_handlers[])(int, void *, struct pt_regs *);
extern int next_request_irq (unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
	unsigned long flags, const char *devname, void *dev_id);
extern void next_free_irq (unsigned int irq, void *dev_id);
extern void next_disable_irq (unsigned int irq);
extern void next_enable_irq (unsigned int irq);
extern void next_turnon_irq (unsigned int irq);
extern void next_turnoff_irq (unsigned int irq);

#endif // _ASM_NEXTINTS_H_
