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

/* XXX
	this driver is far from done, but I'm tired of wrestling
	with the dma silliness.  things to do:
		o pay attention to error conditions (service ints, reset, etc)
		o _stop() for real
		o modular
		o dma into properly alligned skbs
		o handle first packet if we miss the first chain int
		o keep full statistics, byte counters, etc
*/

#define DRV_NAME	"mb8795"
#define DRV_VERSION	"0.01"
#define DRV_RELDATE	"2004-09-30"

#include <linux/module.h>

#include <linux/kernel.h>
//#include <linux/sched.h>
//#include <linux/errno.h>
//#include <linux/string.h>
#include <linux/init.h>
//#include <linux/mm.h>
#include <asm/io.h>
//#include <asm/system.h>
//#include <asm/pgtable.h>

//#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>

#include <asm/nexthw.h>
#include <asm/nextints.h>

#undef DEBUGME_TX 
#undef DEBUGME_RX 

#undef DEBUGME_FUNC

#ifndef DEBUGME 
# if defined(DEBUGME_TX) || defined(DEBUGME_RX)
#  define DEBUGME
# endif
#endif

struct mb8795regs {
	volatile __u8	txstat,txmask;
	volatile __u8	rxstat,rxmask;
	volatile __u8	txmode,rxmode;
	volatile __u8	reset;
	volatile __u8	txcntlow;
	volatile __u8	eaddr[6];
	volatile __u8	reserved_dont_use[1];
	volatile __u8	txcnthi;
};

/* events for tx status/mask */

#define TSTAT_PARERR	0x1
#define TSTAT_16COL	0x2
#define TSTAT_COLL	0x4
#define TSTAT_UNDERFLOW	0x8
#define TSTAT_TXDONE	0x20
#define TSTAT_BUSY	0x40
#define TSTAT_TXAVAIL	0x80

/* events for rx status/mask */

#define RSTAT_OVERFLOW	0x1
#define RSTAT_CRC	0x2
#define RSTAT_ALIGN	0x4
#define RSTAT_RUNT	0x8
#define RSTAT_RESET	0x10	/* received a reset packet ? */
#define RSTAT_PRECV	0x80	/* packet received */

/* bits in reset */
#define RST_RESET	0x8

/* bits in txmode */
#define TXM_NCOL_MASK 0xf0
#define TXM_LOOP_DISABLE 0x2

/* bits in rxmode */
#define RXM_ACCEPT 0x01
#define RXM_PROMISC 0x11

#define NRXBUFS 8
/* must be a power of 2 */
#define ALIGN 32

#define TXBUFLEN (1514)
#define MAXTXBYTES (1514+4+2*ALIGN)
/* must have len be !(%ALIGN) so the end addr is */
#define RXBUFLEN (MAXTXBYTES-(MAXTXBYTES%ALIGN))

#define NEXT_RXBUF(x) ((x+1)%NRXBUFS)

struct mb8795_private {
	struct mb8795regs *mb;
	struct rxdmaregs *rxdma;
	struct txdmaregs *txdma;
	unsigned char *txbuf;
	__u32 p_txbuf;
	__u32 txlen;

	struct rxb {
		struct sk_buff *skb;
		__u32 p_data;
		__u32 len;
		struct rxb *next;
	} rxbufs[NRXBUFS];

	struct rxb *cur_rxb;	/* the buffers in use by the dma */

	struct net_device_stats stats;
};
		
/* XXX */
static int stupidface=0;

/* the ethernet dma on the next seems to be magic
	so we get our own copy */

struct rxdmaregs {
	volatile __u32	csr;
	char padding[16364];
	volatile __u32	saved_start;  /* save in case of abort? */
	volatile __u32	saved_end;
	volatile __u32	saved_next_start;
	volatile __u32	saved_next_end;
	volatile __u32	start;		/* phys start and end addrs of dma block */
	volatile __u32	end;
	volatile __u32	next_start;	/* next in the chain */
	volatile __u32	next_end;
};

