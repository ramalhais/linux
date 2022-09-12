/*
 *	NeXT framebuffer support 
 *		(derived from hpfb, derived from macfb )
 *	Zach Brown <zab@zabbo.net> 1998
 * 
 */

// #include <linux/module.h>
// #include <linux/kernel.h>
// #include <linux/sched.h>
// #include <linux/errno.h>
// #include <linux/string.h>
// #include <linux/mm.h>
// #include <linux/tty.h>
// //#include <linux/malloc.h>
// #include <linux/delay.h>
// #include <linux/init.h>
#include <linux/fb.h>
// #include <linux/dio.h>
#include <linux/platform_device.h>

#include <asm/nexthw.h>

static struct fb_fix_screeninfo	nextfb_fix;
static struct fb_var_screeninfo	nextfb_var;


// #define arraysize(x)    (sizeof(x)/sizeof(*(x)))

// static struct display disp;
// static struct fb_info fb_info;

// #define NEXT_C_NONE 0
// #define NEXT_C_C16 1
// #define NEXT_C_MONO 2

// #ifdef FBCON_HAS_CFB16
// static struct { u_short blue, green, red, pad; } palette[256];
// static union {
// #ifdef FBCON_HAS_CFB16
//     u16 cfb16[16];
// #endif
// } fbcon_cmap;
// #endif

// int next_ctype=NEXT_C_NONE;

// unsigned long fb_start,fb_size;

// static struct fb_var_screeninfo nextfb_defined = {
// 	.xres = 0,
// 	.yres = 0,
// 	.xres_virtual = 0,
// 	.yres_virtual = 0,	/* W,H, W, H (virtual) load xres,xres_virtual*/
// 	.xoffset = 0,
// 	.yoffset = 0,		/* virtual -> visible no offset */
// 	.bits_per_pixel = 0,		/* depth -> load bits_per_pixel */
// 	.grayscale = 0,		/* greyscale ? */
// 	.red = {0,0,0},	/* R */
// 	.green = {0,0,0},	/* G */
// 	.blue = {0,0,0},	/* B */
// 	.transp = {0,0,0},	/* transparency */
// 	.nonstd = 0,		/* standard pixel format */
// 	.activate = FB_ACTIVATE_NOW,
// 	.height = 274,
// 	.width = 195,	/* 14" monitor */
// 	.accel_flags = FB_ACCEL_NONE,
// 	.pixclock = 0L,
// 	.left_margin = 0L,
// 	.right_margin = 0L,
// 	.upper_margin = 0L,
// 	.lower_margin = 0L,
// 	.hsync_len = 0L,
// 	.vsync_len = 0L,
// 	.sync = 0,	/* No sync info */
// 	.vmode = FB_VMODE_NONINTERLACED,
// 	.rotate = 0,
// 	.colorspace = 0, //FIXME
// 	.reserved = {0,0,0,0}
// };

// static struct fb_var_screeninfo nextfb_defined = {
// 	.activate = FB_ACTIVATE_NOW,
// 	.height = 274,
// 	.width = 195,	/* 14" monitor */
// 	.accel_flags = FB_ACCEL_NONE,
// 	.vmode = FB_VMODE_NONINTERLACED,
// };

// struct nextfb_par
// {
// };

// static int currcon = 0;
// struct nextfb_par current_par;

// static void nextfb_encode_var(struct fb_var_screeninfo *var, 
// 				struct nextfb_par *par)
// {
// 	int i=0;

// 	switch (next_ctype) {
// #if defined(CONFIG_FB_NEXT_C16) 
//                 case NEXT_C_C16:
// 			var->bits_per_pixel = 16;
// 			var->grayscale=0;
// 			var->red.offset=12;
// 			var->red.length=4;
// 			var->red.msb_right=0;
// 			var->green.offset=8;
// 			var->green.length=4;
// 			var->green.msb_right=0;
// 			var->blue.offset=4;
// 			var->blue.length=4;
// 			var->blue.msb_right=0;
// 			break;
// #endif
// #if defined(CONFIG_FB_NEXT_MONO)
//                 case NEXT_C_MONO:
// 			var->bits_per_pixel = 2;
// 			var->grayscale=1;
// 			var->red.offset=0;
// 			var->red.length=0;
// 			var->red.msb_right=0;
// 			var->green.offset=0;
// 			var->green.length=0;
// 			var->green.msb_right=0;
// 			var->blue.offset=0;
// 			var->blue.length=0;
// 			var->blue.msb_right=0;
// 			break;
// #endif
// 	}
// 	var->xres=prom_info.fbinfo.vispixx;
// 	var->yres=prom_info.fbinfo.height;
// 	var->xres_virtual=prom_info.fbinfo.realpixx;
// 	var->yres_virtual=var->yres;
// 	var->xoffset=0;
// 	var->yoffset=0;
// /* XXX */
// 	var->transp.offset=0;
// 	var->transp.length=0;
// 	var->transp.msb_right=0;
// 	var->nonstd=0;
// 	var->activate=0;
// 	var->height= -1;
// 	var->width= -1;
// 	var->vmode=FB_VMODE_NONINTERLACED;
// 	var->pixclock=0;
// 	var->sync=0;
// 	var->left_margin=0;
// 	var->right_margin=0;
// 	var->upper_margin=0;
// 	var->lower_margin=0;
// 	var->hsync_len=0;
// 	var->vsync_len=0;
// 	for(i=0;i<arraysize(var->reserved);i++)
// 		var->reserved[i]=0;
// }

