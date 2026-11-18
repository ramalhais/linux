// SPDX-License-Identifier: GPL-2.0
/*
 * next-zs.c: Serial port driver for the NeXT Zilog Z8530 SCC.
 *
 * Derived from drivers/tty/serial/zs.c, the DECstation Z85C30 driver,
 * which in turn was derived from drivers/sbus/char/sunserial.c and
 * drivers/macintosh/macserial.c.
 *
 * NeXT hardware notes:
 *   - The SCC is mapped at NEXT_SCC (slot I/O base + 0x18000) and has
 *     byte-sized registers in the following order:
 *         +0  channel B control
 *         +1  channel A control
 *         +2  channel B data
 *         +3  channel A data
 *         +4  SCC clock select register
 *   - Both channels share the NEXT_IRQ_SCC interrupt.  The Linux
 *     interrupt controller does not use an SCC supplied vector, so the
 *     SCC is kept in "no vector" mode.
 *   - The baud rate generator is driven from the 3.6864 MHz PCLK input
 *     in x16 mode.  The clock select register only selects the SCLK
 *     A/B outputs and does not affect PCLK.
 *
 * Original copyright notices:
 *
 * Copyright (C) 1998-2000 Harald Koerfgen
 * Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2007  Maciej W. Rozycki
 * Copyright (C) 1996 Paul Mackerras (Paul.Mackerras@cs.anu.edu.au)
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/spinlock.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/types.h>

#include <asm/nexthw.h>
#include <asm/nextints.h>

#include "zs.h"


MODULE_AUTHOR("Pedro Ramalhais <ramalhais@gmail.com>");
MODULE_DESCRIPTION("NeXT Z85C30 SCC serial driver");
MODULE_LICENSE("GPL");


static char zs_name[] __initdata = "NeXT Z85C30 serial driver version ";
static char zs_version[] __initdata = "0.01";

#define ZS_NUM_SCCS	1		/* Max # of ZS chips supported.  */
#define ZS_NUM_CHAN	2		/* 2 channels per chip.  */
#define ZS_CHAN_A	0		/* Index of the channel A.  */
#define ZS_CHAN_B	1		/* Index of the channel B.  */
#define ZS_CHAN_IO_SIZE 4		/* IOMEM space size.  */
#define ZS_CHAN_IO_STRIDE 2		/* Control to data register offset.  */
#define ZS_CHAN_IO_OFFSET 0		/* SCC registers start at the base.  */
#define ZS_CHAN_ADDR_STRIDE 1		/* Channel B at +0, channel A at +1.  */
#define ZS_SCC_IO_SIZE	0x20		/* Whole SCC block incl. clock.  */
#define ZS_CLOCK	3686400		/* PCLK input clock rate.  */

#define to_zport(uport) container_of(uport, struct zs_port, port)

static struct zs_scc zs_sccs[ZS_NUM_SCCS];
static struct uart_driver zs_reg;
static void __iomem *zs_membase;

/*
 * The PROM sets the SCC up for 9600 8N1.  Keep the same defaults so that
 * the console does not glitch during the handover.
 */
static u8 zs_init_regs[ZS_NUM_REGS] __initdata = {
	0,				/* write 0 */
	PAR_SPEC,			/* write 1 */
	0,				/* write 2 */
	0,				/* write 3 */
	X16CLK | SB1,			/* write 4 */
	DTR | RTS | Tx8 | TxENAB,	/* write 5 */
	0, 0, 0,			/* write 6, 7, 8 */
	MIE | DLC | NV,			/* write 9 */
	NRZ,				/* write 10 */
	TCBR | RCBR,			/* write 11 */
	0x0a, 0x00,			/* BRG time constant, write 12 + 13 */
	BRSRC | BRENABL,		/* write 14 */
	0,				/* write 15 */
};

/*
 * Debugging.
 */
#undef ZS_DEBUG_REGS


/*
 * Reading and writing Z85C30 registers.
 */
static void recovery_delay(void)
{
	udelay(2);
}

static u8 read_zsreg(struct zs_port *zport, int reg)
{
	void __iomem *control = zport->port.membase + ZS_CHAN_IO_OFFSET;
	u8 retval;

	if (reg != 0) {
		writeb(reg & 0xf, control);
		recovery_delay();
	}
	retval = readb(control);
	recovery_delay();
	return retval;
}

static void write_zsreg(struct zs_port *zport, int reg, u8 value)
{
	void __iomem *control = zport->port.membase + ZS_CHAN_IO_OFFSET;

	if (reg != 0) {
		writeb(reg & 0xf, control);
		recovery_delay();
	}
	writeb(value, control);
	recovery_delay();
}

