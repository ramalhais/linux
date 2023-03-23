// SPDX-License-Identifier: GPL-2.0

/* mb8795.c driver for onboard ether on NeXT machines
 *
 *      Derived from code:
 *
 *      Copyright 1993 United States Government as represented by the
 *      Director, National Security Agency.
 *
 *      This software may be used and distributed according to the terms
 *      of the GNU Public License, incorporated herein by reference.
 */

/*
 *  Sources:
 *      Derived from vague chip descriptions from an IBM
 *      'Baseband Adapter' manual and much bashing of foreheads
 *	against the next hardware.  Lots of other linux drivers
 *	consulted as well.
 *
 *	Named mb8795 as thats the supposed chip name of the Fujitsu
 *	part used, but I must be blind.. I can't find it on the boards :(
 */

// This driver is far from done, but I'm tired of wrestling
// with the dma silliness.  things to do:
// - pay attention to error conditions (service ints, reset, etc)
// - stop() for real
// - modular
// - dma into properly alligned skbs
// - handle first packet if we miss the first chain int
// - keep full statistics, byte counters, etc

// PR
// Send:
// linux -> start_xmit -> setup packet in memory buffer (DMA) and notify adapter
// adapter reads from memory buffer (DMA), sends packet out and triggers IRQ to notify driver it is sent -> txdmaint

// Receive:
// adapter writes received packet to memory (DMA) and triggers IRQ to notify driver there is a packet waiting to be received -> mb8795_rxdmaint
// driver reads packet from memory (DMA)-> linux

#define DRV_NAME "mb8795"

#include <linux/etherdevice.h>
#include <linux/platform_device.h>

#include <asm/nexthw.h>
#include <asm/nextints.h>

// #define DEBUGME_TX
// #define DEBUGME_RX

// #define DEBUGME_FUNC

#ifndef DEBUGME
# if defined(DEBUGME_TX) || defined(DEBUGME_RX)
#  define DEBUGME
# endif
#endif

struct mb8795regs {
	volatile u8 txstat;
	volatile u8 txmask;
	volatile u8 rxstat;
	volatile u8 rxmask;
	volatile u8 txmode;
	volatile u8 rxmode;
	volatile u8 reset;
	volatile u8 txcntlow;
	volatile u8 eaddr[6];
	volatile u8 reserved_dont_use[1];
	volatile u8 txcnthi;
};

// TX status/mask
#define TSTAT_PARERR	0x01
#define TSTAT_16COL	0x02
#define TSTAT_COLL	0x04
#define TSTAT_UNDERFLOW	0x08
#define TSTAT_COXSHORT	0x10
#define TSTAT_TXDONE	0x20
#define TSTAT_BUSY	0x40
#define TSTAT_TXAVAIL	0x80
#define TSTAT_CLEAR	0xff

// RX status/mask
#define RSTAT_OVERFLOW	0x01
#define RSTAT_CRC	0x02
#define RSTAT_ALIGN	0x04
#define RSTAT_RUNT	0x08
#define RSTAT_RESET	0x10	/* received a reset packet ? */
#define RSTAT_PRECV	0x80	/* packet received */
#define RSTAT_CLEAR	0xff

/* bits in reset */
// #define RST_RESET	0x8
#define RST_RESET	0x80 // Acording to Previous emulator and netbsd it should be 0x80

/* bits in txmode */
#define TXM_LOOP_DISABLE	0x02 // Turbo: Enable loop
#define TXM_TURBO		0x04 // Turbo: Select Twisted Pair?
#define TXM_TURBOSTART		0x80 // Turbo: Enable
#define TXM_NCOL_MASK		0xf0

/* bits in rxmode */
#define RXM_DISABLE	0x00
#define RXM_ACCEPT	0x01 // limited. broadcast. turbo: Accept any packets
#define RXM_MULTICAST	0x02 // normal. broadcast+multicast. turbo: Accept own packets
#define RXM_TURBO	0x80 // turbo: Accept packets
#define RXM_PROMISC	(RXM_ACCEPT|RXM_MULTICAST)
// #define RXM_PROMISC 0x11

#define NRXBUFS 8
/* must be a power of 2 */
#define NEXT_ALIGN 32

#define MB_MAGIC_PADDING 15
#define	MB_RX_BOP	0x40000000	// RX Beginning Of Packet
#define	MB_RX_EOP	0x80000000	// RX End Of Packet
#define MB_TX_EOP	0x80000000	// TX End Of Packet

