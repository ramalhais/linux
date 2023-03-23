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

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <asm/nexthw.h>
#include <asm/nextints.h>

#include <scsi/scsi_host.h>

#include "esp_scsi.h"

#define DRV_MODULE_NAME "next_scsi"

#define	NEXT_SCSI_DMA_ENDALIGNMENT 16
#define NEXT_SCSI_DMA_REGS_OFFSET 0x20
#define NEXT_SCSI_ID 7
#define NEXT_SCSI_HZ 20000000 // 10000000

static void next_scsi_esp_write8(struct esp *esp, u8 val, unsigned long reg)
{
	*(volatile u8 *)(esp->regs + reg) = val;
}

static u8 next_scsi_esp_read8(struct esp *esp, unsigned long reg)
{
	return *(volatile u8 *)(esp->regs + reg);
}

static int next_scsi_esp_irq_pending(struct esp *esp)
{
	if (next_scsi_esp_read8(esp, ESP_STATUS) & ESP_STAT_INTR)
		return 1;
	return 0;
}

static void next_scsi_esp_reset_dma(struct esp *esp)
{
	// vdma_disable ((int)esp->dma_regs);

	struct next_dma_channel *scsi_dma = (struct next_dma_channel *)NEXT_SCSI_DMA_BASE;
	scsi_dma->csr = DMA_RESET;
	// *(volatile u8 *)(esp->dma_regs) = ESPCTRL_FLUSH|ESPCTRL_RESET|ESPCTRL_ENABLE_INT|ESPCTRL_CLK20MHz/*ESPCTRL_MODE_DMA*/;
	*(volatile u8 *)(esp->dma_regs) = ESPCTRL_ENABLE_INT|ESPCTRL_MODE_DMA;
}

static void next_scsi_esp_dma_drain(struct esp *esp)
{
	/* nothing to do */
	struct next_dma_channel *scsi_dma = (struct next_dma_channel *)NEXT_SCSI_DMA_BASE;
	scsi_dma->csr |= DMA_SETENABLE|DMA_SETTMEM;
	*(volatile u8 *)(esp->dma_regs) |= ESPCTRL_FLUSH;
}

static void next_scsi_esp_dma_invalidate(struct esp *esp)
{
	// vdma_disable ((int)esp->dma_regs);
	struct next_dma_channel *scsi_dma = (struct next_dma_channel *)NEXT_SCSI_DMA_BASE;
	scsi_dma->csr = DMA_RESET;
}

static void next_scsi_esp_send_dma_cmd(struct esp *esp, u32 addr, u32 esp_count,
				  u32 dma_count, int write, u8 cmd)
{
	struct next_dma_channel *scsi_dma = (struct next_dma_channel *)NEXT_SCSI_DMA_BASE;
	u32 dma_end;

	BUG_ON(!(cmd & ESP_CMD_DMA));

	next_scsi_esp_write8(esp, (esp_count >> 0) & 0xff, ESP_TCLOW);
	next_scsi_esp_write8(esp, (esp_count >> 8) & 0xff, ESP_TCMED);

	// vdma_disable ((int)esp->dma_regs);
	scsi_dma->csr = 0;

	if (write) {
		*(volatile u8 *)(esp->dma_regs) &= ~ESPCTRL_DMA_READ;
		scsi_dma->csr = DMA_RESET|DMA_SETTMEM|(NEXT_IS_TURBO ? DMA_INITDMA_TURBO : DMA_INITDMA);
		// vdma_set_mode ((int)esp->dma_regs, DMA_MODE_READ);
	} else {
		*(volatile u8 *)(esp->dma_regs) |= ESPCTRL_DMA_READ;
		scsi_dma->csr = DMA_RESET|DMA_SETTDEV|(NEXT_IS_TURBO ? DMA_INITDMA_TURBO : DMA_INITDMA);
		// vdma_set_mode ((int)esp->dma_regs, DMA_MODE_WRITE);
	}

	// vdma_set_addr ((int)esp->dma_regs, addr);
	// vdma_set_count ((int)esp->dma_regs, dma_count);
	// vdma_enable ((int)esp->dma_regs);
	scsi_dma->start = addr;//dd_next
	dma_end = addr+dma_count;//dd_limit
	scsi_dma->end = (dma_end+NEXT_SCSI_DMA_ENDALIGNMENT-1)&~(NEXT_SCSI_DMA_ENDALIGNMENT-1);

	// rx = rx->next;

	// Set this up only if setting DMA_SETCHAIN in rxd->csr (chained DMA)
	// rxd->next_start = rx->p_data;//dd_start // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts
	// rxd->next_end = rx->p_data+RXBUFLEN;//dd_stop // set to 0 in netbsd maybe because they don't use CHAINED DMA interrupts

	scsi_dma->csr = DMA_SETENABLE|(write ? DMA_SETTMEM : DMA_SETTDEV)/*|DMA_SETCHAIN*/;

	scsi_esp_cmd(esp, cmd);
}

static int next_scsi_esp_dma_error(struct esp *esp)
{
	// u32 enable = vdma_get_enable((int)esp->dma_regs);

	// if (enable & (R4030_MEM_INTR|R4030_ADDR_INTR))
	// 	return 1;

	return 0;
}