static u8 read_zsdata(struct zs_port *zport)
{
	void __iomem *data = zport->port.membase +
			     ZS_CHAN_IO_STRIDE + ZS_CHAN_IO_OFFSET;
	u8 retval;

	retval = readb(data);
	recovery_delay();
	return retval;
}

static void write_zsdata(struct zs_port *zport, u8 value)
{
	void __iomem *data = zport->port.membase +
			     ZS_CHAN_IO_STRIDE + ZS_CHAN_IO_OFFSET;

	writeb(value, data);
	recovery_delay();
}

#ifdef ZS_DEBUG_REGS
void zs_dump(void)
{
	struct zs_port *zport;
	int i, j;

	for (i = 0; i < ZS_NUM_SCCS * ZS_NUM_CHAN; i++) {
		zport = &zs_sccs[i / ZS_NUM_CHAN].zport[i % ZS_NUM_CHAN];

		if (!zport->scc)
			continue;

		for (j = 0; j < 16; j++)
			pr_info("W%-2d = 0x%02x\t", j, zport->regs[j]);
		pr_info("\n");
		for (j = 0; j < 16; j++)
			pr_info("R%-2d = 0x%02x\t", j, read_zsreg(zport, j));
		pr_info("\n\n");
	}
}
#endif


static void zs_spin_lock_cond_irq(spinlock_t *lock, int irq)
{
	if (irq)
		spin_lock_irq(lock);
	else
		spin_lock(lock);
}

static void zs_spin_unlock_cond_irq(spinlock_t *lock, int irq)
{
	if (irq)
		spin_unlock_irq(lock);
	else
		spin_unlock(lock);
}

static int zs_receive_drain(struct zs_port *zport)
{
	int loops = 10000;

	while ((read_zsreg(zport, R0) & Rx_CH_AV) && --loops)
		read_zsdata(zport);
	return loops;
}

static int zs_transmit_drain(struct zs_port *zport, int irq)
{
	struct zs_scc *scc = zport->scc;
	int loops = 10000;

	while (!(read_zsreg(zport, R0) & Tx_BUF_EMP) && --loops) {
		zs_spin_unlock_cond_irq(&scc->zlock, irq);
		udelay(2);
		zs_spin_lock_cond_irq(&scc->zlock, irq);
	}
	return loops;
}

static int zs_line_drain(struct zs_port *zport, int irq)
{
	struct zs_scc *scc = zport->scc;
	int loops = 10000;

	while (!(read_zsreg(zport, R1) & ALL_SNT) && --loops) {
		zs_spin_unlock_cond_irq(&scc->zlock, irq);
		udelay(2);
		zs_spin_lock_cond_irq(&scc->zlock, irq);
	}
	return loops;
}


static void load_zsregs(struct zs_port *zport, u8 *regs, int irq)
{
	/* Let the current transmission finish.  */
	zs_line_drain(zport, irq);
	/* Load 'em up.  */
	write_zsreg(zport, R3, regs[3] & ~RxENABLE);
	write_zsreg(zport, R5, regs[5] & ~TxENAB);
	write_zsreg(zport, R4, regs[4]);
	write_zsreg(zport, R9, regs[9]);
	write_zsreg(zport, R1, regs[1]);
	write_zsreg(zport, R2, regs[2]);
	write_zsreg(zport, R10, regs[10]);
	write_zsreg(zport, R14, regs[14] & ~BRENABL);
	write_zsreg(zport, R11, regs[11]);
	write_zsreg(zport, R12, regs[12]);
	write_zsreg(zport, R13, regs[13]);
	write_zsreg(zport, R14, regs[14]);
	write_zsreg(zport, R15, regs[15]);
	if (regs[3] & RxENABLE)
		write_zsreg(zport, R3, regs[3]);
	if (regs[5] & TxENAB)
		write_zsreg(zport, R5, regs[5]);
}


/*
 * Status handling routines.
 */

/*
 * zs_tx_empty() -- get the transmitter empty status
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 *	    is emptied.  On bus types like RS485, the transmitter must
 *	    release the bus after transmitting.  This must be done when
 *	    the transmit shift register is empty, not be done when the
 *	    transmit holding register is empty.  This functionality
 *	    allows an RS485 driver to be written in user space.
 */
static unsigned int zs_tx_empty(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	unsigned long flags;
	u8 status;

	spin_lock_irqsave(&scc->zlock, flags);
	status = read_zsreg(zport, R1);
	spin_unlock_irqrestore(&scc->zlock, flags);

	return status & ALL_SNT ? TIOCSER_TEMT : 0;
}

/*
 * The SCC has no dedicated DSR input; on NeXT the DSR signal is wired
 * to the SYNC/HUNT pin.  CTS and DCD use the regular per-channel inputs.
 */
