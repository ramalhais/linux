// SPDX-License-Identifier: GPL-2.0-only

// next_scsi.c: ESP front-end for NeXT Computer/cube/station.
// based on jazz_esp.c: Copyright (C) 2007 Thomas Bogendörfer (tsbogend@alpha.frankende)
// 2022 Pedro Ramalhais <ramalhais@gmail.com>

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <asm/nexthw.h>
#include <asm/nextints.h>

#include <scsi/scsi_host.h>

#include "esp_scsi.h"

#define DRV_MODULE_NAME "next_scsi"

#if defined(CONFIG_NEXT_SCSI_DEBUG)
#define dprintk printk
#else
#define dprintk(...) do { } while (0)
#endif

#define	NEXT_SCSI_DMA_ENDALIGNMENT 16
#define NEXT_SCSI_DMA_REGS_OFFSET 0x20
// #define NEXT_SCSI_DMA_MAXSIZE 4096 // netbsd says 64k?
#define NEXT_SCSI_ID 7
#define NEXT_SCSI_HZ 20000000
#define NEXT_ESP_DELAY 100 // us(microseconds)

u8 dma_regs;
#define ESPCTRL_INIT(b)		dma_regs=b; *(volatile u8 *)(esp->dma_regs) = dma_regs; udelay(NEXT_ESP_DELAY);
#define ESPCTRL_SET(b)		dma_regs|=b; *(volatile u8 *)(esp->dma_regs) = dma_regs; udelay(NEXT_ESP_DELAY);
#define ESPCTRL_CLEAR(b)	dma_regs&=~b; *(volatile u8 *)(esp->dma_regs) = dma_regs; udelay(NEXT_ESP_DELAY);

struct next_dma_channel *scsi_dma;

static void next_scsi_esp_write8(struct esp *esp, u8 val, unsigned long reg)
{
	*(volatile u8 *)(esp->regs + reg) = val;
}

static u8 next_scsi_esp_read8(struct esp *esp, unsigned long reg)
{
	return *(volatile u8 *)(esp->regs + reg);
}

static int next_scsi_irq_pending(struct esp *esp)
{
	// u8 status = next_scsi_esp_read8(esp, ESP_STATUS);
	// 	shost_printk(KERN_INFO, esp->host,
	// 		"next_scsi_irq_pending(): ESP_STATUS=0x%hhx", status);

	// u8 interrupt_status = next_scsi_esp_read8(esp, ESP_INTRPT);
	// 	shost_printk(KERN_INFO, esp->host,
	// 		"next_scsi_irq_pending(): ESP_INTRPT=0x%hhx", interrupt_status);

	// if (status & ESP_STAT_INTR)
		return next_irq_pending(esp->host->irq);

	// shost_printk(KERN_INFO, esp->host, "next_scsi_irq_pending(): Missing ESP_STAT_INTR !!!");

	// return 0;
}

// Called from esp_scsi.c esp_bootup_reset()
static void next_scsi_reset_dma(struct esp *esp)
{
	// *(volatile u8 *)(esp->dma_regs) = ESPCTRL_FLUSH|ESPCTRL_RESET|ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz/*ESPCTRL_MODE_DMA*/;

	// *(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_MODE_DMA; // works on Previous

	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz|ESPCTRL_RESET;
	// udelay(NEXT_ESP_DELAY);
	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz;
	// udelay(NEXT_ESP_DELAY);

	scsi_dma->csr = DMA_RESET;
	dprintk(KERN_ERR "next_scsi_reset_dma()\n");
}