struct txdmaregs {
	volatile __u32	csr;
	char padding[0x3fec];
	volatile __u32	saved_start;  /* save in case of abort? */
	volatile __u32	saved_end;
	volatile __u32	saved_next_start;
	volatile __u32	saved_next_end;
	volatile __u32	r_start; /* (read only) phys start and end addrs of dma block */
	volatile __u32	end;
	volatile __u32	next_start;	/* next in the chain */
	volatile __u32	next_end;
	char paddinnnng[0x1f0];
	volatile __u32	w_start; /* (write only) phys start and end addrs of dma block */
};

#ifdef DEBUGME
void	dumpregs(struct mb8795regs *mb)
{
	printk("txs: 0x%0X, txma: 0x%0X ",mb->txstat,mb->txmask);
	printk("rxs: 0x%0X, rxma: 0x%0X\n",mb->rxstat,mb->rxmask);
	printk("txmo: 0x%0X, rxmo: 0x%0X\n ",mb->txmode,mb->rxmode);
	printk("reset: 0x%0X\n ",mb->reset);
};

void hdump(unsigned char *d,int len) {

	if (len>32)len=32;

	for(;len;len--) {printk("%02x ",*d);d++;};
}
	
void dumpdmaregs(void *ptr) {
	/* hack :) */
	struct rxdmaregs *rxd=(struct rxdmaregs *)ptr;
	__u32  csr=rxd->csr,saved_start=rxd->saved_start,saved_end=rxd->saved_end,
		saved_next_start=rxd->saved_next_start,saved_next_end=rxd->saved_next_end,
		start=rxd->start,end=rxd->end,next_start=rxd->next_start,next_end=rxd->next_end;

	printk("csr:%x ",csr);
	printk("ss:%x ",saved_start);
	printk("se:%x " ,saved_end);
	printk("sns:%x ",saved_next_start);
	printk("sne:%x ",saved_next_end);
	printk("s:%x ",start);
	printk("e:%x ",end);
	printk("ns:%x ",next_start);
	printk("ne:%x ",next_end);
};
	
#endif

static void setup_rxdma(struct device *dev)
{
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv;
        struct rxdmaregs *rxd=(struct rxdmaregs *)priv->rxdma;
	struct rxb *rx=priv->cur_rxb;

	/* init the rx dma buffer pointers*/

        rxd->csr=0;
        rxd->csr=(DMA_RESET|DMA_SETTMEM);

	rxd->start=rx->p_data;
        rxd->end=rx->p_data+RXBUFLEN;

	rx=rx->next;

	rxd->next_start=rx->p_data;
	rxd->next_end=rx->p_data+RXBUFLEN;

        rxd->csr=DMA_SETENABLE|DMA_SETCHAIN; 
}

/* XXX check for return of NULL :( */

static inline struct sk_buff *mb_new_skb(void) {
	struct sk_buff *newskb;
	unsigned int fixup;

	newskb=dev_alloc_skb(RXBUFLEN);
	fixup=(unsigned int)newskb->data&(ALIGN-1);
	if(fixup) skb_reserve(newskb,ALIGN-fixup);
		
	return newskb;
}

static inline void handle_packet(struct mb8795_private *priv,struct device *dev,struct rxb *rx) {
	int len=rx->len;
	struct sk_buff *skb = rx->skb;

	cache_clear(rx->p_data,len);
	skb->dev=dev;
	skb_put(skb,len);
	skb->protocol=eth_type_trans(skb,dev);

	priv->stats.rx_packets++;
	priv->stats.rx_bytes+=len;

	netif_rx(skb); 

	rx->skb=mb_new_skb();
	rx->p_data=VTOP(rx->skb->data);
	rx->len=0;
}

/*  no rx ints are being allowed for now.. */