static unsigned int zs_rr0_to_mctrl(u8 status)
{
	unsigned int mctrl = 0;

	if (status & CTS)
		mctrl |= TIOCM_CTS;
	if (status & DCD)
		mctrl |= TIOCM_CAR;
	if (status & SYNC_HUNT)
		mctrl |= TIOCM_DSR;

	return mctrl;
}

static unsigned int zs_raw_get_mctrl(struct zs_port *zport)
{
	return zs_rr0_to_mctrl(read_zsreg(zport, R0));
}

static unsigned int zs_raw_xor_mctrl(struct zs_port *zport)
{
	unsigned int mctrl, delta;

	mctrl = zs_raw_get_mctrl(zport);
	delta = mctrl ^ zport->mctrl;
	if (delta)
		zport->mctrl = mctrl;

	return delta;
}

static unsigned int zs_get_mctrl(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	unsigned int mctrl;

	spin_lock(&scc->zlock);
	mctrl = zs_raw_get_mctrl(zport);
	spin_unlock(&scc->zlock);

	return mctrl;
}

static void zs_set_mctrl(struct uart_port *uport, unsigned int mctrl)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	u8 oldloop, newloop, oldr5, newr5;

	spin_lock(&scc->zlock);

	/* DTR and RTS are per-channel outputs on NeXT.  */
	oldr5 = zport->regs[5];
	newr5 = oldr5;
	if (mctrl & TIOCM_DTR)
		newr5 |= DTR;
	else
		newr5 &= ~DTR;
	if (mctrl & TIOCM_RTS)
		newr5 |= RTS;
	else
		newr5 &= ~RTS;
	if (newr5 != oldr5) {
		zport->regs[5] = newr5;
		write_zsreg(zport, R5, zport->regs[5]);
	}

	/* Rarely modified, so don't poke at hardware unless necessary. */
	oldloop = zport->regs[14];
	newloop = oldloop;
	if (mctrl & TIOCM_LOOP)
		newloop |= LOOPBAK;
	else
		newloop &= ~LOOPBAK;
	if (newloop != oldloop) {
		zport->regs[14] = newloop;
		write_zsreg(zport, R14, zport->regs[14]);
	}
	spin_unlock(&scc->zlock);
}

static void zs_raw_stop_tx(struct zs_port *zport)
{
	write_zsreg(zport, R0, RES_Tx_P);
	zport->tx_stopped = 1;
}

static void zs_stop_tx(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;

	spin_lock(&scc->zlock);
	zs_raw_stop_tx(zport);
	spin_unlock(&scc->zlock);
}

static void zs_raw_transmit_chars(struct zs_port *);

static void zs_start_tx(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;

	spin_lock(&scc->zlock);
	if (zport->tx_stopped) {
		zs_transmit_drain(zport, 0);
		zport->tx_stopped = 0;
		zs_raw_transmit_chars(zport);
	}
	spin_unlock(&scc->zlock);
}

static void zs_stop_rx(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;

	spin_lock(&scc->zlock);
	zport->regs[15] &= ~(BRKIE | DCDIE | CTSIE);
	zport->regs[1] &= ~(RxINT_MASK | TxINT_ENAB | EXT_INT_ENAB);
	zport->regs[1] |= RxINT_DISAB;

	write_zsreg(zport, R15, zport->regs[15]);
	write_zsreg(zport, R1, zport->regs[1]);
	spin_unlock(&scc->zlock);
}

static void zs_enable_ms(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;

	spin_lock(&scc->zlock);

	/* Clear Ext interrupts if not being handled already.  */
	if (!(zport->regs[1] & EXT_INT_ENAB))
		write_zsreg(zport, R0, RES_EXT_INT);

	zport->regs[1] |= EXT_INT_ENAB;
	zport->regs[15] |= DCDIE | CTSIE;

	zs_raw_xor_mctrl(zport);

	write_zsreg(zport, R1, zport->regs[1]);
	write_zsreg(zport, R15, zport->regs[15]);
	spin_unlock(&scc->zlock);
}

static void zs_break_ctl(struct uart_port *uport, int break_state)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	unsigned long flags;

	spin_lock_irqsave(&scc->zlock, flags);
	if (break_state == -1)
		zport->regs[5] |= SND_BRK;
	else
		zport->regs[5] &= ~SND_BRK;
	write_zsreg(zport, R5, zport->regs[5]);
	spin_unlock_irqrestore(&scc->zlock, flags);
}


/*
 * Interrupt handling routines.
 */
#define Rx_BRK 0x0100			/* BREAK event software flag.  */
#define Rx_SYS 0x0200			/* SysRq event software flag.  */