#define TXBUFLEN (1514)
#define MAXTXBYTES (1514+4+2*NEXT_ALIGN)
// must have len be !(%NEXT_ALIGN) so the end addr is
#define RXBUFLEN (MAXTXBYTES-(MAXTXBYTES%NEXT_ALIGN))

// #define NEXT_RXBUF(x) ((x+1)%NRXBUFS)

// #define MAX_DMASIZE 4096
// #define	DMA_ENDALIGNMENT	16	// DMA must start(Previous) and end on quad longword //default
// #define ENDMA_ENDALIGNMENT	32	// Ethernet DMA is very special //TX
// #define	DMA_ENDALIGN(type, addr) ((type)(((unsigned)(addr)+DMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1))) // default
// #define	ENDMA_ENDALIGN(type, addr) ((type)((((unsigned)(addr)+ENDMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1))|0x80000000)) // TX with end of packet bit?

struct mb8795_private {
	struct platform_device *pdev;
	struct net_device *ndev;

	bool is_turbo;

	// Ugly hack to attempt to pass unique dev_id to shared IRQs, so that it finds the correct handler on free_irq(). Probably not working because we need to pass the pointer (&) to these :( We need a NeXT IRQ driver
	struct mb8795_private *irq_rx;
	struct mb8795_private *irq_tx;
	struct mb8795_private *irq_rx_dma;
	struct mb8795_private *irq_tx_dma;

	struct mb8795regs *mb;
	struct next_dma_channel *rxdma;
	struct next_dma_channel *txdma;

	unsigned char *txbuf;
	u32 p_txbuf;
	u32 txlen;

	struct rxb {
		struct sk_buff *skb;
		u32 p_data;
		u32 len;
		struct rxb *next;
	} rxbufs[NRXBUFS];
	struct rxb *cur_rxb;	/* the buffers in use by the dma */

	struct net_device_stats stats;
};

#ifdef DEBUGME
void	dumpregs(struct mb8795regs *mb)
{
	pr_info("txstat=0x%0x txmask=0x%0x", mb->txstat, mb->txmask);
	pr_info("rxstat=0x%0x rxmask=0x%0x", mb->rxstat, mb->rxmask);
	pr_info("txmode=0x%0x rxmode=0x%0x", mb->txmode, mb->rxmode);
	pr_info("reset=0x%0x\n ", mb->reset);
};

// void hdump(unsigned char *d,int len) {
// 	if (len > 32) {
// 		len = 32;
// 	}

// 	for(; len; len--) {
// 		printk("%02x ",*d);
// 		d++;
// 	};
// }

void dumpdmaregs(void *ptr, bool is_turbo)
{
	struct next_dma_channel *rxd = (struct next_dma_channel *)ptr;

	pr_info("csr=0x%x", rxd->csr);
	if (!is_turbo) {
		pr_info("saved_start=0x%x saved_end=0x%x",		rxd->saved_start,	rxd->saved_end);
		pr_info("saved_next_start=0x%x saved_next_end=0x%x",	rxd->saved_next_start,	rxd->saved_next_end);
	} else {
		pr_info("turbo_rx_saved_start=0x%x",	rxd->turbo_rx_saved_start);
	}
	pr_info("start=0x%x end=0x%x",			rxd->start,		rxd->end);
	pr_info("next_start=0x%x next_end=0x%x",	rxd->next_start,	rxd->next_end);
	pr_info("next_initbuf=0x%x",			rxd->next_initbuf);
};
#endif

/* XXX check for return of NULL :( */
static inline struct sk_buff *mb_new_skb(struct net_device *ndev)
{
	struct sk_buff *newskb;
	unsigned int fixup;

	// newskb=dev_alloc_skb(RXBUFLEN);
	newskb = netdev_alloc_skb(ndev, RXBUFLEN);
	fixup = (unsigned int)newskb->data&(NEXT_ALIGN-1);
	if (fixup)
		skb_reserve(newskb, NEXT_ALIGN-fixup);

	return newskb;
}

static inline void handle_packet(struct mb8795_private *priv, struct net_device *ndev, struct rxb *rx)
{
	int len = rx->len;
	struct sk_buff *skb = rx->skb;

	cache_clear(rx->p_data, len);
	skb->dev = ndev;
	skb_put(skb, len);
	skb->protocol = eth_type_trans(skb, ndev);

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += len;

	netif_rx(skb);

	rx->skb = mb_new_skb(ndev);
	rx->p_data = virt_to_phys(rx->skb->data);
	rx->len = 0;
}