static void mb8795_rxint(int irq, void *dev_id, struct pt_regs *regs)
{
	struct device *dev=(struct device *)dev_id;
	struct mb8795_private *priv=(struct mb8795_private *)dev->priv; 
	struct mb8795regs *mb=(struct mb8795regs *)priv->mb; 

	__u8 rxstat=mb->rxstat;

#ifdef DEBUGME_RX
	printk("rxs: %x ",rxstat);
#endif

	if(rxstat & RSTAT_PRECV) {
		mb->rxstat=RSTAT_PRECV;	
	}
}

/* we set up the dma engine to start recieving the next
	packet while we go off and queue the packet that
	this int was for. this way we find another chain
	int waiting for us when we're done with this one */

/* the dma handling here is _very_ touchy.  mis stepping can cause really 
	wacked garbage like turning on loopback.
	No, I don't know either. */

static void mb8795_rxdmaint(int irq, void *dev_id, struct pt_regs *regs)
{
        struct device *dev=(struct device *)dev_id;
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv;
	struct rxdmaregs *rxd=(struct rxdmaregs *)priv->rxdma;
	struct rxb *rx=(struct rxb *)priv->cur_rxb;

	__u32 csr=rxd->csr;
	__u32 ss=rxd->saved_start,se=rxd->saved_end,
		s=rxd->start,ns=rxd->next_start;

#ifdef DEBUGME_RX
	printk("csr: %x %x %x %x %x",csr,ss,se,s,ns);
#endif
	/* perhaps should check chip status 
		sometime to look for rx errors */

	if(csr&DMA_CINT && csr&DMA_ENABLED) {
		/* chain int, we have another buffer yet */
		rxd->csr=DMA_CLEARCHAINI;
		rx->len=se-ss-4;
		handle_packet(priv,dev,rx);
	} else {
		/* if we missed the first chained int we'll
		have a waiting packet in the 'first' slot
		but aren't currently dealing with it. Fix */
		rx=rx->next;
		rx->len=s-ns-4;
		if(csr&DMA_CINT) {
			rxd->csr=DMA_CLEARCHAINI;
		}
		priv->cur_rxb=rx->next;
		setup_rxdma(dev);
		handle_packet(priv,dev,rx);
		priv->stats.rx_packets++;
	}

#ifdef DEBUGME_RX
/*	dumpdmaregs(rxd);   */
#endif
}
static void mb8795_txint(int irq, void *dev_id, struct pt_regs *regs)
{
	struct device *dev=(struct device *)dev_id;
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv;

	__u8 txstat=priv->mb->txstat;

	if(txstat & TSTAT_COLL) {
		priv->stats.collisions++;
		priv->mb->txstat=TSTAT_COLL;		
	}
}

/* this sucks.  Needs to be much brighter at realizing that the
	chip / dma might be wedged 
	and paying attention to the fate of the packet we just sent out
	might be wise */

static void mb8795_txdmaint(int irq, void *dev_id, struct pt_regs *regs)
{
	struct device *dev=(struct device *)dev_id;
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv;
        struct txdmaregs *txd=(struct txdmaregs *)priv->txdma;

#ifdef DEBUGME_TX
	struct mb8795regs *mb=(struct mb8795regs *)priv->mb;
        printk("txdma : %x  ",mb->txstat);
	dumpdmaregs(txd);
#endif

	/* ? we're in an interrupt ? */
	dev->interrupt=1;

	txd->csr=DMA_RESET;
	priv->stats.tx_packets++;
	priv->stats.tx_bytes+=priv->txlen;
	dev->tbusy=0;
	mark_bh(NET_BH);

	dev->interrupt=0;
	return;
}

static int mb8795_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct mb8795_private *priv=(struct mb8795_private *)dev->priv; 
        struct txdmaregs *txd=(struct txdmaregs *)priv->txdma;


#ifdef DEBUGME_TX
	printk("xmts [%p %d]: %x ",skb->data,skb->len,mb->txstat);