static void zs_receive_chars(struct zs_port *zport)
{
	struct uart_port *uport = &zport->port;
	struct zs_scc *scc = zport->scc;
	struct uart_icount *icount;
	unsigned int avail, status;
	int count;
	u8 ch, flag;

	for (count = 16; count; count--) {
		spin_lock(&scc->zlock);
		avail = read_zsreg(zport, R0) & Rx_CH_AV;
		spin_unlock(&scc->zlock);
		if (!avail)
			break;

		spin_lock(&scc->zlock);
		status = read_zsreg(zport, R1) & (Rx_OVR | FRM_ERR | PAR_ERR);
		ch = read_zsdata(zport);
		spin_unlock(&scc->zlock);

		flag = TTY_NORMAL;

		icount = &uport->icount;
		icount->rx++;

		/* Handle the null char got when BREAK is removed.  */
		if (!ch)
			status |= zport->tty_break;
		if (unlikely(status &
			     (Rx_OVR | FRM_ERR | PAR_ERR | Rx_SYS | Rx_BRK))) {
			zport->tty_break = 0;

			/* Reset the error indication.  */
			if (status & (Rx_OVR | FRM_ERR | PAR_ERR)) {
				spin_lock(&scc->zlock);
				write_zsreg(zport, R0, ERR_RES);
				spin_unlock(&scc->zlock);
			}

			if (status & (Rx_SYS | Rx_BRK)) {
				icount->brk++;
				/* SysRq discards the null char.  */
				if (status & Rx_SYS)
					continue;
			} else if (status & FRM_ERR)
				icount->frame++;
			else if (status & PAR_ERR)
				icount->parity++;
			if (status & Rx_OVR)
				icount->overrun++;

			status &= uport->read_status_mask;
			if (status & Rx_BRK)
				flag = TTY_BREAK;
			else if (status & FRM_ERR)
				flag = TTY_FRAME;
			else if (status & PAR_ERR)
				flag = TTY_PARITY;
		}

		if (uart_handle_sysrq_char(uport, ch))
			continue;

		uart_insert_char(uport, status, Rx_OVR, ch, flag);
	}

	tty_flip_buffer_push(&uport->state->port);
}

static void zs_raw_transmit_chars(struct zs_port *zport)
{
	struct tty_port *tport = &zport->port.state->port;
	unsigned char ch;

	/* XON/XOFF chars.  */
	if (zport->port.x_char) {
		write_zsdata(zport, zport->port.x_char);
		zport->port.icount.tx++;
		zport->port.x_char = 0;
		return;
	}

	/* If nothing to do or stopped or hardware stopped.  */
	if (uart_tx_stopped(&zport->port) ||
			!uart_fifo_get(&zport->port, &ch)) {
		zs_raw_stop_tx(zport);
		return;
	}

	/* Send char.  */
	write_zsdata(zport, ch);

	if (kfifo_len(&tport->xmit_fifo) < WAKEUP_CHARS)
		uart_write_wakeup(&zport->port);

	/* Are we are done?  */
	if (kfifo_is_empty(&tport->xmit_fifo))
		zs_raw_stop_tx(zport);
}

static void zs_transmit_chars(struct zs_port *zport)
{
	struct zs_scc *scc = zport->scc;

	spin_lock(&scc->zlock);
	zs_raw_transmit_chars(zport);
	spin_unlock(&scc->zlock);
}

static void zs_status_handle(struct zs_port *zport)
{
	struct uart_port *uport = &zport->port;
	struct zs_scc *scc = zport->scc;
	unsigned int delta;
	u8 status, brk;

	spin_lock(&scc->zlock);

	/* Get status from Read Register 0.  */
	status = read_zsreg(zport, R0);

	if (zport->regs[15] & BRKIE) {
		brk = status & BRK_ABRT;
		if (brk && !zport->brk) {
			spin_unlock(&scc->zlock);
			if (uart_handle_break(uport))
				zport->tty_break = Rx_SYS;
			else
				zport->tty_break = Rx_BRK;
			spin_lock(&scc->zlock);
		}
		zport->brk = brk;
	}

	delta = zs_rr0_to_mctrl(status) ^ zport->mctrl;
	if (delta)
		zport->mctrl = zs_rr0_to_mctrl(status);

	/* Clear the status condition...  */
	write_zsreg(zport, R0, RES_EXT_INT);

	spin_unlock(&scc->zlock);

	if (delta & TIOCM_CTS)
		uart_handle_cts_change(uport, zport->mctrl & TIOCM_CTS);
	if (delta & TIOCM_CAR)
		uart_handle_dcd_change(uport, zport->mctrl & TIOCM_CAR);
	if (delta & TIOCM_DSR)
		uport->icount.dsr++;

	if (delta)
		wake_up_interruptible(&uport->state->port.delta_msr_wait);
}

/*
 * This is the Z85C30 driver's generic interrupt routine.
 */