static void next_scsi_dma_drain(struct esp *esp)
{
	u32 prev_addr = 0;
	int try = 0;
	u16 esp_count;

//	udelay(20);
	dprintk(KERN_ERR "dma_regs+1 =0x%x\n", *(volatile u8 *)(esp->dma_regs+1));

	#define ESP_FLUSH_MAX_TRIES 4 // 16/4 = DMA_ALIGNMENT/FIFO_ALIGNMENT
	while ((try < ESP_FLUSH_MAX_TRIES) /*&& (scsi_dma->start > prev_addr)*/) {
		prev_addr = scsi_dma->start;

//		scsi_dma->csr |= DMA_SETENABLE|DMA_SETTMEM;
		*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz|ESPCTRL_MODE_DMA|ESPCTRL_DMA_READ|ESPCTRL_FLUSH;
//		udelay(5);
		*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz|ESPCTRL_MODE_DMA|ESPCTRL_DMA_READ;
//		udelay(5);

		try++;
		dprintk(KERN_ERR "next_scsi_dma_drain(): Flushing. Try=%d\n", try);
	}
	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz;
	esp_count = next_scsi_esp_read8(esp, ESP_TCLOW);
	esp_count |= next_scsi_esp_read8(esp, ESP_TCMED)<<8;
	dprintk(KERN_ERR "esp_count=%d\n", esp_count);
	scsi_dma->csr = DMA_RESET;

	dprintk(KERN_ERR "next_scsi_dma_drain(): Flushed. tries=%d cur_addr=0x%x scsi_dma->start=0x%x\n", try, prev_addr, scsi_dma->start);
}

static void next_scsi_dma_invalidate(struct esp *esp)
{
	dprintk(KERN_ERR "next_scsi_dma_invalidate(): START: scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", (volatile u32)(scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));
//	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz;
//	scsi_dma->csr = DMA_RESET;
	// scsi_dma->csr = 0;
//	printk(KERN_ERR "next_scsi_dma_invalidate(): END: scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", (volatile u32)(scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));
}

static void next_scsi_send_dma_cmd(struct esp *esp, u32 addr, u32 esp_count,
				  u32 dma_count, int write, u8 cmd)
{
	BUG_ON(!(cmd & ESP_CMD_DMA));

	dprintk(KERN_ERR "next_scsi_send_dma_cmd(): START: addr=0x%x, esp_count=%d, dma_count=%d, write=%d, scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", addr, esp_count, dma_count, write, (volatile u32)(scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));

	scsi_esp_cmd(esp, ESP_CMD_FLUSH); // FIXME: test

	// scsi_dma->csr = 0;
	scsi_dma->csr = DMA_RESET|(write ? DMA_SETTMEM : DMA_SETTDEV)/*|(NEXT_IS_TURBO ? DMA_INITDMA_TURBO : DMA_INITDMA)*/;

	scsi_dma->next_initbuf = addr; // not used in netbsd
	scsi_dma->start = addr;
	scsi_dma->end = ((addr+dma_count+NEXT_SCSI_DMA_ENDALIGNMENT-1)&(~(NEXT_SCSI_DMA_ENDALIGNMENT-1))); //|(write ? DMA_WRITE : 0);
	scsi_dma->csr = DMA_SETENABLE|(write ? DMA_SETTMEM : DMA_SETTDEV)/*|DMA_SETCHAIN*/;
	// scsi_dma->csr |= DMA_SETENABLE;

	// scsi_dma->next_end = 0; //TEST dd_stop
	// if (!NEXT_IS_TURBO) {
	// 	scsi_dma->next_start = 0;
	// 	scsi_dma->saved_end = scsi_dma->end;
	// } else {
	// 	scsi_dma->next_start = addr;
	// }
	// scsi_dma->next_end = 0;
	// if (!NEXT_IS_TURBO)
	// 	scsi_dma->saved_end = scsi_dma->end;
	// rx = rx->next;

	// Set this up only if setting DMA_SETCHAIN in rxd->csr (chained DMA)
	// rxd->next_start = rx->p_data;//dd_start // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts
	// rxd->next_end = rx->p_data+RXBUFLEN;//dd_stop // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts

	// write is SCSI to Memory
	// *(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz|ESPCTRL_MODE_DMA|(write ? ESPCTRL_DMA_READ : 0);

	// scsi_esp_cmd(esp, ESP_CMD_FLUSH);

	next_scsi_esp_write8(esp, (esp_count >> 0) & 0xff, ESP_TCLOW);
	next_scsi_esp_write8(esp, (esp_count >> 8) & 0xff, ESP_TCMED);

	// scsi_esp_cmd(esp, ESP_CMD_DMA);
	scsi_esp_cmd(esp, ESP_CMD_NULL); // FIXME: in NextMach code

	scsi_esp_cmd(esp, cmd);
//	udelay(NEXT_ESP_DELAY);

	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz|ESPCTRL_MODE_DMA|(write ? ESPCTRL_DMA_READ : 0); // Go my son!
//	udelay(NEXT_ESP_DELAY);

	dprintk(KERN_ERR "next_scsi_send_dma_cmd(): END: scsi_dma->next_initbuf=0x%x, scsi_dma->start=0x%x, scsi_dma->end=0x%x, scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", (volatile u32)scsi_dma->next_initbuf, (volatile u32)scsi_dma->start, (volatile u32)scsi_dma->end, (volatile u32)(scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));
}