//  no rx ints are being allowed for now...
static irqreturn_t mb8795_rxint(int irq, void *dev_id)
{
	unsigned long flags;
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_ENETR)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }

#ifdef DEBUGME_RX
	pr_info("\nRX int: rxstat=0x%x", mb->rxstat);
#endif

	if (mb->rxstat&RSTAT_PRECV)
		mb->rxstat = RSTAT_PRECV; // is this like an acknowledgement? or should it be cleared(0) or 0xff?

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static void setup_rxdma(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct rxb *rx = priv->cur_rxb;

	// init the rx dma buffer pointers

	rxd->csr = 0;
	rxd->csr = DMA_RESET|DMA_SETTMEM|(priv->is_turbo ? DMA_INITDMA_TURBO : DMA_INITDMA);

	if (!priv->is_turbo) {
		// Probably not needed
		// rxd->saved_start = 0;//dd_saved_next
		// rxd->saved_end = 0;//dd_saved_limit
		// rxd->saved_next_start = 0;//dd_saved_start
		// rxd->saved_next_end = 0;//dd_saved_stop
	} else {
		// rxd->saved_start = rx->p_data; // this blows up on turbo in Previous.
	}

	rxd->start = rx->p_data;//dd_next
	rxd->end = rx->p_data+RXBUFLEN;//dd_limit

	rx = rx->next;

	// Set this up only if setting DMA_SETCHAIN in rxd->csr (chained DMA)
	rxd->next_start = rx->p_data;//dd_start // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts
	rxd->next_end = rx->p_data+RXBUFLEN;//dd_stop // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts

	rxd->csr = DMA_SETENABLE|DMA_SETTMEM|DMA_SETCHAIN;
}

// We set up the dma engine to start recieving the next
// packet while we go off and queue the packet that
// this int was for. this way we find another chain
// int waiting for us when we're done with this one

// The dma handling here is _very_ touchy.  mis stepping can cause really
// wacked garbage like turning on loopback.
// No, I don't know either.
static irqreturn_t mb8795_rxdmaint(int irq, void *dev_id)
{
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct rxb *rx = (struct rxb *)priv->cur_rxb;
	u32 csr;
	unsigned long flags;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_ENETR_DMA)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }

#ifdef DEBUGME_RX
	pr_info("\nRX DMA int: rxstat=0x%x ", priv->mb->rxstat);
	dumpdmaregs(priv->rxdma, priv->is_turbo);
#endif
	// Perhaps should check chip status sometime to look for rx errors

	csr = rxd->csr;

	// netbsd
	// if ((csr&DMA_ENABLED) == 0) {
	// 	rxd->csr = DMA_RESET|DMA_CLEARCHAINI;
	// 	goto bail;
	// }
	// if (csr&DMA_CINT) {
	// 	rxd->csr = DMA_CLEARCHAINI;
	// }
	// if ((csr&DMA_CINT) == 0 ||
	//     (priv->rxstat & RSTAT_PRECV) == 0) {
	// 	goto bail;
	// }

	if(!(priv->mb->rxstat&RSTAT_PRECV)) {
		// setup_rxdma(priv->ndev); // try this?
		goto bail;
	}

	if (csr&DMA_CINT && csr&DMA_ENABLED) {
		// Chain interrupt, we have another buffer yet
		rxd->csr = DMA_CLEARCHAINI;

		// if (priv->is_turbo) {
		// 	// gotpkt = rxd->saved_start/*dd_saved_next*/;
		// 	// rx->len = rxd->start/*dd_next*/ - rxd->saved_start/*dd_saved_next*/; //saved_start will blow up on turbo;
		// 	// rx->len = rxd->start/*dd_next*/ - (rx->p_data)/*rxd->saved_start is dd_saved_next*/; //saved_start will blow up on turbo;
		// 	rx->len = rxd->turbo_rx_saved_start - rx->p_data - 4; // -4 is the checksum?
		// } else {
		// 	// old code worked for non-turbo:
		// 	// rx->len= rxd->saved_end - rxd->saved_start - 4; // -4? struct packing problem? Previous bug? NeXT Machine type differences?
		// 	rx->len = rxd->saved_end/*dd_saved_limit*/ - rx->p_data - 4; // hack: For some reason Previous is setting saved_start to zero, so we get the physical start from rx->p_data.

		// 	// new code for non-turbo is the same for turbo? WTF? use old code until fixed
		// 	// gotpkt = rxd->saved_start/*dd_saved_next*/;
		// 	// rx->len = rxd->start/*dd_next*/ - (rx->p_data)/*rxd->saved_start is dd_saved_next*/;
		// }

		rx->len = (priv->is_turbo ? rxd->turbo_rx_saved_start : rxd->saved_end) - rx->p_data - 4; // -4 is the checksum?
#ifdef DEBUGME_RX
		pr_info("CINT1 rx->p_data=0x%x rx->len=0x%x (%d) ", rx->p_data, rx->len, rx->len);
#endif

		handle_packet(priv, priv->ndev, rx);
	} else {
		// if we missed the first chained int we'll
		// have a waiting packet in the 'first' slot
		// but aren't currently dealing with it. Fix
		rx = rx->next;
		rx->len = rxd->start - rxd->next_start - 4;// FIXME: this is probably wrong! Seems always correct after all. WTF
#ifdef DEBUGME_RX
		pr_info("CINT2 rx->p_data=0x%x rx->len=0x%x (%d) ", rx->p_data, rx->len, rx->len);
#endif
		if (csr&DMA_CINT)
			rxd->csr = DMA_CLEARCHAINI;
		priv->cur_rxb = rx->next;
		setup_rxdma(priv->ndev);
		handle_packet(priv, priv->ndev, rx);
		// priv->stats.rx_packets++;// already done in handle_packet?
	}