// static void nextfb_get_par(struct nextfb_par *par)
// {
// 	*par=current_par;
// }

// static int fb_update_var(int con, struct fb_info *info)
// {
// 	return 0;
// }

// static int do_fb_set_var(struct fb_var_screeninfo *var, int isactive)
// {
// 	struct nextfb_par par;
	
// 	nextfb_get_par(&par);
// 	nextfb_encode_var(var, &par);
// 	return 0;
// }

// static int nextfb_getcolreg(unsigned regno, unsigned *red, unsigned *green, unsigned *blue,
//                          unsigned *transp, struct fb_info *info)
// {
// #if defined(FBCON_HAS_CFB16)
// 	if((next_ctype==NEXT_C_C16) && (regno < 16)) {
// /*		*red=fbcon_cfb16_cmap[regno]>>12;
// 		*green=(fbcon_cfb16_cmap[regno]>>8)&0xf;
// 		*blue=(fbcon_cfb16_cmap[regno]>>4)&0xf; */
// 		*red=palette[regno].red;
// 		*green=palette[regno].green;
// 		*blue=palette[regno].blue;

// 	}
// #endif
// 	return 0;
// }		

// static int nextfb_setcolreg(unsigned regno,unsigned  red, unsigned green, unsigned blue,
//                          unsigned transp, struct fb_info *info)
// {
// #if defined(FBCON_HAS_CFB16)
// 	if( (next_ctype==NEXT_C_C16) && (regno < 16)) {
// 		palette[regno].red   = red;
// 		palette[regno].green = green;
// 		palette[regno].blue  = blue;

// 		fbcon_cmap.cfb16[regno] =
// 			((red   & 0xf000) ) |
// 			((green & 0xf000) >>  4) |
// 			((blue  & 0xf000) >> 8);

// 	}
// #endif
// 	return 0;
// }


// static int nextfb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
// 			  struct fb_info *info)
// {
//         if (con == currcon) /* current console ? */
//                 return fb_get_cmap(cmap, kspc,
//                                    nextfb_getcolreg, info);
//         else
//                 if (fb_display[con].cmap.len) /* non default colormap ? */
//                         fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
//                 else
//                         fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
//                                      cmap, kspc ? 0 : 2);

// 	return 0;
// }

// static int nextfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
// 			  struct fb_info *info)
// {
//     int err;

//     if (!fb_display[con].cmap.len) {    /* no colormap allocated? */
//         if ((err = fb_alloc_cmap(&fb_display[con].cmap,
//                                  1<<fb_display[con].var.bits_per_pixel, 0)))
//             return err;
//     }
//     if (con == currcon) {               /* current console? */
//         err = fb_set_cmap(cmap, kspc, nextfb_setcolreg,
//                           info);
//         return err;
//     } else
//         fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
//     return 0;

// }

// static int nextfb_get_var(struct fb_var_screeninfo *var, int con,
// 			 struct fb_info *info)
// {
// 	struct nextfb_par par;
// 	if(con==-1)
// 	{
// 		nextfb_get_par(&par);
// 		nextfb_encode_var(var, &par);
// 	}
// 	else
// 		*var=fb_display[con].var;
// 	return 0;
// }

// static int nextfb_set_var(struct fb_var_screeninfo *var, int con,
// 			 struct fb_info *info)
// {
// 	int err;
	
// 	if ((err=do_fb_set_var(var, 1)))
// 		return err;
// 	return 0;
// }

// static void nextfb_encode_fix(struct fb_fix_screeninfo *fix, 
// 				struct nextfb_par *par)
// {
// 	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

