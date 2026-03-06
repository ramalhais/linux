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
#include <linux/mm.h>

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
// #define DMA_ENDALIGNMENT	16	// DMA must start(Previous) and end on quad longword //default
// #define ENDMA_ENDALIGNMENT	32	// Ethernet DMA is very special
// #define DMA_ENDALIGN(type, addr) ((type)(((unsigned)(addr)+DMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1))) // default
// #define ENDMA_ENDALIGN(type, addr) ((type)((((unsigned)(addr)+ENDMA_ENDALIGNMENT-1)&~(DMA_ENDALIGNMENT-1))|0x80000000)) // TX with end of packet bit?

struct mb8795_private {
	// spinlock_t lock;
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

static inline struct sk_buff *mb_new_skb(struct net_device *ndev)
{
	struct sk_buff *newskb;
	unsigned int fixup;

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

static irqreturn_t mb8795_rxint(int irq, void *dev_id)
{
	unsigned long flags;
#ifdef DEBUGME_RX
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;
#endif

	local_irq_save(flags);

#ifdef DEBUGME_RX
	pr_info("\nRX int: rxstat=0x%x", mb->rxstat);
#endif

	// if (mb->rxstat&RSTAT_PRECV)
	// 	mb->rxstat = RSTAT_PRECV; // is this like an acknowledgement? or should it be cleared(0) or 0xff?

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static void setup_rxdma(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct rxb *rx = priv->cur_rxb;

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

	rxd->start = rx->p_data;
	rxd->end = rx->p_data+RXBUFLEN;

	rx = rx->next;

	rxd->next_start = rx->p_data;
	rxd->next_end = rx->p_data+RXBUFLEN;

	rxd->csr = DMA_SETENABLE|DMA_SETTMEM|DMA_SETCHAIN;
}

static irqreturn_t mb8795_rxdmaint(int irq, void *dev_id)
{
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct rxb *rx = (struct rxb *)priv->cur_rxb;
	u32 csr;
	u8 rxstat;
	unsigned long flags;

	local_irq_save(flags);

	// Perhaps should check chip status sometime to look for rx errors
	rxstat = priv->mb->rxstat;
	csr = rxd->csr;

	if (!(rxstat&RSTAT_PRECV) /*|| csr&DMA_OVERFLOW*/) {
		priv->cur_rxb = rx->next;
		setup_rxdma(priv->ndev);
		goto bail;
	}

	if ((csr&DMA_CINT) && (csr&DMA_ENABLED)) {
		rxd->csr = DMA_CLEARCHAINI;
		// hack: For some reason Previous is setting saved_start to zero, so we get the physical start from rx->p_data
		rx->len = (priv->is_turbo ? rxd->turbo_rx_saved_start : rxd->saved_end) - rx->p_data - 4; // -4 is the checksum?
		handle_packet(priv, priv->ndev, rx);
	} else {
		// if we missed the first chained int we'll
		// have a waiting packet in the 'first' slot
		// but aren't currently dealing with it. Fix
		rx = rx->next;
		rx->len = rxd->start - rxd->next_start - 4; // FIXME: this is probably wrong! Seems always correct after all. WTF

		if (csr&DMA_CINT)
			rxd->csr = DMA_CLEARCHAINI;

		handle_packet(priv, priv->ndev, rx);

		priv->cur_rxb = rx->next;
		setup_rxdma(priv->ndev);
	}

bail:
	priv->mb->rxstat = RSTAT_CLEAR;

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static irqreturn_t mb8795_txint(int irq, void *dev_id)
{
	unsigned long flags;
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;

	local_irq_save(flags);

	if (priv->mb->txstat&TSTAT_COLL) {
		priv->stats.collisions++;
		priv->mb->txstat = TSTAT_COLL;
	}

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static irqreturn_t mb8795_txdmaint(int irq, void *dev_id)
{
	unsigned long flags;
	struct mb8795_private *priv = (struct mb8795_private *)dev_id;
	struct next_dma_channel *txd = (struct next_dma_channel *)priv->txdma;

	local_irq_save(flags);

	if (priv->mb->txstat&(TSTAT_TXAVAIL|TSTAT_TXDONE)) {
		priv->mb->txstat = TSTAT_CLEAR;
		priv->mb->txmask = 0;
	}

	txd->csr = DMA_RESET/*|DMA_CLEARCHAINI*/; // DMA_CLEARCHAINI breaks DMA turbo in Previous

	priv->stats.tx_packets++;
	priv->stats.tx_bytes += priv->txlen;

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


	memcpy(priv->txbuf, skb->data, skb->len);
	cache_push(priv->p_txbuf, skb->len);

	priv->mb->txstat = TSTAT_CLEAR;
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

	// try: eth_skb_pad(struct sk_buff *skb) and remove the +15 below? No!, 15 is magic and also in Previous.
	// The 0x80000000 is also some kind of magic (bit 31(highest bit)) End Of Packet.
	// txd->end	= (priv->p_txbuf+TXBUFLEN+15) | 0x80000000;
	txd->end = (priv->p_txbuf + priv->txlen + (priv->is_turbo ? 0 : MB_MAGIC_PADDING)) | MB_TX_EOP; //dd_limit should align to 16 TX and 32bytes on RX
	txd->next_end = 0;
	if (!priv->is_turbo)
		txd->saved_end = txd->end;// not needed in netbsd code?

	txd->csr = DMA_SETENABLE;
	if (priv->is_turbo)
		priv->mb->txmode |= TXM_TURBOSTART;

	dev_kfree_skb(skb);

	local_irq_restore(flags);
	return 0;
}

static void mb8795_reset(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;
	struct next_dma_channel *rxd = (struct next_dma_channel *)priv->rxdma;
	struct next_dma_channel *txd = (struct next_dma_channel *)priv->txdma;

	mb->reset = RST_RESET;

	mb->txmask = 0;
	mb->txstat = TSTAT_CLEAR;
	mb->txmode = (priv->is_turbo ? TXM_TURBO : TXM_LOOP_DISABLE);

	mb->rxmask = 0;
	mb->rxstat = RSTAT_CLEAR;
	mb->rxmode = (priv->is_turbo ? (RXM_TURBO|RXM_ACCEPT) : RXM_PROMISC);

	rxd->csr = DMA_RESET;
	txd->csr = DMA_RESET;
}

static int mb8795_stop(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);

	mb8795_reset(ndev);

	free_irq(NEXT_IRQ_ENETR_DMA, priv->irq_rx_dma);
	free_irq(NEXT_IRQ_ENETX_DMA, priv->irq_tx_dma);
	free_irq(NEXT_IRQ_ENETR, priv->irq_rx);
	free_irq(NEXT_IRQ_ENETX, priv->irq_tx);

	return 0;
}

static int mb8795_open(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);
	struct mb8795regs *mb = (struct mb8795regs *)priv->mb;

	mb8795_reset(ndev);
	setup_rxdma(ndev);

	priv->irq_rx = priv;
	priv->irq_tx = priv;
	priv->irq_rx_dma = priv;
	priv->irq_tx_dma = priv;

	if (request_irq(NEXT_IRQ_ENETR, mb8795_rxint, 0, "Ethernet RX", priv->irq_rx)) {
		pr_err("Failed to register interrupt for NeXT Ethernet RX\n");
		goto err_out_irq_rx;
	}
	if (request_irq(NEXT_IRQ_ENETX, mb8795_txint, 0, "Ethernet TX", priv->irq_tx)) {
		pr_err("Failed to register interrupt for NeXT Ethernet TX\n");
		goto err_out_irq_tx;
	}
	if (request_irq(NEXT_IRQ_ENETR_DMA, mb8795_rxdmaint, 0, "Ethernet RX DMA", priv->irq_rx_dma)) {
		pr_err("Failed to register interrupt for NeXT Ethernet RX DMA\n");
		goto err_out_irq_rx_dma;
	}
	if (request_irq(NEXT_IRQ_ENETX_DMA, mb8795_txdmaint, 0, "Ethernet TX DMA", priv->irq_tx_dma)) {
		pr_err("Failed to register interrupt for NeXT Ethernet TX DMA\n");
		goto err_out_irq_tx_dma;
	}

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

	return 0;

err_out_irq_tx_dma:
	free_irq(NEXT_IRQ_ENETX_DMA, priv->irq_tx_dma);
err_out_irq_rx_dma:
	free_irq(NEXT_IRQ_ENETR_DMA, priv->irq_rx_dma);
err_out_irq_tx:
	free_irq(NEXT_IRQ_ENETX, priv->irq_tx);
err_out_irq_rx:
	return -EAGAIN;
}

static struct net_device_stats *mb8795_get_stats(struct net_device *ndev)
{
	struct mb8795_private *priv = netdev_priv(ndev);

	return &priv->stats;
}

inline void bytecopy(void *d, void *s, size_t n)
{
	while (n--)
		*(volatile u8 *)d++ = *(volatile u8 *)s++;
}

static int mb8795_set_mac_address(struct net_device *ndev, void *p)
{
	const struct sockaddr *addr = p;
	struct mb8795_private *priv;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	eth_hw_addr_set(ndev, addr->sa_data);

	priv = netdev_priv(ndev);
	bytecopy((void *)priv->mb->eaddr, (void *)addr->sa_data, ETH_ALEN);

	return 0;
}

static const struct net_device_ops mb8795_ndev_ops = {
	.ndo_open		= mb8795_open,
	.ndo_stop		= mb8795_stop,
	.ndo_start_xmit		= mb8795_start_xmit,
	// .ndo_tx_timeout	= mb8795_tx_timeout,
	.ndo_get_stats		= mb8795_get_stats,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= mb8795_set_mac_address
};

static int mb8795_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct mb8795_private *priv;
	int err;
	int i;

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

	priv->mb = ioremap(NEXT_ETHER, sizeof(struct mb8795regs));

	// FIXME: try to force twisted pair ethernet. should work for all 68040 but blows up accessing bmap->bm_drw on turbos for some reason :( memory alignment?
	if (!(priv->is_turbo) && !NEXT_IS_030) {
		struct bmap_chip *bmap = ioremap(NEXT_BMAP, sizeof(struct bmap_chip));

		// Receive on Twisted Pair Ethernet
		bmap->bm_drw |= BMAP_TPE;
		priv->mb->txmode &= ~TXM_LOOP_DISABLE;

		iounmap(bmap);
	}

	priv->mb->reset = RST_RESET;
	priv->mb->txmask = 0;
	priv->mb->rxmask = 0;
	priv->mb->txstat = TSTAT_CLEAR;
	priv->mb->rxstat = RSTAT_CLEAR;
	priv->mb->txmode = priv->is_turbo ? TXM_TURBO : TXM_LOOP_DISABLE;
	priv->mb->rxmode = RXM_DISABLE;

	priv->rxdma = ioremap(NEXT_CSR_ETHER_RX, sizeof(struct next_dma_channel));
	priv->txdma = ioremap(NEXT_CSR_ETHER_TX, sizeof(struct next_dma_channel));

	char *eprom = ioremap(NEXT_EPROM, NEXT_EPROM_SIZE);
	if (is_valid_ether_addr(eprom+8)) {
		eth_hw_addr_set(ndev, eprom+8);
		dev_info(&pdev->dev, "Using PROM MAC Address: %pM\n", ndev->dev_addr);
	} else {
		eth_hw_addr_random(ndev);
		dev_info(&pdev->dev, "Invalid PROM MAC Address: %pM. Using random address: %pM\n", eprom+8, ndev->dev_addr);
	}
	iounmap(eprom); // on 68030: "iounmap: bad pmd(00000000)"
	bytecopy((void *)priv->mb->eaddr, (void *)ndev->dev_addr, ETH_ALEN); // ether_addr_copy, memcpy, memcpy_toio, all fail because of memory alignment?
	// work-around message "Expected addr:"
	ether_addr_copy(ndev->dev_addr_shadow, ndev->dev_addr);

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register net device\n");
		goto err_out_free_netdev;
	}

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	priv->txbuf = devm_kmalloc(&ndev->dev, TXBUFLEN, GFP_KERNEL);
	// assuming we're on a page boundry for dma alignment
	priv->p_txbuf = virt_to_phys(priv->txbuf);

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

	return 0;

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