#ifdef DEBUGME_RX
	// dumpdmaregs(priv->rxdma, priv->is_turbo);
#endif

bail:
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static irqreturn_t mb8795_txint(int irq, void *dev_id)
{
	unsigned long flags;
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_ENETX)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }

#ifdef DEBUGME_TX
	pr_info("\nTX int: txstat=0x%x", priv->mb->txstat);
	dumpdmaregs(priv->txdma, priv->is_turbo);
#endif
	if (priv->mb->txstat&TSTAT_COLL) {
		priv->stats.collisions++;
		priv->mb->txstat = TSTAT_COLL;
	}

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

// This sucks.  Needs to be much brighter at realizing that the chip / dma
// might be wedged and paying attention to the fate of the packet we just sent
// out might be wise
static irqreturn_t mb8795_txdmaint(int irq, void *dev_id)
{
	unsigned long flags;
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct next_dma_channel *txd = (struct next_dma_channel *)priv->txdma;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_ENETX_DMA)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }

#ifdef DEBUGME_TX
	pr_info("TX DMA int: txstat=0x%x ", priv->mb->txstat);
	dumpdmaregs(txd, priv->is_turbo);
#endif

	// We're in an interrupt?
	// dev->interrupt=1;
	if (priv->mb->txstat&(TSTAT_TXAVAIL|TSTAT_TXDONE)) {
		priv->mb->txstat = TSTAT_CLEAR;
		priv->mb->txmask = 0;
	}

	// netbsd
	// if (state & (DMACSR_COMPLETE|DMACSR_BUSEXC)) {
	// 	txdma->dd_csr = DMACSR_RESET | DMACSR_CLRCOMPLETE;
	// }

	txd->csr = DMA_RESET/*|DMA_CLEARCHAINI*/; // DMA_CLEARCHAINI breaks DMA turbo in Previous

	// netbsd. sucess only if no collision
	// if ((txs & EN_TXS_COLLERR) == 0)
	// 		return len;

	priv->stats.tx_packets++;
	priv->stats.tx_bytes += priv->txlen;
	// dev->tbusy=0;
	// mark_bh(NET_BH);

	// dev->interrupt=0;

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static int mb8795_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct next_dma_channel *txd = (struct next_dma_channel *)priv->txdma;
	unsigned long flags;

	// if (!priv->is_turbo) {
	// 	u8 txstat = priv->mb->txstat;

	// 	while ((txstat&TSTAT_TXAVAIL) == 0)
	// 		dev_info(&priv->pdev->dev, "TX not ready. txstat=%x\n", txstat);
	// }

	local_irq_save(flags);

#ifdef DEBUGME_TX
	pr_info("\nstart_xmit p=%p len=%d txstat=%x ", skb->data, skb->len, priv->mb->txstat);
#endif

// 	// check for xmit timeout ?
// 	if (dev->tbusy) {
// 		int tickssince = jiffies-dev->trans_start;
// #ifdef	DEBUGME_TX
// 		printk("st ");
// #endif
// 		if (tickssince < 20)  // o/~ big wheel keep on turnin' o/~
// 			return 1;