// 	fix->smem_start=fb_start;
// 	fix->smem_len=fb_size;
// 	fix->type = FB_TYPE_PACKED_PIXELS;
// 	fix->xpanstep=0;
// 	fix->ypanstep=0;
// 	fix->ywrapstep=0;
// 	fix->line_length=prom_info.fbinfo.line_length;

// 	switch (next_ctype) {
// #if defined(CONFIG_FB_NEXT_C16) 
//                 case NEXT_C_C16:
// 			fix->visual = FB_VISUAL_TRUECOLOR;
// 			strcpy(fix->id, "NeXT C16");
//                         break;
// #endif
// #if defined(CONFIG_FB_NEXT_MONO)
//                 case NEXT_C_MONO:
// 			fix->visual = FB_VISUAL_MONO01;
// 			strcpy(fix->id, "NeXT Mono"); 
//                         break;
// #endif
// 	}
// }


// static int nextfb_get_fix(struct fb_fix_screeninfo *fix, int con,
// 			 struct fb_info *info)
// {
// 	struct nextfb_par par;
// 	nextfb_get_par(&par);
// 	nextfb_encode_fix(fix, &par);
// 	return 0;
// }

// static int nextfb_ioctl(struct inode *inode, struct file *file, 
// 		       unsigned int cmd, unsigned long arg, int con,
// 		       struct fb_info *info)
// {
// 	return -EINVAL;
// }

// static int nextfb_switch(int con, struct fb_info *info)
// {
// 	do_fb_set_var(&fb_display[con].var,1);
// 	currcon=con;
// 	return 0;
// }

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */

// static void nextfb_blank(int blank, struct fb_info *info)
// {
// 	/* XXX we can diddle the CMD_ bits in the 
// 	dac to handle this.. */
// }

// static int nextfb_open(struct fb_info *info, int user)
// {
// 	/*
// 	 * Nothing, only a usage count for the moment
// 	 */
// 	MOD_INC_USE_COUNT;
// 	return(0);
// }

// static void nextfb_set_disp(int con)
// {
// 	struct fb_fix_screeninfo fix;
// 	struct display *display;
	
// 	if (con >= 0)
// 		display = &fb_display[con];
// 	else
// 		display = &disp;	/* used during initialization */

// 	nextfb_get_fix(&fix, con, 0);

// 	memset(display, 0, sizeof(struct display));
// 	display->screen_base = fix.smem_start;
// 	display->visual = fix.visual;
// 	display->type = fix.type;
// 	display->type_aux = fix.type_aux;
// 	display->ypanstep = fix.ypanstep;
// 	display->ywrapstep = fix.ywrapstep;
// 	display->line_length = fix.line_length;
// 	display->next_line = fix.line_length;
// 		/* XXX see nextfb_blank() ablove :) */
// 	display->can_soft_blank = 0;
// 	display->inverse = 0;
// 	nextfb_get_var(&display->var,-1,&fb_info);

// 	switch (next_ctype) {
// #if defined(CONFIG_FB_NEXT_C16) 
//                 case NEXT_C_C16:
// 			display->dispsw = &fbcon_cfb16;
// 			display->dispsw_data = fbcon_cmap.cfb16;
//                         break;
// #endif
// #if defined(CONFIG_FB_NEXT_MONO)
//                 case NEXT_C_MONO:display->dispsw = &fbcon_cfb2;
//                         break;
// #endif
// 	}
// }

// static int nextfb_release(struct fb_info *info, int user)
// {
// 	MOD_DEC_USE_COUNT;
// 	return(0);
// }

// static struct fb_ops nextfb_ops = {
// 	/* open/release and usage marking */
// 	.fb_open = nextfb_open,
// 	.fb_release = nextfb_release,
// //	.fb_get_fix = nextfb_get_fix,
// //	.fb_get_var = nextfb_get_var,
// //	.fb_set_var = nextfb_set_var,
// //	.fb_get_cmap = nextfb_get_cmap,
// //	.fb_set_cmap = nextfb_set_cmap,
// //	.fb_ioctl = nextfb_ioctl
// };

// __initfunc(void nextfb_init(void))
// {

// 	int i;

// 	switch (prom_info.fbinfo.pixels_pword) {
// #if defined(CONFIG_FB_NEXT_C16) 
//                 case 2:next_ctype=NEXT_C_C16;
//                         break;
// #endif
// #if defined(CONFIG_FB_NEXT_MONO)
//                 case 16:next_ctype=NEXT_C_MONO;
//                         break;
// #endif
//                 default: /* 4? 8? 32? */
// 			panic("unknown display depth %d\n",prom_info.fbinfo.pixels_pword);
//                         break;
// 	}

