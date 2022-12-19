// SPDX-License-Identifier: GPL-2.0

// arch/m68k/next/ints.c
// 2022-12-18 Pedro Ramalhais <ramalhais@gmail.com>

#include <linux/interrupt.h>
#include <asm/nextints.h>

/* old stuff
// Description of next int sources
// (level ) + (offset << 3) + (shared flag << 7)
#define INF_SHARED(x) (x&0x80)
#define INF_OFFS(x) ((x>>3)&15)
#define INF_LEVEL(x) (x&7)
#define INF(l,o,s) (l+(o<<3)+(s<<7))

int irqinfo[NEXT_NRINTS]={
	INF(1,0,0),		// NEXT_IRQ_SOFTINT0
	INF(2,0,0),		// NEXT_IRQ_SOFTINT1
	INF(3,0,1),		// NEXT_IRQ_POWER
	INF(3,1,1),		// NEXT_IRQ_ADB
	INF(3,2,1),		// NEXT_IRQ_MONITOR
	INF(3,3,1),		// NEXT_IRQ_VIDEO
	INF(3,4,1),		// NEXT_IRQ_DSP_3
	INF(3,5,1),		// NEXT_IRQ_PHONE
	INF(3,6,1),		// NEXT_IRQ_SOUND_OVRUN
	INF(3,7,1),		// NEXT_IRQ_ENETR
	INF(3,8,1),		// NEXT_IRQ_ENETX
	INF(3,9,1),		// NEXT_IRQ_PRINTER
	INF(3,10,1),	// NEXT_IRQ_SCSI
	INF(3,11,1),	// NEXT_IRQ_C16_VIDEO. used to be esdi?
	INF(4,0,0),		// NEXT_IRQ_DSP_4
	INF(5,0,1),		// NEXT_IRQ_BUS
	INF(5,1,1),		// NEXT_IRQ_REMOTE
	INF(5,2,1),		// NEXT_IRQ_SCC
	INF(6,0,1),		// NEXT_IRQ_R2M_DMA
	INF(6,1,1),		// NEXT_IRQ_M2R_DMA
	INF(6,2,1),		// NEXT_IRQ_DSP_DMA
	INF(6,3,1),		// NEXT_IRQ_SCC_DMA
	INF(6,4,1),		// NEXT_IRQ_SOUND_IN_DMA
	INF(6,5,1),		// NEXT_IRQ_SOUND_OUT_DMA
	INF(6,6,1),		// NEXT_IRQ_PRINTER_DMA
	INF(6,7,1),		// NEXT_IRQ_DISK_DMA
	INF(6,8,1),		// NEXT_IRQ_SCSI_DMA
	INF(6,9,1),		// NEXT_IRQ_ENETR_DMA
	INF(6,10,1),	// NEXT_IRQ_ENETX_DMA
	INF(6,11,1),	// NEXT_IRQ_TIMER
	INF(7,0,1),		// NEXT_IRQ_PFAIL
	INF(7,1,1),		// NEXT_IRQ_NMI
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

// XXX use for int cursor in fbcon instead of just disabling it
#define C16_INTCLEAR	1
#define C16_INTENABLE	2
#define C16_UNBLANK	4
*(volatile unsigned char *)0xff018180= (C16_UNBLANK|C16_INTCLEAR)&~(C16_INTENABLE);
*/

static irqreturn_t next_nmi_handler(int irq, void *dev_id)
{
	static volatile int in_nmi;

	if (in_nmi)
		return IRQ_HANDLED;
	in_nmi = 1;

	pr_info("Non-Maskable Interrupt\n");
	show_registers(get_irq_regs());

	in_nmi = 0;
	return IRQ_HANDLED;
}

static irqreturn_t next_pfail_handler(int irq, void *dev_id)
{
	pr_info("Power Fail Interrupt\n");
	show_registers(get_irq_regs());

	return IRQ_HANDLED;
}

static void next_irq_mask(struct irq_data *data)
{
	next_intmask_disable(data->irq - NEXT_IRQ_BASE);
}

static void next_irq_unmask(struct irq_data *data)
{
	next_intmask_enable(data->irq - NEXT_IRQ_BASE);
}

static struct irq_chip next_irq_chip = {
	.name		= "NeXT",
	.irq_mask	= next_irq_mask,
	.irq_unmask	= next_irq_unmask,
};

static void next_int3(struct irq_desc *desc)
{
	for (int i = NEXT_IPL_3MIN; i < NEXT_IPL_3MIN + NEXT_IPL_3NUM; i++)
		if (next_irq_pending(NEXT_IRQ_BASE + i))
			generic_handle_irq(NEXT_IRQ_BASE + i);
}

static void next_int5(struct irq_desc *desc)
{
	for (int i = NEXT_IPL_5MIN; i < NEXT_IPL_5MIN + NEXT_IPL_5NUM; i++)
		if (next_irq_pending(NEXT_IRQ_BASE + i))
			generic_handle_irq(NEXT_IRQ_BASE + i);
}

static void next_int6(struct irq_desc *desc)
{
	for (int i = NEXT_IPL_6MIN; i < NEXT_IPL_6MIN + NEXT_IPL_6NUM; i++)
		if (next_irq_pending(NEXT_IRQ_BASE + i))
			generic_handle_irq(NEXT_IRQ_BASE + i);
}

static void next_int7(struct irq_desc *desc)
{
	for (int i = NEXT_IPL_7MIN; i < NEXT_IPL_7MIN + NEXT_IPL_7NUM; i++)
		if (next_irq_pending(NEXT_IRQ_BASE + i))
			generic_handle_irq(NEXT_IRQ_BASE + i);
}

void __init next_init_IRQ(void)
{
	// Disable all interrupts on NeXT IRQ controller
	next_intmask = 0;

	m68k_setup_irq_controller(&next_irq_chip, handle_simple_irq, NEXT_IRQ_BASE, NEXT_NRINTS);

	irq_set_chained_handler(IRQ_AUTO_3, next_int3);
	irq_set_chained_handler(IRQ_AUTO_5, next_int5);
	irq_set_chained_handler(IRQ_AUTO_6, next_int6);
	irq_set_chained_handler(IRQ_AUTO_7, next_int7);

	if (request_irq(NEXT_IRQ_NMI, next_nmi_handler, 0, "NMI", next_nmi_handler))
		pr_err("Couldn't register NMI interrupt\n");
	if (request_irq(NEXT_IRQ_PFAIL, next_pfail_handler, 0, "Power Fail", next_pfail_handler))
		pr_err("Couldn't register Power Fail interrupt\n");
}