// 		printk("xmit timeout, we're bummed\n");
// 		// should reset
// 		dev->tbusy = 0;
// 		return 0;
// 	}

// 	// we only have 1 tx buffer, perhaps should have a ring in memory
// 	//	filled in by txdone int..
// 	if (test_and_set_bit(0, (void *)&dev->tbusy) != 0) {
// 		printk("%s: Transmitter access conflict.\n", dev->name);
// 		return 1;
// 	}

	memcpy(priv->txbuf, skb->data, skb->len);
	// flush_page_to_ram((unsigned long)priv->txbuf&~(PAGE_SIZE-1));
	cache_push(priv->p_txbuf, skb->len);
	// This should do the same as cache_push():
	// arch_sync_dma_for_device(phys_addr_t handle, size_t size, enum dma_data_direction dir)
	// sync_dma_for_device(priv->p_txbuf, skb->len, DMA_TO_DEVICE);

#ifdef DEBUGME_TX
	pr_info("Before\n");
	dumpdmaregs(txd, priv->is_turbo);
	pr_info("\n");
#endif

	priv->mb->txstat = TSTAT_CLEAR;
	// ok, cross our fingers
	txd->csr = DMA_RESET/*|DMA_CLEARCHAINI*/|(priv->is_turbo ? DMA_INITDMA_TURBO : DMA_INITDMA);
	txd->csr = 0; // Configure DMA direction to device? DMA_SETTDEV (same value)?

	priv->txlen = skb->len;

	txd->next_initbuf = priv->p_txbuf; // not used in netbsd

	txd->start = priv->p_txbuf;//only on turbo? dd_next
	if (!priv->is_turbo) {
		txd->saved_start = priv->p_txbuf;
		txd->next_start = 0;//TEST dd_start
	} else {
		txd->next_start = priv->p_txbuf; //dd_start
	}

	// aaargh.  This is evil.
	// The 0x8.. is necesary, but the TXBUFLEN padding
	// is evil.  trying to just account for the +15 breakage
	// wasn't enough for small packets.. aligning to
	// 16/32/64bytes didn't work either.. I hope its not
	// really splatting that much over the net.. I need
	// an analyzer :)

	// try: eth_skb_pad(struct sk_buff *skb) and remove the +15 below? No 15 is magic and also in Previous.
	// The 0x80000000 is also some kind of magic (bit 31(highest bit)) End Of Packet.
	// txd->end	= (priv->p_txbuf+TXBUFLEN+15) | 0x80000000;
	txd->end = (priv->p_txbuf + priv->txlen + (priv->is_turbo ? 0 : MB_MAGIC_PADDING)) | MB_TX_EOP; //dd_limit should align to 16 or maybe 32bytes
	txd->next_end = 0; //TEST dd_stop
	if (!priv->is_turbo)
		txd->saved_end = txd->end;// not needed in netbsd code?

#ifdef DEBUGME_TX
	pr_info("After\n");
	dumpdmaregs(txd, priv->is_turbo);
	pr_info("\n");
#endif

	// dev->trans_start = jiffies;
	txd->csr = DMA_SETENABLE;
	if (priv->is_turbo)
		priv->mb->txmode |= TXM_TURBOSTART;

	// Freeing SKB after triggering DMA may be faster
	dev_kfree_skb(skb);

	local_irq_restore(flags);
	return 0;
}

/* stop all things ethernet.. */
static void mb8795_reset(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct next_dma_channel *txd = (struct next_dma_channel *)priv->txdma;

	// this chip shouldn't do anything until this is cleared
	mb->reset = RST_RESET;

	mb->txmask = 0;
	mb->txstat = TSTAT_CLEAR;
	mb->txmode = priv->is_turbo ? TXM_TURBO : TXM_LOOP_DISABLE;

	mb->rxmask = 0;
	mb->rxstat = RSTAT_CLEAR;
	mb->rxmode = priv->is_turbo ? RXM_TURBO|RXM_ACCEPT : RXM_PROMISC;

	rxd->csr = DMA_RESET;
	txd->csr = DMA_RESET;
}

static int mb8795_stop(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);

#ifdef DEBUGME_FUNC
	pr_info("in %s\n", __func_);