// 	printk("Using %dx%dx%dbpp frame buffer at 0x%0X\n",
// 		prom_info.fbinfo.vispixx,prom_info.fbinfo.height,32/prom_info.fbinfo.pixels_pword,
// 		prom_info.fbinfo.frames[1].phys);
// 	for(i=0;i<6;i++) {
// 		if(i==1) continue;
// 		if(prom_info.fbinfo.frames[i].phys || prom_info.fbinfo.frames[i].virt ||
// 			prom_info.fbinfo.frames[i].len) {

// 			printk("Mystery vid mem: #%d 0x%0X -> 0x%0X (%d)\n",
// 				i,prom_info.fbinfo.frames[i].phys,
// 				prom_info.fbinfo.frames[i].virt,
// 				prom_info.fbinfo.frames[i].len);
// 		}
// 	}

// 	nextfb_defined.bits_per_pixel=32/prom_info.fbinfo.pixels_pword;
// 	fb_start=prom_info.fbinfo.frames[1].phys;
// 	fb_size=prom_info.fbinfo.frames[1].len;

// 	/*
// 	 *	Fill in the available video resolution
// 	 */
	 
// 	nextfb_defined.xres = prom_info.fbinfo.vispixx;
// 	nextfb_defined.yres = prom_info.fbinfo.height;
// 	nextfb_defined.xres_virtual = nextfb_defined.xres;
// 	nextfb_defined.yres_virtual = nextfb_defined.yres;

// 	/*
// 	 *	Let there be consoles..
// 	 */
// 	strcpy(fb_info.modename, "NeXT");
// 	fb_info.fontname[0]='\0';
// 	fb_info.changevar = NULL;
// 	fb_info.node = -1;
// 	fb_info.fbops = &nextfb_ops;
// 	fb_info.disp = &disp;
// 	fb_info.switch_con = &nextfb_switch;
// 	fb_info.updatevar = &fb_update_var;
// 	fb_info.blank = &nextfb_blank;
// 	do_fb_set_var(&nextfb_defined, 1);

// 	nextfb_get_var(&disp.var, -1, &fb_info);
// 	nextfb_set_disp(-1);

// 	if (register_framebuffer(&fb_info) < 0)
// 		return;

// 	return;
// }

// __initfunc(void nextfb_setup(char *options, int *ints))
// {
// }

static const struct fb_ops nextfb_ops = {
	.owner		= THIS_MODULE,
	// .fb_setcolreg	= q40fb_setcolreg,
	// .fb_fillrect	= cfb_fillrect,
	// .fb_copyarea	= cfb_copyarea,
	// .fb_imageblit	= cfb_imageblit,
};

