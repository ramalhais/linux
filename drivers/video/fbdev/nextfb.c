// SPDX-License-Identifier: GPL-2.0

// NeXT framebuffer support
// (derived from hpfb, derived from macfb )
// Zach Brown <zab@zabbo.net> 1998
// Pedro Ramalhais <ramalhais@gmail.com> 2022

#include <linux/fb.h>
#include <linux/platform_device.h>

#include <asm/nexthw.h>

MODULE_DESCRIPTION("NeXT Frambuffer driver");
MODULE_ALIAS("platform:nextfb");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pedro Ramalhais <ramalhais@gmail.com>");

static int nextfb_setcolreg(unsigned int regno, unsigned int red, unsigned int green,
				unsigned int blue, unsigned int transp,
				struct fb_info *info)
{
	if (regno < 16) {
		if (info->var.bits_per_pixel == 16) {
			red   >>= 16 - info->var.red.length;
			green >>= 16 - info->var.green.length;
			blue  >>= 16 - info->var.blue.length;
			((u32 *)(info->pseudo_palette))[regno] =
				(red   << info->var.red.offset)   |
				(green << info->var.green.offset) |
				(blue  << info->var.blue.offset);
		}
		// Duplicated in case we need to tweak for bits_per_pixel=2
		if (info->var.bits_per_pixel == 2) {
			red   >>= 16 - info->var.red.length;
			green >>= 16 - info->var.green.length;
			blue  >>= 16 - info->var.blue.length;
			((u32 *)(info->pseudo_palette))[regno] =
				(red   << info->var.red.offset)   |
				(green << info->var.green.offset) |
				(blue  << info->var.blue.offset);
		}
	} else
		pr_info("%s called with regno=%d", __func__, regno);

	return 0;
}

static const struct fb_ops nextfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= nextfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

// Duplicated because we would need more variables and 030 may be a bit different
static int nextfb_probe_030(struct platform_device *dev)
{
	struct fb_info *info;

	if (!MACH_IS_NEXT) {
		pr_err("This computer is not a NeXT. Not setting up framebuffer.\n");
		return -ENXIO;
	}

	info = framebuffer_alloc(sizeof(u32) * 16, &dev->dev);
	if (!info) {
		pr_err("Failed to allocate framebuffer device");
		return -ENOMEM;
	}

// Assuming the NeXT Computer (030) uses the same framebuffer as the NeXT Mono
#define NEXTFB_ADDR	0x0b000000UL
#define NEXTFB_SIZE	0x3a800UL	// 0x3a800+backing store(frame2)=256KB
#define NEXTFB_WIDTH	1120
#define NEXTFB_WIDTH_V	1152		// try 1152 or 1120
#define NEXTFB_HEIGHT	832
#define NEXTFB_HEIGHT_V	910		// try 910 or 936
#define NEXTFB_BPP	2

	info->fix.smem_start	= NEXTFB_ADDR;
	info->fix.smem_len	= NEXTFB_SIZE;
	info->screen_base	= ioremap(info->fix.smem_start, info->fix.smem_len);

	fb_info(info, "Hardcoded %dx%dx%dbpp framebuffer @ 0x%x+0x%x mapped @ 0x%x\n",
		NEXTFB_WIDTH,
		NEXTFB_HEIGHT,
		NEXTFB_BPP,
		(unsigned int)(info->fix.smem_start),
		(unsigned int)(info->fix.smem_len),
		(unsigned int)(info->screen_base)
	);

	// Before we go any further and blow up somewhere because of missing MMU mappings,
	// let's see if we can write to framebuffer memory and blow up here instead.
	*(volatile unsigned char *)(info->screen_base) = 0x00;

	strncpy(info->fix.id, "NeXT (030) Mono", 16);
	info->fix.line_length	= NEXTFB_WIDTH_V>>2;	// 1152 * 2bits per pixel / 8bits per byte = 1152*1/4. >> 2 divides by 2 two times (4).
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_TRUECOLOR;
	info->fix.accel		= FB_ACCEL_NONE;
	info->var.red		= (struct fb_bitfield){0, 2, 0};
	info->var.green		= (struct fb_bitfield){0, 2, 0};
	info->var.blue		= (struct fb_bitfield){0, 2, 0};
	info->var.grayscale	= 0;
	info->var.xres		= NEXTFB_WIDTH;
	info->var.yres		= NEXTFB_HEIGHT;
	info->var.xres_virtual	= NEXTFB_WIDTH_V;
	info->var.yres_virtual	= NEXTFB_HEIGHT_V;
	info->var.bits_per_pixel= NEXTFB_BPP;
	info->var.activate	= FB_ACTIVATE_NOW;
	info->var.height	= 274;
	info->var.width		= 195;	/* 14" monitor */
	info->var.vmode		= FB_VMODE_NONINTERLACED;

	info->fbops = &nextfb_ops;
	info->flags = FBINFO_READS_FAST;
	info->pseudo_palette = info->par; // par is allocated in framebuffer_alloc()

	fb_invert_cmaps();

	if (register_framebuffer(info) < 0) {
		fb_err(info, "Unable to register NeXT frame buffer.\n");
		iounmap(info->screen_base);
		framebuffer_release(info);
		return -EINVAL;
	}

	fb_info(info, "Finished probing NeXT frame buffer.\n");
	return 0;
}