#endif

	// dev->start = 0;
	// dev->tbusy = 1;

	// Should really free skbs here too :)

	mb8795_reset(ndev);

	// next_intmask_disable(NEXT_IRQ_ENETR_DMA-NEXT_IRQ_BASE);
	// next_intmask_disable(NEXT_IRQ_ENETX_DMA-NEXT_IRQ_BASE);
	// next_intmask_disable(NEXT_IRQ_ENETR-NEXT_IRQ_BASE);
	// next_intmask_disable(NEXT_IRQ_ENETX-NEXT_IRQ_BASE);

	free_irq(NEXT_IRQ_ENETR_DMA, priv->irq_rx_dma);
	free_irq(NEXT_IRQ_ENETX_DMA, priv->irq_tx_dma);
	free_irq(NEXT_IRQ_ENETR, priv->irq_rx);
	free_irq(NEXT_IRQ_ENETX, priv->irq_tx);
	// free_irq(IRQ_AUTO_6, priv->irq_rx_dma);
	// free_irq(IRQ_AUTO_6, priv->irq_tx_dma);
	// free_irq(IRQ_AUTO_3, priv->irq_rx);
	// free_irq(IRQ_AUTO_3, priv->irq_tx);

	return 0;
}

static int mb8795_open(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;

#ifdef DEBUGME_FUNC
	pr_info("in %s\n", __func_);
#endif

	mb8795_reset(ndev);

	// ether_addr_copy((u8 *)priv->mb->eaddr, (u8 *)ndev->dev_addr);
	ether_addr_copy((u8 *)ndev->dev_addr, (u8 *)priv->mb->eaddr);

	// dev->tbusy = 0;
	// dev->interrupt = 0;
	// dev->start = 1;

	setup_rxdma(ndev);
#ifdef DEBUGME_RX
	pr_info("open():");
	dumpdmaregs(priv->rxdma, priv->is_turbo);
	pr_info("\n");
#endif

	priv->irq_rx = priv;
	priv->irq_tx = priv;
	priv->irq_rx_dma = priv;
	priv->irq_tx_dma = priv;

	// if (request_irq(IRQ_AUTO_3, mb8795_rxint, IRQF_SHARED, "NeXT Ethernet Receive", priv->irq_rx)) {
	if (request_irq(NEXT_IRQ_ENETR, mb8795_rxint, 0, "Ethernet RX", priv->irq_rx)) {
		pr_err("Failed to register interrupt for NeXT Ethernet RX\n");
		goto err_out_irq_rx;
	}
	// if (request_irq(IRQ_AUTO_3, mb8795_txint, IRQF_SHARED, "NeXT Ethernet Transmit", priv->irq_tx)) {
	if (request_irq(NEXT_IRQ_ENETX, mb8795_txint, 0, "Ethernet TX", priv->irq_tx)) {
		pr_err("Failed to register interrupt for NeXT Ethernet TX\n");
		goto err_out_irq_tx;
	}
	// if (request_irq(IRQ_AUTO_6, mb8795_rxdmaint, IRQF_SHARED, "NeXT Ethernet DMA Receive", priv->irq_rx_dma)) {
	if (request_irq(NEXT_IRQ_ENETR_DMA, mb8795_rxdmaint, 0, "Ethernet RX DMA", priv->irq_rx_dma)) {
		pr_err("Failed to register interrupt for NeXT Ethernet RX DMA\n");
		goto err_out_irq_rx_dma;
	}
	// if (request_irq(IRQ_AUTO_6, mb8795_txdmaint, IRQF_SHARED, "NeXT Ethernet DMA Transmit", priv->irq_tx_dma)) {
	if (request_irq(NEXT_IRQ_ENETX_DMA, mb8795_txdmaint, 0, "Ethernet TX DMA", priv->irq_tx_dma)) {
		pr_err("Failed to register interrupt for NeXT Ethernet TX DMA\n");
		goto err_out_irq_tx_dma;
	}
	// next_intmask_enable(NEXT_IRQ_ENETR-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_ENETX-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_ENETR_DMA-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_ENETX_DMA-NEXT_IRQ_BASE);

	// enable interrupts
	// I couldn't get the chip to stop giving us tons of errors,
	// I blame my complete lack of understanding of the dma hardware

		// mb->rxmask=RSTAT_OVERFLOW|RSTAT_CRC|RSTAT_ALIGN|RSTAT_RUNT|RSTAT_RESET|RSTAT_PRECV;

		// should do a reset on 16col someday...
		// mb->txmask=TSTAT_PARERR|TSTAT_16COL|TSTAT_COLL|TSTAT_UNDERFLOW;
		// mb->txmask = TSTAT_COLL;
		// mb->txmask = TSTAT_PARERR|TSTAT_16COL|TSTAT_COLL|TSTAT_UNDERFLOW;
		// mb->rxmask = RSTAT_PRECV|RSTAT_RESET|RSTAT_RUNT;
		// mb->rxmask = RSTAT_OVERFLOW|RSTAT_CRC|RSTAT_ALIGN|RSTAT_RUNT|RSTAT_RESET|RSTAT_PRECV;
	mb->txmask = 0;
	// mb->txstat = TSTAT_CLEAR;
	// mb->rxmask = 0;
	mb->rxmask = RSTAT_PRECV|RSTAT_RESET|RSTAT_RUNT;
	// mb->rxstat = RSTAT_CLEAR;

	// rock n' roll
	mb->reset = 0;

#ifdef DEBUGME
	dumpregs(mb);
#endif

	return 0;

err_out_irq_tx_dma:
	// free_irq(IRQ_AUTO_3, priv->irq_tx_dma);
	free_irq(NEXT_IRQ_ENETX_DMA, priv->irq_tx_dma);
err_out_irq_rx_dma:
	// free_irq(IRQ_AUTO_6, priv->irq_rx_dma);
	free_irq(NEXT_IRQ_ENETR_DMA, priv->irq_rx_dma);
err_out_irq_tx:
	// free_irq(IRQ_AUTO_6, priv->irq_tx);
	free_irq(NEXT_IRQ_ENETX, priv->irq_tx);
err_out_irq_rx:
	return -EAGAIN;
}