static irqreturn_t zs_interrupt(int irq, void *dev_id)
{
	struct zs_scc *scc = dev_id;
	struct zs_port *zport_a = &scc->zport[ZS_CHAN_A];
	struct zs_port *zport_b = &scc->zport[ZS_CHAN_B];
	irqreturn_t status = IRQ_NONE;
	u8 zs_intreg;
	int count;

	/*
	 * NOTE: The read register 3, which holds the irq status,
	 *       does so for both channels on each chip.  Although
	 *       the status value itself must be read from the A
	 *       channel and is only valid when read from channel A.
	 */
	for (count = 16; count; count--) {
		spin_lock(&scc->zlock);
		zs_intreg = read_zsreg(zport_a, R3);
		spin_unlock(&scc->zlock);
		if (!zs_intreg)
			break;

		/*
		 * We do not like losing characters, so we prioritise
		 * interrupt sources a little bit differently than
		 * the SCC would, was it allowed to.
		 */
		if (zs_intreg & CHBRxIP)
			zs_receive_chars(zport_b);
		if (zs_intreg & CHARxIP)
			zs_receive_chars(zport_a);
		if (zs_intreg & CHBEXT)
			zs_status_handle(zport_b);
		if (zs_intreg & CHAEXT)
			zs_status_handle(zport_a);
		if (zs_intreg & CHBTxIP)
			zs_transmit_chars(zport_b);
		if (zs_intreg & CHATxIP)
			zs_transmit_chars(zport_a);

		status = IRQ_HANDLED;
	}

	return status;
}


/*
 * Finally, routines used to initialize the serial port.
 */
static int zs_startup(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	unsigned long flags;
	int irq_guard;
	int ret;

	irq_guard = atomic_add_return(1, &scc->irq_guard);
	if (irq_guard == 1) {
		ret = request_irq(uport->irq, zs_interrupt,
				  IRQF_SHARED, "next-scc", scc);
		if (ret) {
			atomic_add(-1, &scc->irq_guard);
			pr_err("next-zs: can't get irq %d\n", uport->irq);
			return ret;
		}
	}

	spin_lock_irqsave(&scc->zlock, flags);

	/* Clear the receive FIFO.  */
	zs_receive_drain(zport);

	/* Clear the interrupt registers.  */
	write_zsreg(zport, R0, ERR_RES);
	write_zsreg(zport, R0, RES_Tx_P);
	/* But Ext only if not being handled already.  */
	if (!(zport->regs[1] & EXT_INT_ENAB))
		write_zsreg(zport, R0, RES_EXT_INT);

	/* Finally, enable sequencing and interrupts.  */
	zport->regs[1] &= ~RxINT_MASK;
	zport->regs[1] |= RxINT_ALL | TxINT_ENAB | EXT_INT_ENAB;
	zport->regs[3] |= RxENABLE;
	zport->regs[15] |= BRKIE;
	write_zsreg(zport, R1, zport->regs[1]);
	write_zsreg(zport, R3, zport->regs[3]);
	write_zsreg(zport, R5, zport->regs[5]);
	write_zsreg(zport, R15, zport->regs[15]);

	/* Record the current state of RR0.  */
	zport->mctrl = zs_raw_get_mctrl(zport);
	zport->brk = read_zsreg(zport, R0) & BRK_ABRT;

	zport->tx_stopped = 1;

	spin_unlock_irqrestore(&scc->zlock, flags);

	return 0;
}

static void zs_shutdown(struct uart_port *uport)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	unsigned long flags;
	int irq_guard;

	spin_lock_irqsave(&scc->zlock, flags);

	zport->regs[1] &= ~(RxINT_MASK | TxINT_ENAB | EXT_INT_ENAB);
	zport->regs[3] &= ~RxENABLE;
	zport->regs[15] &= ~(BRKIE | DCDIE | CTSIE);
	write_zsreg(zport, R1, zport->regs[1]);
	write_zsreg(zport, R5, zport->regs[5]);
	write_zsreg(zport, R3, zport->regs[3]);
	write_zsreg(zport, R15, zport->regs[15]);

	spin_unlock_irqrestore(&scc->zlock, flags);

	irq_guard = atomic_add_return(-1, &scc->irq_guard);
	if (!irq_guard)
		free_irq(uport->irq, scc);
}


static void zs_reset(struct zs_port *zport)
{
	struct zs_port *zport_a = &zport->scc->zport[ZS_CHAN_A];
	struct zs_scc *scc = zport->scc;
	int irq;
	unsigned long flags;

	spin_lock_irqsave(&scc->zlock, flags);
	irq = !irqs_disabled_flags(flags);

	/* Reset the pointer first, just in case...  */
	read_zsreg(zport, R0);
	/* And let the current transmission finish.  */
	zs_line_drain(zport, irq);
	write_zsreg(zport, R9, zport == zport_a ? CHRA : CHRB);
	udelay(10);
	write_zsreg(zport, R9, 0);

	load_zsregs(zport, zport->regs, irq);
	spin_unlock_irqrestore(&scc->zlock, flags);
}