static int nextfb_probe(struct platform_device *dev)
{
	int bpp;
	struct nextfb_businfo frame;
	struct fb_info *info;

	if (!MACH_IS_NEXT) {
		pr_err("This computer is not a NeXT. Not setting up framebuffer.\n");
		return -ENXIO;
	}

	info = framebuffer_alloc(sizeof(u32) * 16, &dev->dev);
	if (!info) {
		pr_err("Failed to allocate framebuffer device");
		return -ENOMEM;
	}
	fb_info(info, "Probing NeXT frame buffer.\n");

	if (prom_info.fbinfo.pixels_pword == 0) {
		fb_err(info, "Pixels Per Word can't be 0. This is probably a NeXT Computer 68030.\n");
		framebuffer_release(info);
		return nextfb_probe_030(dev);
	}

	if (prom_info.fbinfo.pixels_pword != 2 && prom_info.fbinfo.pixels_pword != 16) {
		fb_err(info, "Unknown value %d for Pixels Per Word.\n", prom_info.fbinfo.pixels_pword);
		framebuffer_release(info);
		return -ENXIO;
	}

	bpp = 32/prom_info.fbinfo.pixels_pword;

#define NEXTFB_FRAME 1	// frame 1 is the framebuffer
	frame = prom_info.fbinfo.frames[NEXTFB_FRAME];

	info->fix.smem_start	= frame.phys;
	info->fix.smem_len	= frame.len;
	info->screen_base	= ioremap(info->fix.smem_start, info->fix.smem_len);

	fb_info(info, "Detected %dx%dx%dbpp (%dppw) framebuffer @ 0x%x+0x%x mapped @ 0x%x\n",
		prom_info.fbinfo.vispixx,
		prom_info.fbinfo.height,
		bpp,
		prom_info.fbinfo.pixels_pword,
		(unsigned int)(info->fix.smem_start),
		(unsigned int)(info->fix.smem_len),
		(unsigned int)(info->screen_base)
	);

	// Before we go any further and blow up somewhere because of missing MMU mappings,
	// let's see if we can write to framebuffer memory and blow up here instead.
	*(volatile unsigned char *)(info->screen_base) = 0x00;

	info->fix.line_length	= prom_info.fbinfo.line_length;
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_TRUECOLOR;
	info->fix.accel		= FB_ACCEL_NONE;

	info->var.grayscale		= 0;
	info->var.xres			= prom_info.fbinfo.vispixx;
	info->var.yres			= prom_info.fbinfo.height;
	info->var.xres_virtual		= prom_info.fbinfo.realpixx; // try info->var.xres ?
	info->var.yres_virtual		= info->var.yres;
	info->var.bits_per_pixel	= bpp;
	info->var.activate		= FB_ACTIVATE_NOW;
	info->var.height		= 274;
	info->var.width			= 195;	// 14" monitor
	info->var.vmode			= FB_VMODE_NONINTERLACED;

	for (int i = 0; i < 6; i++) {
		if (i == NEXTFB_FRAME)
			continue;

		if (prom_info.fbinfo.frames[i].phys || prom_info.fbinfo.frames[i].virt || prom_info.fbinfo.frames[i].len) {
			// This seems to be the rest of the memory chip on the framebuffer
			// Mono: 239616(fb)+22464(mistery) = 262080 bytes. 256KB is 262144 bytes, so fb+mistery+64=256KB
			// Color: 1916928(fb)+179712(mistery) = 2096640 bytes. 2MB=2*1024*1024=2097152, so fb+mistery+512=2MB
			fb_info(info, "Backing store memory: #%d @ 0x%x+%x\n",
				i,
				prom_info.fbinfo.frames[i].phys,
				prom_info.fbinfo.frames[i].len
			);
		}
	}

	info->fbops = &nextfb_ops;
	info->flags = FBINFO_READS_FAST;
	info->pseudo_palette = info->par; // par is allocated in framebuffer_alloc()

	if (bpp == 16) {
		strncpy(info->fix.id,	"NeXT C16", 16);
		info->var.red		= (struct fb_bitfield){12, 4, 0};
		info->var.green		= (struct fb_bitfield){8, 4, 0};
		info->var.blue		= (struct fb_bitfield){4, 4, 0};
	} else if (bpp == 2) {
		strncpy(info->fix.id,	"NeXT Mono", 16);
		info->var.red		= (struct fb_bitfield){0, 2, 0};
		info->var.green		= (struct fb_bitfield){0, 2, 0};
		info->var.blue		= (struct fb_bitfield){0, 2, 0};
		fb_invert_cmaps();
	} else
		strncpy(info->fix.id,	"NeXT Unknown", 16);


	if (register_framebuffer(info) < 0) {
		pr_err("Unable to register NeXT frame buffer.\n");
		iounmap(info->screen_base);
		// fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
		return -EINVAL;
	}

	fb_info(info, "Finished probing NeXT frame buffer.\n");
	return 0;
}

static struct platform_driver nextfb_driver = {
	.probe	= nextfb_probe,
	.driver	= {
		.name	= "nextfb",
	},
};

// module_platform_driver(nextfb_driver);
// module_platform_driver_probe(nextfb_driver, nextfb_probe);

static struct platform_device nextfb_device = {
	.name	= "nextfb",
};

static int nextfb_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&nextfb_driver);

	if (!ret) {
		ret = platform_device_register(&nextfb_device);
		if (ret)
			platform_driver_unregister(&nextfb_driver);
	}
	return ret;
}

module_init(nextfb_init);