static struct net_device_stats *mb8795_get_stats(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);

	return &priv->stats;
}


static const struct net_device_ops mb8795_ndev_ops = {
	.ndo_open				= mb8795_open,
	.ndo_stop				= mb8795_stop,
	.ndo_start_xmit			= mb8795_start_xmit,
	// .ndo_tx_timeout			= mb8795_tx_timeout,
	.ndo_get_stats			= mb8795_get_stats,
	.ndo_validate_addr		= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr
};

static int mb8795_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct mb8795_private *priv;
	volatile struct bmap_chip *bmap = (void __iomem *)NEXT_BMAP;
	int err;
	int i;

	dev_info(&pdev->dev, "Probing\n");

	ndev = alloc_etherdev(sizeof(struct mb8795_private));
	if (!ndev) {
		dev_err(&pdev->dev, "Failed to allocate ethernet device\n");
		return -ENOMEM;
	}
	ndev->netdev_ops = &mb8795_ndev_ops;

	priv = netdev_priv(ndev);
	priv->pdev = pdev;
	priv->ndev = ndev;
	priv->is_turbo = NEXT_IS_TURBO;

	// Kick BMAP
	// if (machine_type == NeXT_X15) // cube040?
	dev_info(&pdev->dev, "Kicking BMAP\n");
	bmap->bm_lo = 0;
	dev_info(&pdev->dev, "BMAP kicked\n");

	// priv->mb = (void __iomem *)NEXT_ETHER;
	priv->mb = ioremap(0x2000000+0x6000, sizeof(struct mb8795regs));
	// priv->mb = __iomem ioremap(0x02000000+0x00100000+0x00006000, sizeof(struct mb8795regs));
	// work-around bug?
	priv->mb->txmode = TXM_LOOP_DISABLE;
	mdelay(10);
	priv->mb->txmode = 0;

	priv->mb->reset = RST_RESET;
	priv->mb->txmask = 0;
	priv->mb->rxmask = 0;
	priv->mb->txstat = TSTAT_CLEAR;
	priv->mb->rxstat = RSTAT_CLEAR;
	priv->mb->txmode = priv->is_turbo ? TXM_TURBO : TXM_LOOP_DISABLE;
	priv->mb->rxmode = RXM_DISABLE;

	priv->rxdma = (struct next_dma_channel *)NEXT_ETHER_RXDMA_BASE;
	priv->txdma = (struct next_dma_channel *)NEXT_ETHER_TXDMA_BASE;

	// dev_info(&pdev->dev, "rxdma->csr=0x%x rxdma->turbo_rx_saved_start=0x%x rxdma->start=0x%x txdma->csr=0x%x txdma->start=0x%x\n", &priv->rxdma->csr, &priv->rxdma->turbo_rx_saved_start, &priv->rxdma->start, &priv->txdma->csr, &priv->txdma->start);

	dev_info(&pdev->dev, "PROM MAC Address: %pM\n", eprom_info.eaddr);
	dev_info(&pdev->dev, "Ethernet Chip MAC Address: %pM\n", priv->mb->eaddr);

	if (eprom_info.eaddr[0]|eprom_info.eaddr[1]|eprom_info.eaddr[2]|eprom_info.eaddr[3]|eprom_info.eaddr[4]|eprom_info.eaddr[5]) {
		ether_addr_copy((u8 *)eprom_info.eaddr, (u8 *)priv->mb->eaddr);
		dev_info(&pdev->dev, "Using PROM MAC Address\n");
	} else if (priv->mb->eaddr[0]|priv->mb->eaddr[1]|priv->mb->eaddr[2]|priv->mb->eaddr[3]|priv->mb->eaddr[4]|priv->mb->eaddr[5]) {
		dev_info(&pdev->dev, "Using Ethernet Chip MAC Address\n");
	} else {
		eth_hw_addr_random(ndev);
		ether_addr_copy((u8 *)ndev->dev_addr, (u8 *)priv->mb->eaddr);
		dev_info(&pdev->dev, "Missing Ethernet Chip and PROM MAC address. Assigning random address\n");
	}
	// ether_addr_copy((u8 *)priv->mb->eaddr, (u8 *)ndev->dev_addr);
	eth_hw_addr_set(ndev, (u8 *)priv->mb->eaddr);
	dev_info(&pdev->dev, "Netdev MAC Address: %pM\n", ndev->dev_addr);

	// spin_lock_init(&bp->lock);

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register net device\n");
		goto err_out_free_netdev;
	}

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	priv->txbuf = devm_kmalloc(&ndev->dev, TXBUFLEN, GFP_KERNEL);

	// ___GFP_DMA32 linux/kernel/dma/direct.c
	// buf_pool->rx_skb = devm_kcalloc(dev, buf_pool->slots,
	// 				sizeof(struct sk_buff *),
	// 				GFP_KERNEL);

	/* assuming we're on a page boundry for dma alignment */
	priv->p_txbuf = virt_to_phys(priv->txbuf);

		// OR linux/arch/m68k/kernel/dma.c
		// arch_dma_alloc(struct device *dev, size_t size, dma_addr_t *dma_handle,
		// gfp_t gfp, unsigned long attrs)
		// Example: priv->txbuf = dma_alloc(pdev->dev, TXBUFLEN, priv->p_txbuf, GFP_DMA | GFP_KERNEL, 0)
		// dma_alloc_coherent(p_dev, TX_REG_DESC_SIZE *
		// 	TX_DESC_NUM, &priv->tx_base,
		// 	GFP_DMA | GFP_KERNEL);
		// This is better since it disables cache
		// dmam_alloc_coherent(struct device *dev, size_t size,
		// 	dma_addr_t *dma_handle, gfp_t gfp)

	// rx ring
	for (i = 0; i < NRXBUFS; i++) {
		struct rxb *rx = (struct rxb *)&priv->rxbufs[i];

		rx->skb = mb_new_skb(ndev);
		rx->p_data = virt_to_phys(rx->skb->data);
		rx->len = 0;  /* is filled in by chain handler */

		if (i)
			priv->rxbufs[i-1].next = rx;
	}

	// take care of head/tail
	priv->rxbufs[0].next	= &priv->rxbufs[1];
	priv->rxbufs[i-1].next	= &priv->rxbufs[0];
	priv->cur_rxb		= &priv->rxbufs[0];

	mb8795_reset(ndev);

	// dev_info(&pdev->dev, "Finished probing\n");

	return 0;

// err_out_unregister_netdev:
// 	unregister_netdev(ndev);
// err_out_free_irq:
// 	free_irq(ndev->irq, ndev);
err_out_free_netdev:
	free_netdev(ndev);
	return err;
}

static struct platform_driver mb8795_driver = {
	.probe	= mb8795_probe,
	// .remove	= mb8795_remove,
	.driver	= { .name = DRV_NAME }
};

static struct platform_device mb8795_device = {
	.name = DRV_NAME,
};

static int mb8795_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mb8795_driver);

	if (!ret) {
		ret = platform_device_register(&mb8795_device);
		if (ret)
			platform_driver_unregister(&mb8795_driver);
	}
	return ret;
}

module_init(mb8795_init);