static int next_scsi_dma_error(struct esp *esp)
{
	// while (scsi_dma->csr&DMA_READING) {
	// 	printk(KERN_ERR "next_scsi_dma_error(): DMA_READING. delaying %d", NEXT_ESP_DELAY);
	// 	udelay(NEXT_ESP_DELAY);
	// }

	if (scsi_dma->csr&(DMA_BUSERR|DMA_OVERFLOW)) {
		printk(KERN_ERR "next_scsi_dma_error(): scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", (scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));
		scsi_dma->csr = DMA_RESET/*|DMA_BUSERR|DMA_OVERFLOW|DMA_READING*/;
		// next_scsi_dma_invalidate(esp);
		// udelay(NEXT_ESP_DELAY);
		printk(KERN_ERR "next_scsi_dma_error(): scsi_dma->csr=0x%x esp->dma_regs=0x%x\n", (scsi_dma->csr), *(volatile u8 *)(esp->dma_regs));
		return 1;
	}

	return 0;
}

static u32 next_scsi_dma_length_limit(struct esp *esp, u32 dma_addr, u32 dma_len)
{
	dprintk(KERN_ERR "next_scsi_dma_length_limit(): dma_addr=0x%x dma_len=%d\n",
		dma_addr, dma_len);
	return dma_len;
}

static const struct esp_driver_ops next_scsi_esp_ops = {
	.esp_write8		= next_scsi_esp_write8,
	.esp_read8		= next_scsi_esp_read8,
	.irq_pending		= next_scsi_irq_pending,
	.dma_length_limit	= next_scsi_dma_length_limit,
	.reset_dma		= next_scsi_reset_dma,
	.dma_drain		= next_scsi_dma_drain,
	.dma_invalidate		= next_scsi_dma_invalidate,
	.send_dma_cmd		= next_scsi_send_dma_cmd,
	.dma_error		= next_scsi_dma_error,
};

irqreturn_t next_scsi_dma_intr(int irq, void *dev_id);
irqreturn_t next_scsi_dma_intr(int irq, void *dev_id)
{
	struct esp *esp = dev_id;
	unsigned long flags;
	u32 csr = scsi_dma->csr;

	dprintk(KERN_ERR "next_scsi_dma_intr(): START scsi_dma->csr=0x%x esp->dma_regs=0x%x\n",
		csr, *(volatile u8 *)(esp->dma_regs));
	spin_lock_irqsave(esp->host->host_lock, flags);

	if (csr&DMA_CINT) {
		scsi_dma->csr = csr|DMA_CLEARCHAINI;
	} else {
		scsi_dma->csr = DMA_RESET;
	}
	// if (csr&DMA_SETTMEM) {
	// 	// Device To Memory (Receive)
	// 	// Handle received data in DMA memory?
	// 	// Setup (re-arm) DMA for next transfer? or do this when sending cmd?
	// 	scsi_dma->csr = DMA_RESET;
	// } else {
	// 	// Memory to Device (Transmit)
	// 	// This is a confirmation that data was received by the device
	// 	scsi_dma->csr = DMA_RESET;
	// }
	// data_len = scsi_dma->start - scsi_dma->next_start - 4;

	spin_unlock_irqrestore(esp->host->host_lock, flags);
	dprintk(KERN_ERR "next_scsi_dma_intr(): END scsi_dma->csr=0x%x esp->dma_regs=0x%x\n",
		scsi_dma->csr, *(volatile u8 *)(esp->dma_regs));
	return IRQ_HANDLED;
}