static void zs_set_termios(struct uart_port *uport, struct ktermios *termios,
			   const struct ktermios *old_termios)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	int irq;
	unsigned int baud, brg;
	unsigned long flags;

	spin_lock_irqsave(&scc->zlock, flags);
	irq = !irqs_disabled_flags(flags);

	/* Byte size.  */
	zport->regs[3] &= ~RxNBITS_MASK;
	zport->regs[5] &= ~TxNBITS_MASK;
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		zport->regs[3] |= Rx5;
		zport->regs[5] |= Tx5;
		break;
	case CS6:
		zport->regs[3] |= Rx6;
		zport->regs[5] |= Tx6;
		break;
	case CS7:
		zport->regs[3] |= Rx7;
		zport->regs[5] |= Tx7;
		break;
	case CS8:
	default:
		zport->regs[3] |= Rx8;
		zport->regs[5] |= Tx8;
		break;
	}

	/* Parity and stop bits.  */
	zport->regs[4] &= ~(XCLK_MASK | SB_MASK | PAR_ENA | PAR_EVEN);
	if (termios->c_cflag & CSTOPB)
		zport->regs[4] |= SB2;
	else
		zport->regs[4] |= SB1;
	if (termios->c_cflag & PARENB)
		zport->regs[4] |= PAR_ENA;
	if (!(termios->c_cflag & PARODD))
		zport->regs[4] |= PAR_EVEN;
	switch (zport->clk_mode) {
	case 64:
		zport->regs[4] |= X64CLK;
		break;
	case 32:
		zport->regs[4] |= X32CLK;
		break;
	case 16:
		zport->regs[4] |= X16CLK;
		break;
	case 1:
		zport->regs[4] |= X1CLK;
		break;
	default:
		BUG();
	}

	baud = uart_get_baud_rate(uport, termios, old_termios, 0,
				  uport->uartclk / zport->clk_mode / 4);

	brg = ZS_BPS_TO_BRG(baud, uport->uartclk / zport->clk_mode);
	zport->regs[12] = brg & 0xff;
	zport->regs[13] = (brg >> 8) & 0xff;

	uart_update_timeout(uport, termios->c_cflag, baud);

	uport->read_status_mask = Rx_OVR;
	if (termios->c_iflag & INPCK)
		uport->read_status_mask |= FRM_ERR | PAR_ERR;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		uport->read_status_mask |= Rx_BRK;

	uport->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		uport->ignore_status_mask |= FRM_ERR | PAR_ERR;
	if (termios->c_iflag & IGNBRK) {
		uport->ignore_status_mask |= Rx_BRK;
		if (termios->c_iflag & IGNPAR)
			uport->ignore_status_mask |= Rx_OVR;
	}

	if (termios->c_cflag & CREAD)
		zport->regs[3] |= RxENABLE;
	else
		zport->regs[3] &= ~RxENABLE;

	/* Keep DTR/RTS in sync with the generic serial core state.  */
	if (uport->mctrl & TIOCM_DTR)
		zport->regs[5] |= DTR;
	else
		zport->regs[5] &= ~DTR;
	if (uport->mctrl & TIOCM_RTS)
		zport->regs[5] |= RTS;
	else
		zport->regs[5] &= ~RTS;

	if (!(termios->c_cflag & CLOCAL))
		zport->regs[15] |= DCDIE;
	else
		zport->regs[15] &= ~DCDIE;
	if (termios->c_cflag & CRTSCTS)
		zport->regs[15] |= CTSIE;
	else
		zport->regs[15] &= ~CTSIE;
	zs_raw_xor_mctrl(zport);

	/* Load up the new values.  */
	load_zsregs(zport, zport->regs, irq);

	spin_unlock_irqrestore(&scc->zlock, flags);
}


static const char *zs_type(struct uart_port *uport)
{
	return "Z85C30 SCC";
}

/*
 * The SCC mapping is owned by the platform device and is set up before
 * the port is registered, so there is nothing to do here.  In particular
 * do not clear membase, as the serial core may call request_port() again
 * after a user requested reconfiguration.
 */
static void zs_release_port(struct uart_port *uport)
{
}

static int zs_request_port(struct uart_port *uport)
{
	return 0;
}

static void zs_config_port(struct uart_port *uport, int flags)
{
	struct zs_port *zport = to_zport(uport);

	if (flags & UART_CONFIG_TYPE) {
		if (zs_request_port(uport))
			return;

		uport->type = PORT_ZS;

		zs_reset(zport);
	}
}