#endif

	/* check for xmit timeout ? */
	if(dev->tbusy) {
		int tickssince=jiffies-dev->trans_start;
#ifdef	DEBUGME_TX
		printk("st ");
#endif
		if(tickssince < 20)  /* o/~ big wheel keep on turnin' o/~ */
			return 1;

		printk("xmit timeout, we're bummed\n");
		/* should reset */
	        dev->tbusy = 0;
		return 0;
	}

	/* we only have 1 tx buffer, perhaps should have a ring in memory 
		filled in by txdone int.. */
	if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
		printk("%s: Transmitter access conflict.\n", dev->name);
		return(1);
	}

	memcpy(priv->txbuf,skb->data,skb->len);
/*	flush_page_to_ram((unsigned long)priv->txbuf&~(PAGE_SIZE-1));  */
	cache_push(priv->p_txbuf,skb->len);

#ifdef DEBUGME_TX
	dumpdmaregs(txd);
	printk("\n");
#endif

	/* ok, cross our fingers */
	txd->csr=DMA_RESET|DMA_INITDMA|DMA_CLEARCHAINI;
	txd->csr=0;

	priv->txlen=skb->len;

	txd->w_start=priv->p_txbuf;
	txd->saved_start=priv->p_txbuf; 


	/* aaargh.  This is evil.
		The 0x8.. is necesary, but the TXBUFLEN padding
		is evil.  trying to just account for the +15 breakage
		wasn't enough for small packets.. alligning to 
		16/32/64bytes didn't work either.. I hope its not
		really splatting that much over the net.. I need
		an analyzer :) */
	txd->end=(priv->p_txbuf+TXBUFLEN+15) | 0x80000000;
	txd->saved_end=(priv->p_txbuf+TXBUFLEN+15) | 0x80000000; 


	dev_kfree_skb(skb); 
	
	dev->trans_start = jiffies;
	txd->csr=DMA_SETENABLE;

	return 0;
}

/* stop all things ethernet.. */
static void mb8795_reset(struct device *dev)
{
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv; 
        struct mb8795regs *mb=(struct mb8795regs *)priv->mb; 
        struct rxdmaregs *rxd=(struct rxdmaregs *)priv->rxdma; 
        struct txdmaregs *txd=(struct txdmaregs *)priv->txdma;

	/* this chip shouldn't do anything until this is cleared */
	mb->reset=RST_RESET;

        mb->txmode=TXM_LOOP_DISABLE;
        mb->rxmode=RXM_ACCEPT;
        mb->rxmask=0;
        mb->txmask=0;

	/* reset / init dma channels */
	rxd->csr=(DMA_RESET|DMA_CLEARCHAINI);
	txd->csr=(DMA_RESET|DMA_CLEARCHAINI);

}

static int mb8795_stop(struct device *dev)
{
#ifdef DEBUGME_FUNC
	printk("in %s\n",__FUNCTION__);
#endif

	dev->start = 0;
	dev->tbusy = 1;

/*	should really free skbs here too :) */

	mb8795_reset(dev);
	
	free_irq(NEXT_IRQ_ENETR,dev);
	free_irq(NEXT_IRQ_ENETX,dev);
	free_irq(NEXT_IRQ_ENETX_DMA,dev);
	free_irq(NEXT_IRQ_ENETR_DMA,dev);

	return 0;
}