static const struct esp_driver_ops next_scsi_esp_ops = {
	.esp_write8	=	next_scsi_esp_write8,
	.esp_read8	=	next_scsi_esp_read8,
	.irq_pending	=	next_scsi_esp_irq_pending,
	.reset_dma	=	next_scsi_esp_reset_dma,
	.dma_drain	=	next_scsi_esp_dma_drain,
	.dma_invalidate	=	next_scsi_esp_dma_invalidate,
	.send_dma_cmd	=	next_scsi_esp_send_dma_cmd,
	.dma_error	=	next_scsi_esp_dma_error,
};

irqreturn_t next_scsi_dma_intr(int irq, void *dev_id)
{
	struct esp *esp = dev_id;
	unsigned long flags;
	irqreturn_t ret;
	struct next_dma_channel *scsi_dma = (struct next_dma_channel *)NEXT_SCSI_DMA_BASE;
	u32 csr;

	spin_lock_irqsave(esp->host->host_lock, flags);
	// ret = IRQ_NONE;

	// if (!next_irq_pending(NEXT_IRQ_SCSI_DMA))
	// 	goto done;

	ret = IRQ_HANDLED;
	csr = scsi_dma->csr;
	if (csr&DMA_SETTMEM) {
		// Device To Memory (Receive)
		// Handle received data in DMA memory?
		// Setup (re-arm) DMA for next transfer? or do this when sending cmd?
		scsi_dma->csr = DMA_RESET;
	} else {
		// Memory to Device (Transmit)
		// This is a confirmation that data was received by the device
		scsi_dma->csr = DMA_RESET;
	}
	// data_len = scsi_dma->start - scsi_dma->next_start - 4;

// done:
	spin_unlock_irqrestore(esp->host->host_lock, flags);
	return ret;
}

static int next_scsi_probe(struct platform_device *dev)
{
	struct scsi_host_template *tpnt = &scsi_esp_template;
	struct Scsi_Host *host;
	struct esp *esp;
	// struct resource *res;
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

	esp->flags = ESP_FLAG_USE_FIFO|ESP_FLAG_NO_SELAS|ESP_FLAG_NO_SA3/*|ESP_FLAG_NO_DMA_MAP*/; // Previous does not implement SELAS or SA3. Might work on real hardware (probably not SA3).

	// res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	// if (!res)
	// 	goto fail_unlink;

	esp->regs = (void __iomem *)NEXT_SCSI_BASE;//res->start
	// if (!esp->regs)
	// 	goto fail_unlink;

	// res = platform_get_resource(dev, IORESOURCE_MEM, 1);
	// if (!res)
	// 	goto fail_unlink;

	esp->dma_regs = (void __iomem *)(NEXT_SCSI_BASE + NEXT_SCSI_DMA_REGS_OFFSET);//res->start

	esp->command_block = dma_alloc_coherent(esp->dev, 16,
						&esp->command_block_dma,
						GFP_KERNEL);
	if (!esp->command_block)
		goto fail_unmap_regs;

	host->irq = NEXT_IRQ_SCSI;//err = platform_get_irq(dev, 0);
	// host->irq = IRQ_AUTO_3;//err = platform_get_irq(dev, 0);
	// if (err < 0)
	// 	goto fail_unmap_command_block;
	err = request_irq(host->irq, scsi_esp_intr, 0, DRV_MODULE_NAME, esp);
	// err = request_irq(host->irq, scsi_esp_intr, IRQF_SHARED, DRV_MODULE_NAME, esp);
	if (err < 0)
		goto fail_unmap_command_block;

	err = request_irq(NEXT_IRQ_SCSI_DMA, next_scsi_dma_intr, 0, DRV_MODULE_NAME " DMA", esp);
	// err = request_irq(IRQ_AUTO_6, next_scsi_dma_intr, IRQF_SHARED, DRV_MODULE_NAME " DMA", esp);
	if (err < 0)
		goto fail_free_irq;

	// next_intmask_enable(NEXT_IRQ_SCSI-NEXT_IRQ_BASE);
	// next_intmask_enable(NEXT_IRQ_SCSI_DMA-NEXT_IRQ_BASE);

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
// fail_unlink:
	scsi_host_put(host);
fail:
	return err;
}

static int next_scsi_remove(struct platform_device *dev)
{
	struct esp *esp = dev_get_drvdata(&dev->dev);
	unsigned int irq = esp->host->irq;

	scsi_esp_unregister(esp);

	// next_intmask_disable(NEXT_IRQ_SCSI-NEXT_IRQ_BASE);
	// next_intmask_disable(NEXT_IRQ_SCSI_DMA-NEXT_IRQ_BASE);
	// free_irq(irq, esp);
	// free_irq(IRQ_AUTO_6, esp);
	free_irq(irq, esp);
	free_irq(NEXT_IRQ_SCSI_DMA, esp);

	dma_free_coherent(esp->dev, 16,
			  esp->command_block,
			  esp->command_block_dma);

	scsi_host_put(esp->host);

	return 0;
}

/* work with hotplug and coldplug */
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