static int zs_verify_port(struct uart_port *uport, struct serial_struct *ser)
{
	struct zs_port *zport = to_zport(uport);
	int ret = 0;

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_ZS)
		ret = -EINVAL;
	if (ser->irq != uport->irq)
		ret = -EINVAL;
	if (ser->baud_base != uport->uartclk / zport->clk_mode / 4)
		ret = -EINVAL;
	return ret;
}


static const struct uart_ops zs_ops = {
	.tx_empty	= zs_tx_empty,
	.set_mctrl	= zs_set_mctrl,
	.get_mctrl	= zs_get_mctrl,
	.stop_tx	= zs_stop_tx,
	.start_tx	= zs_start_tx,
	.stop_rx	= zs_stop_rx,
	.enable_ms	= zs_enable_ms,
	.break_ctl	= zs_break_ctl,
	.startup	= zs_startup,
	.shutdown	= zs_shutdown,
	.set_termios	= zs_set_termios,
	.type		= zs_type,
	.release_port	= zs_release_port,
	.request_port	= zs_request_port,
	.config_port	= zs_config_port,
	.verify_port	= zs_verify_port,
};

/*
 * Initialize Z85C30 port structures.
 */
static int __init zs_probe(struct platform_device *pdev)
{
	struct resource *mem_resource, *irq_resource;
	struct uart_port *uport;
	int chip, side, i, ret;

	mem_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq_resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!mem_resource || !irq_resource)
		return -ENODEV;

	chip = pdev->id;
	if (chip < 0 || chip >= ZS_NUM_SCCS)
		return -ENODEV;

	zs_membase = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(zs_membase))
		return PTR_ERR(zs_membase);

	spin_lock_init(&zs_sccs[chip].zlock);
	for (side = 0; side < ZS_NUM_CHAN; side++) {
		struct zs_port *zport = &zs_sccs[chip].zport[side];

		uport = &zport->port;
		zport->scc	= &zs_sccs[chip];
		zport->clk_mode	= 16;

		uport->dev	= &pdev->dev;
		uport->has_sysrq = IS_ENABLED(CONFIG_SERIAL_NEXT_ZS_CONSOLE);
		uport->irq	= irq_resource->start;
		uport->uartclk	= ZS_CLOCK;
		uport->fifosize	= 1;
		uport->iotype	= UPIO_MEM;
		uport->flags	= UPF_BOOT_AUTOCONF;
		uport->ops	= &zs_ops;
		uport->line	= chip * ZS_NUM_CHAN + side;
		uport->mapbase	= mem_resource->start +
				  (side ^ ZS_CHAN_B) * ZS_CHAN_ADDR_STRIDE;
		uport->membase	= zs_membase +
				  (side ^ ZS_CHAN_B) * ZS_CHAN_ADDR_STRIDE;

		for (i = 0; i < ZS_NUM_REGS; i++)
			zport->regs[i] = zs_init_regs[i];

		ret = uart_add_one_port(&zs_reg, uport);
		if (ret) {
			uport->dev = NULL;
			for (i = side - 1; i >= 0; i--) {
				struct uart_port *prev =
					&zs_sccs[chip].zport[i].port;

				uart_remove_one_port(&zs_reg, prev);
				prev->dev = NULL;
			}
			return ret;
		}
	}

	return 0;
}

static void __exit zs_remove(struct platform_device *pdev)
{
	int chip, side;

	chip = pdev->id;
	if (chip < 0 || chip >= ZS_NUM_SCCS)
		return;

	for (side = ZS_NUM_CHAN - 1; side >= 0; side--) {
		struct zs_port *zport = &zs_sccs[chip].zport[side];
		struct uart_port *uport = &zport->port;

		if (uport->dev)
			uart_remove_one_port(&zs_reg, uport);
	}
}