static int nextfb_probe(struct platform_device *dev)
{
	int bpp;
	struct nextfb_businfo frame;
	struct fb_info *info = framebuffer_alloc(sizeof(u32) * 16, &dev->dev);
	*(volatile unsigned char *)(0xff110000)=0xD3; // Previous debug
	if (!info) {
	*(volatile unsigned char *)(0xff110000)=0xD4; // Previous debug
		return -ENOMEM;
	}
	fb_info(info, "Probing NeXT frame buffer.\n");
	*(volatile unsigned char *)(0xff110000)=0xD5; // Previous debug

	if (!MACH_IS_NEXT) {
	*(volatile unsigned char *)(0xff110000)=0xD6; // Previous debug
		fb_info(info, "This computer is not a NeXT. Bailing out.\n");
		return -ENXIO;
	}

	if (prom_info.fbinfo.pixels_pword <= 0) {
	*(volatile unsigned char *)(0xff110000)=0xD7; // Previous debug
		fb_info(info, "Pixels Per Word can't be 0.\n");
		return -ENXIO;
	}

	if (prom_info.fbinfo.pixels_pword != 2 && prom_info.fbinfo.pixels_pword != 16) {
	*(volatile unsigned char *)(0xff110000)=0xD8; // Previous debug
		fb_info(info, "Unknown value %d for Pixels Per Word.\n", prom_info.fbinfo.pixels_pword);
	}

	bpp = 32/prom_info.fbinfo.pixels_pword;

	frame = prom_info.fbinfo.frames[1];

	fb_info(info, "NeXT fbinfo: %dx%dx%dbpp (%dppw) frames[1]=0x%x+0x%x\n",
		prom_info.fbinfo.vispixx,
		prom_info.fbinfo.height,
		bpp,
		prom_info.fbinfo.pixels_pword,
		frame.phys,
		frame.len
	);

	nextfb_fix.smem_start 	= frame.phys;
	nextfb_fix.smem_len		= frame.len;
	nextfb_fix.line_length 	= prom_info.fbinfo.line_length;
	nextfb_fix.type 		= FB_TYPE_PACKED_PIXELS;
	// nextfb_fix.xpanstep		= 0;
	// nextfb_fix.ypanstep		= 0;
	// nextfb_fix.ywrapstep	= 0;
	nextfb_fix.visual 		= bpp >= 8 ? FB_VISUAL_TRUECOLOR : FB_VISUAL_MONO01;
	if (bpp == 16) {
	*(volatile unsigned char *)(0xff110000)=0xD9; // Previous debug
		strncpy (nextfb_fix.id,	"NeXT C16", 16);
		nextfb_var.red		= (struct fb_bitfield){6, 5, 0};
		nextfb_var.green	= (struct fb_bitfield){11, 5, 0};
		nextfb_var.blue		= (struct fb_bitfield){0, 6, 0};
	} else if (bpp == 2) {
	*(volatile unsigned char *)(0xff110000)=0xDA; // Previous debug
		strncpy (nextfb_fix.id,	"NeXT Mono", 16);
		nextfb_var.grayscale	= 1;
	} else {
	*(volatile unsigned char *)(0xff110000)=0xDB; // Previous debug
		strncpy (nextfb_fix.id,	"NeXT Unknown", 16);
	}
	nextfb_fix.accel		= FB_ACCEL_NONE;
	nextfb_var.xres 		= prom_info.fbinfo.vispixx;
	nextfb_var.yres 		= prom_info.fbinfo.height;
	nextfb_var.xres_virtual = prom_info.fbinfo.realpixx; // try nextfb_var.xres ?
	nextfb_var.yres_virtual = nextfb_var.yres;
	nextfb_var.bits_per_pixel	= bpp;
	nextfb_var.activate		= FB_ACTIVATE_NOW;
	nextfb_var.height 		= 274;
	nextfb_var.width 		= 195;	/* 14" monitor */
	nextfb_var.vmode		= FB_VMODE_NONINTERLACED;
	// nextfb_var.accel_flags 	= FB_ACCEL_NONE;

	fb_info(info, "Scanning other frames[]\n");
	for(int i=0; i<6; i++) {
	*(volatile unsigned char *)(0xff110000)=0xDC; // Previous debug
		if(i==1) continue;
		if(prom_info.fbinfo.frames[i].phys || prom_info.fbinfo.frames[i].virt || prom_info.fbinfo.frames[i].len) {
	*(volatile unsigned char *)(0xff110000)=0xDD; // Previous debug
			fb_info(info, "Mystery frame: #%d phys=0x%0X virt=0x%0X len=%d\n",
				i,
				prom_info.fbinfo.frames[i].phys,
				prom_info.fbinfo.frames[i].virt,
				prom_info.fbinfo.frames[i].len
			);
		}
	}

	info->var = nextfb_var;
	info->fix = nextfb_fix;
	info->fbops = &nextfb_ops;
	info->flags = FBINFO_DEFAULT;
	// info->pseudo_palette = info->par;
	// info->par = NULL;
	info->screen_base = (char *)nextfb_fix.smem_start;

	// if (fb_alloc_cmap(&info->cmap, 256, 0) < 0) {
	// 	framebuffer_release(info);
	// 	return -ENOMEM;
	// }

	// master_outb(3, DISPLAY_CONTROL_REG);

	if (register_framebuffer(info) < 0) {
	*(volatile unsigned char *)(0xff110000)=0xDE; // Previous debug
		printk(KERN_ERR "Unable to register NeXT frame buffer.\n");
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

// static struct platform_device nextfb_device = {
// 	.name	= "nextfb",
// };

// static int __init nextfb_init(void)
// {
// 	int ret = 0;

// 	if (fb_get_options("nextfb", NULL))
// 		return -ENODEV;

// 	ret = platform_driver_register(&nextfb_driver);

// 	if (!ret) {
// 		ret = platform_device_register(&nextfb_device);
// 		if (ret)
// 			platform_driver_unregister(&nextfb_driver);
// 	}
// 	return ret;
// }

// module_init(nextfb_init);
MODULE_LICENSE("GPL");
module_platform_driver(nextfb_driver);
// #define DRIVER_DESC "LCD controller driver for AU1200 processors"
// MODULE_DESCRIPTION(DRIVER_DESC);