static int mb8795_open(struct device *dev)
{
        struct mb8795_private *priv=(struct mb8795_private *)dev->priv; 
        struct mb8795regs *mb=(struct mb8795regs *)priv->mb; 
	int i;

#ifdef DEBUGME_FUNC
	printk("in %s\n",__FUNCTION__);
#endif

	mb8795_reset(dev);

	/* set the hw addr */
	for(i=0;i<6;i++)  mb->eaddr[i]=dev->dev_addr[i]; 

	if(request_irq(NEXT_IRQ_ENETR,mb8795_rxint,0,"mb8795 RX",dev)) goto bail1;
	if(request_irq(NEXT_IRQ_ENETX,mb8795_txint,0,"mb8795 TX",dev)) goto bail2;
	if(request_irq(NEXT_IRQ_ENETX_DMA,mb8795_txdmaint,0,"mb8795 TX DMA",dev)) goto bail3;
	if(request_irq(NEXT_IRQ_ENETR_DMA,mb8795_rxdmaint,0,"mb8795 RX DMA",dev)) goto bail4;

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	setup_rxdma(dev);
#ifdef DEBUGME_RX
	printk("setup: ");
	dumpdmaregs(rxd);
	printk("\n");
#endif

	/* enable interrupts */
/*	I couldn't get the chip to stop giving us tons of errors,
	I blame my complete lack of understanding of the dma hardware */

/*        mb->rxmask=RSTAT_OVERFLOW|RSTAT_CRC|RSTAT_ALIGN|RSTAT_RUNT|RSTAT_RESET|RSTAT_PRECV;    */

	/* should do a reset on 16col someday.. */
/*        mb->txmask=TSTAT_PARERR|TSTAT_16COL|TSTAT_COLL|TSTAT_UNDERFLOW; */
        mb->txmask=TSTAT_COLL;

	/* rock n' roll */
	mb->reset=0; 

#ifdef DEBUGME
	dumpregs(mb);
#endif

	return 0;

bail4: free_irq(NEXT_IRQ_ENETX_DMA,dev);

bail3: free_irq(NEXT_IRQ_ENETX,dev);

bail2: free_irq(NEXT_IRQ_ENETR,dev);

bail1:

	return EAGAIN;
}

static struct net_device_stats *mb8795_get_stats(struct device *dev)
{
	struct mb8795_private *priv=(struct mb8795_private *)dev->priv;

	return(&priv->stats);
}
	
int __init mb8795_probe(struct device *dev))
{
	struct mb8795_private *priv;
	int i;

#ifdef DEBUGME_FUNC
	printk("in %s\n",__FUNCTION__);
#endif

	if(!stupidface) {
		stupidface=1; /* why do we get called so many times? */
		printk("%s %s %s %s, onboard ether, address: ",DRV_NAME, DRV_VERSION, DRV_RELDATE,dev->name);

		for(i=0;i<5;i++) {
			printk("%0X:",dev->dev_addr[i]=eprom_info.eaddr[i]);
		}
		printk("%0X\n",dev->dev_addr[5]=eprom_info.eaddr[5]);

		init_etherdev(dev, 0);

		dev->priv = kmalloc(sizeof(struct mb8795_private), GFP_KERNEL);
		priv=(struct mb8795_private *)dev->priv;
		memset(priv, 0, sizeof(struct mb8795_private));
		priv->mb=(struct mb8795regs *)NEXT_ETHER_BASE;
		priv->rxdma=(struct rxdmaregs *)NEXT_ETHER_RXDMA_BASE;
		priv->txdma=(struct txdmaregs *)NEXT_ETHER_TXDMA_BASE;
		priv->txbuf = (unsigned char *)kmalloc(TXBUFLEN, GFP_KERNEL);
		/* assuming we're on a page boundry for dma alignment */
		priv->p_txbuf=VTOP(priv->txbuf); 

		/* rx ring */
		for(i=0;i<NRXBUFS;i++) {
			struct rxb *rx=(struct rxb *)&priv->rxbufs[i];


			rx->skb=mb_new_skb();
			rx->p_data=VTOP(rx->skb->data);
			rx->len=0;  /* is filled in by chain handler */

			if(i) priv->rxbufs[i-1].next=rx;

		}
		
		/* take care of head/tail */
		priv->rxbufs[0].next=&priv->rxbufs[1];
		priv->rxbufs[i-1].next=&priv->rxbufs[0];

		priv->cur_rxb=&priv->rxbufs[0];

		dev->open=&mb8795_open;
		dev->stop=&mb8795_stop;
		dev->hard_start_xmit=&mb8795_start_xmit;
		dev->get_stats=mb8795_get_stats;
		
		return 0;
	}
	
	return ENODEV;
}

	

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c mb8795.c"
 *  version-control: t
 *  tab-width: 4
 *  kept-new-versions: 5
 * End:
 */