#ifdef CONFIG_SERIAL_NEXT_ZS_CONSOLE
static void zs_console_putchar(struct uart_port *uport, unsigned char ch)
{
	struct zs_port *zport = to_zport(uport);
	struct zs_scc *scc = zport->scc;
	int irq;
	unsigned long flags;

	spin_lock_irqsave(&scc->zlock, flags);
	irq = !irqs_disabled_flags(flags);
	if (zs_transmit_drain(zport, irq))
		write_zsdata(zport, ch);
	spin_unlock_irqrestore(&scc->zlock, flags);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 */
static void zs_console_write(struct console *co, const char *s,
			     unsigned int count)
{
	int chip, side;
	struct zs_port *zport;
	struct zs_scc *scc;
	unsigned long flags;
	u8 txint, txenb;
	int irq;

	if (co->index < 0 || co->index >= ZS_NUM_SCCS * ZS_NUM_CHAN)
		return;

	chip = co->index / ZS_NUM_CHAN;
	side = co->index % ZS_NUM_CHAN;
	zport = &zs_sccs[chip].zport[side];
	scc = zport->scc;

	if (!scc)
		return;

	/* Disable transmit interrupts and enable the transmitter. */
	spin_lock_irqsave(&scc->zlock, flags);
	txint = zport->regs[1];
	txenb = zport->regs[5];
	if (txint & TxINT_ENAB) {
		zport->regs[1] = txint & ~TxINT_ENAB;
		write_zsreg(zport, R1, zport->regs[1]);
	}
	if (!(txenb & TxENAB)) {
		zport->regs[5] = txenb | TxENAB;
		write_zsreg(zport, R5, zport->regs[5]);
	}
	spin_unlock_irqrestore(&scc->zlock, flags);

	uart_console_write(&zport->port, s, count, zs_console_putchar);

	/* Restore transmit interrupts and the transmitter enable. */
	spin_lock_irqsave(&scc->zlock, flags);
	irq = !irqs_disabled_flags(flags);
	zs_line_drain(zport, irq);
	if (!(txenb & TxENAB)) {
		zport->regs[5] &= ~TxENAB;
		write_zsreg(zport, R5, zport->regs[5]);
	}
	if (txint & TxINT_ENAB) {
		zport->regs[1] |= TxINT_ENAB;
		write_zsreg(zport, R1, zport->regs[1]);

		/* Resume any transmission as the TxIP bit won't be set.  */
		if (!zport->tx_stopped)
			zs_raw_transmit_chars(zport);
	}
	spin_unlock_irqrestore(&scc->zlock, flags);
}

/*
 * Setup serial console baud/bits/parity.  We do two things here:
 * - construct a cflag setting for the first uart_open()
 * - initialise the serial port
 * Return non-zero if we didn't find a serial port.
 */
static int __init zs_console_setup(struct console *co, char *options)
{
	int chip, side;
	struct zs_port *zport;
	struct uart_port *uport;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= ZS_NUM_SCCS * ZS_NUM_CHAN)
		co->index = 0;

	chip = co->index / ZS_NUM_CHAN;
	side = co->index % ZS_NUM_CHAN;
	zport = &zs_sccs[chip].zport[side];
	uport = &zport->port;

	if (!zport->scc)
		return -ENODEV;
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	return uart_set_options(uport, co, baud, parity, bits, flow);
}

static struct console zs_console = {
	.name	= "ttyS",
	.write	= zs_console_write,
	.device	= uart_console_device,
	.setup	= zs_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data	= &zs_reg,
};

#define SERIAL_NEXT_ZS_CONSOLE	(&zs_console)
#else
#define SERIAL_NEXT_ZS_CONSOLE	NULL
#endif /* CONFIG_SERIAL_NEXT_ZS_CONSOLE */

static struct uart_driver zs_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "serial_next_zs",
	.dev_name		= "ttyS",
	.major			= TTY_MAJOR,
	.minor			= 64,
	.nr			= ZS_NUM_SCCS * ZS_NUM_CHAN,
	.cons			= SERIAL_NEXT_ZS_CONSOLE,
};

static struct platform_driver zs_driver = {
	.remove = __exit_p(zs_remove),
	.driver = { .name = "next-zs" },
};

static struct platform_device *zs_platform_device;

/* zs_init inits the driver. */
static int __init zs_init(void)
{
	struct resource res[2];
	int ret;

	pr_info("%s%s\n", zs_name, zs_version);

	memset(res, 0, sizeof(res));
	res[0].name = "scc";
	res[0].start = NEXT_SCC;
	res[0].end = NEXT_SCC + ZS_SCC_IO_SIZE - 1;
	res[0].flags = IORESOURCE_MEM;
	res[1].name = "scc";
	res[1].start = NEXT_IRQ_SCC;
	res[1].end = NEXT_IRQ_SCC;
	res[1].flags = IORESOURCE_IRQ;

	ret = uart_register_driver(&zs_reg);
	if (ret)
		return ret;

	zs_platform_device = platform_device_register_simple("next-zs", 0,
							     res,
							     ARRAY_SIZE(res));
	if (IS_ERR(zs_platform_device)) {
		ret = PTR_ERR(zs_platform_device);
		goto err_unregister_driver;
	}

	ret = platform_driver_probe(&zs_driver, zs_probe);
	if (ret)
		goto err_unregister_device;

	return 0;

err_unregister_device:
	platform_device_unregister(zs_platform_device);
err_unregister_driver:
	uart_unregister_driver(&zs_reg);
	return ret;
}

static void __exit zs_exit(void)
{
	platform_driver_unregister(&zs_driver);
	uart_unregister_driver(&zs_reg);
	platform_device_unregister(zs_platform_device);
}

module_init(zs_init);
module_exit(zs_exit);

MODULE_ALIAS_CHARDEV_MAJOR(TTY_MAJOR);