static int next_scsi_probe(struct platform_device *dev)
{
	struct scsi_host_template *tpnt = (struct scsi_host_template *)&scsi_esp_template;
	struct Scsi_Host *host;
	struct esp *esp;
	int err;

	host = scsi_host_alloc(tpnt, sizeof(struct esp));

	err = -ENOMEM;
	if (!host)
		goto fail;

	host->max_id = NEXT_SCSI_ID + 1;
	esp = shost_priv(host);

	esp->host = host;
	esp->dev = &dev->dev;
	esp->ops = &next_scsi_esp_ops;

	esp->flags = ESP_FLAG_USE_FIFO|ESP_FLAG_NO_SELAS|ESP_FLAG_NO_SA3; // Previous does not implement SELAS or SA3. Might work on real hardware (probably not SA3).
	// esp->flags = ESP_FLAG_USE_FIFO/*|ESP_FLAG_NO_SELAS|ESP_FLAG_NO_SA3*//*|ESP_FLAG_NO_DMA_MAP*/; // Real hardware?

	esp->regs = ioremap(NEXT_SCSI, NEXT_SCSI_DMA_REGS_OFFSET);

	if (!esp->regs)
		goto fail_unlink;

	esp->dma_regs = ioremap(NEXT_SCSI+NEXT_SCSI_DMA_REGS_OFFSET, sizeof(u8));
	scsi_dma = ioremap(NEXT_CSR_SCSI, sizeof(struct next_dma_channel));

	struct fd_cntrl_regs *fdcp = ioremap(NEXT_FLOPPY, sizeof(struct fd_cntrl_regs));
	pr_info("flpctl initial: 0x%x", fdcp->flpctl);
	fdcp->flpctl &= ~FLC_82077_SEL;
	pr_info("flpctl deselect floppy: 0x%x", fdcp->flpctl);

	esp->command_block = dma_alloc_coherent(esp->dev, 16,
						&esp->command_block_dma,
						GFP_KERNEL);
	if (!esp->command_block)
		goto fail_unmap_regs;

	host->irq = NEXT_IRQ_SCSI;
	err = request_irq(host->irq, scsi_esp_intr, 0, DRV_MODULE_NAME, esp);
	if (err < 0)
		goto fail_unmap_command_block;

	err = request_irq(NEXT_IRQ_SCSI_DMA, next_scsi_dma_intr, 0, DRV_MODULE_NAME " DMA", esp);
	if (err < 0)
		goto fail_free_irq;

	esp->scsi_id = NEXT_SCSI_ID;
	esp->host->this_id = esp->scsi_id;
	esp->scsi_id_mask = (1 << esp->scsi_id);
	esp->cfreq = NEXT_SCSI_HZ;

	dev_set_drvdata(&dev->dev, esp);

	err = scsi_esp_register(esp);
	if (err)
		goto fail_free_irq;

	return 0;

fail_free_irq:
	free_irq(host->irq, esp);
fail_unmap_command_block:
	dma_free_coherent(esp->dev, 16,
			  esp->command_block,
			  esp->command_block_dma);
fail_unmap_regs:
fail_unlink:
	scsi_host_put(host);
fail:
	return err;
}

static void next_scsi_remove(struct platform_device *dev)
{
	struct esp *esp = dev_get_drvdata(&dev->dev);

	scsi_esp_unregister(esp);

	free_irq(esp->host->irq, esp);
	free_irq(NEXT_IRQ_SCSI_DMA, esp);

	dma_free_coherent(esp->dev, 16,
			  esp->command_block,
			  esp->command_block_dma);

	scsi_host_put(esp->host);
}

MODULE_ALIAS("platform:next_scsi");

static struct platform_driver next_scsi_driver = {
	.probe		= next_scsi_probe,
	.remove		= next_scsi_remove,
	.driver	= {
		.name	= DRV_MODULE_NAME,
	},
};

// module_platform_driver(next_scsi_driver);

static struct platform_device next_scsi_device = {
	.name = DRV_MODULE_NAME,
};

static int next_scsi_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&next_scsi_driver);

	if (!ret) {
		ret = platform_device_register(&next_scsi_device);
		if (ret)
			platform_driver_unregister(&next_scsi_driver);
	}
	return ret;
}

module_init(next_scsi_init);

MODULE_DESCRIPTION("NeXT ESP SCSI driver");
MODULE_AUTHOR("Pedro Ramalhais <ramalhais@gmail.com>");
MODULE_LICENSE("GPL");
