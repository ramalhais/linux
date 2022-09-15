/*
 *  linux/arch/m68k/next/kbd.c
 *
 *  Copyright (C) 1998 Zach Brown <zab@zabbo.net>
 *
 *  deal with the keyboard/mouse interface
 *
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
// #include <linux/types.h>
// #include <linux/kernel.h>
// #include <linux/kernel.h>
// #include <linux/mm.h>

#include <asm/nextints.h>
// #include <asm/nexthw.h>
// #include <asm/irq.h>

// #include <linux/kbd_ll.h>

// #include "nextmap.c"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pedro Ramalhais <ramalhais@gmail.com>");
MODULE_DESCRIPTION("Keyboard driver for NeXT Computer/Cube/Station");
MODULE_ALIAS("platform:next-keyboard");

/* shouldn't be global i'll wager :) */
struct mon {
	volatile __u32 csr,data,km_data,snd_data;
} *mon=(struct mon *)NEXT_MON_BASE;

/* bits in csr */
#define KM_INT		0x00800000
#define KM_HAVEDATA	0x00400000

/* high bits in km_data */

#define KD_ADDRMASK (0xf << 24)

#define KD_KADDR (0 << 24)
#define KD_MADDR (1 << 24)


/* bits in km_data for keyboard */

#define KD_KEYMASK	0x007f
#define KD_DIRECTION	0x0080
#define KD_CNTL		0x0100
#define KD_LSHIFT	0x0200
#define KD_RSHIFT	0x0400
#define KD_LCOMM	0x0800
#define KD_RCOMM	0x1000
#define KD_LALT		0x2000
#define KD_RALT		0x4000
#define KD_VALID	0x8000 /* only set for scancode keys ? */

#define KD_FLAGKEYS	0x7f00

/* bits in km_data for mouse 
typedef union {
	__u32	data;
	__u32	junk : 16,
		dy: 7, 
		rbut: 1,
		dx: 7,
		lbut:1;
} ; */

#define DEFAULT_KEYB_REP_DELAY  (HZ/4)
#define DEFAULT_KEYB_REP_RATE   (HZ/25)

#define NR_CTRL_KEYS 7
// #define CTRL_BASE_CODE 81
#define CTRL_BASE_CODE 100	// We may be missing some keys. Let's give it some room.

static unsigned int oldflagmap=0;

struct next_kbd {
	struct input_dev *input;
	// void __iomem *addr;
	// int irq;
	// https://web.archive.org/web/20220131154708if_/http://xahlee.info/kbd/i/NeXT_keyboard_kTcqz.jpg
	// https://web.archive.org/web/20220203021711if_/http://xahlee.info/kbd/i/NeXT_Computer_keyboard_66682.jpg
	unsigned short keycodes[CTRL_BASE_CODE+6+1];
};

struct next_kbd tpl_next_kbd = {
	.keycodes = {
		[3] = KEY_BACKSLASH,
		[4] = KEY_LEFTBRACE,
		[5] = KEY_RIGHTBRACE,
		[6] = KEY_I,
		[7] = KEY_O,
		[8] = KEY_P,
		[9] = KEY_LEFT,
		[11] = KEY_KP0,
		[13] = KEY_KPDOT,
		[13] = KEY_KPENTER,
		[16] = KEY_RIGHT,
		[17] = KEY_KP1,
		[18] = KEY_KP4,
		[19] = KEY_KP6,
		[20] = KEY_KP3,
		[21] = KEY_KPPLUS,
		[22] = KEY_UP,
		[23] = KEY_KP2,
		[24] = KEY_KP5,
		[27] = KEY_DELETE,
		[28] = KEY_EQUAL,
		[29] = KEY_MINUS,
		[30] = KEY_8,
		[31] = KEY_9,
		[32] = KEY_0,
		[33] = KEY_KP7,
		[34] = KEY_KP8,
		[35] = KEY_KP9,
		[36] = KEY_KPMINUS,
		[37] = KEY_KPASTERISK,
		[38] = KEY_GRAVE,
		[39] = KEY_EQUAL,
		[40] = KEY_KPSLASH,
		[42] = KEY_ENTER,
		[43] = KEY_APOSTROPHE,
		[44] = KEY_SEMICOLON,
		[45] = KEY_L,
		[46] = KEY_COMMA,
		[47] = KEY_DOT,
		[48] = KEY_SLASH,
		[49] = KEY_Z,
		[50] = KEY_X,
		[51] = KEY_C,
		[52] = KEY_V,
		[53] = KEY_B,
		[54] = KEY_M,
		[55] = KEY_N,
		[56] = KEY_SPACE,
		[57] = KEY_A,
		[58] = KEY_S,
		[59] = KEY_D,
		[60] = KEY_F,
		[61] = KEY_G,
		[62] = KEY_K,
		[63] = KEY_J,
		[64] = KEY_H,
		[65] = KEY_TAB,
		[66] = KEY_Q,
		[67] = KEY_W,
		[68] = KEY_E,
		[69] = KEY_R,
		[70] = KEY_U,
		[71] = KEY_Y,
		[72] = KEY_T,
		[73] = KEY_ESC,
		[74] = KEY_1,
		[75] = KEY_2,
		[76] = KEY_3,
		[77] = KEY_4,
		[78] = KEY_7,
		[79] = KEY_6,
		[80] = KEY_5,
		[CTRL_BASE_CODE]	= KEY_LEFTCTRL,
		[CTRL_BASE_CODE+1]	= KEY_LEFTSHIFT,
		[CTRL_BASE_CODE+2]	= KEY_RIGHTSHIFT,
		[CTRL_BASE_CODE+3]	= KEY_LEFTMETA,
		[CTRL_BASE_CODE+4]	= KEY_RIGHTMETA,
		[CTRL_BASE_CODE+5]	= KEY_LEFTALT,
		[CTRL_BASE_CODE+6]	= KEY_RIGHTALT
	}
};

// void next_kbd_int(int irq, void *dev_id, struct pt_regs *regs)
// {
// 	__u32 csr=mon->csr;
// 	__u32 data;

// 	unsigned int mask,index,changed,scan;
	
// 	/* ack the int */
// 	mon->csr=(csr & ~KM_INT);

// 	if(!(csr & KM_HAVEDATA)) return;

// 	data=mon->km_data;

// 	if((data & KD_ADDRMASK) == KD_KADDR) {
// 		/* keyboard reporting */


// 		/* the next only has a bitmap of which ctl keys
// 		are currently down, so we just triggers ups and downs
// 		to match how we last thought the world was. We do this
// 		first so real keys pressed after we get out of sync
// 		don't suffer. */

// 		changed=oldflagmap;
// 		oldflagmap=data & KD_FLAGKEYS;

// 		if((changed^=oldflagmap)) {
// 			/* use cool bitmap instructions */
// 			for(mask=KD_CNTL,index=0;index<7;mask<<=1,index++) {
	
// 				if(!(changed&mask)) continue;
// 				if(!(scan=flagscans[index])) continue;
// 				handle_scancode(scan | (data&mask ? 0: 0x80));

// 			}
// 		}
// 		if(data & KD_VALID) {  
// 			/* a 'real' key has changed */
// 			/* luckly the next and kernel use the same 'up/down' flag :) */
// 			handle_scancode(data&(KD_KEYMASK|KD_DIRECTION));
// 		}
				
// 	} 
// }

static irqreturn_t next_kbd_int(int irq, void *dev_id)
{
	unsigned long flags;
	struct next_kbd *kbd = dev_id;
	struct input_dev *input = kbd->input;
	__u32 csr;
	__u32 data;
	unsigned int mask,index,changed,scan;

	// *(volatile unsigned char *)(0xff110000)=0xF6; // Previous debug
	local_irq_save(flags);

	if (!next_irq_pending(NEXT_IRQ_KYBD_MOUSE)) {
	// *(volatile unsigned char *)(0xff110000)=0xF7; // Previous debug
		printk("IRQ3 not for NeXT keyboard/mouse\n");
		local_irq_restore(flags);
		return IRQ_NONE;
	}

	
	/* ack the int */
	// mon->csr=(csr & ~KM_INT);
	csr = mon->csr;
	// *(volatile unsigned char *)(0xff110000)=0xFB; // Previous debug

	if(!(csr & KM_HAVEDATA)) {
	// *(volatile unsigned char *)(0xff110000)=0xF8; // Previous debug
		printk("NeXT keyboard/mouse interrupt, but no data to handle\n");
		return IRQ_HANDLED;
	}

	data=mon->km_data;
	// *(volatile unsigned char *)(0xff110000)=0xFC; // Previous debug

	if((data & KD_ADDRMASK) == KD_KADDR) {
		/* keyboard reporting */


		/* the next only has a bitmap of which ctl keys
		are currently down, so we just triggers ups and downs
		to match how we last thought the world was. We do this
		first so real keys pressed after we get out of sync
		don't suffer. */

		changed=oldflagmap;
		oldflagmap=data & KD_FLAGKEYS;

		if((changed^=oldflagmap)) {
			/* use cool bitmap instructions */
			for(mask=KD_CNTL,index=0; index<NR_CTRL_KEYS; mask<<=1,index++) {	
				if (!(changed&mask))
					continue;
	*(volatile unsigned char *)(0xff110000)=0xF9; // Previous debug
				scan = CTRL_BASE_CODE+index;
				input_report_key(input, scan, !(data&KD_DIRECTION));
				input_sync(input);
	*(volatile unsigned char *)(0xff110000)=scan; // Previous debug
	*(volatile unsigned char *)(0xff110000)=data&KD_DIRECTION; // Previous debug
			}
		}
		if(data&KD_VALID && data&KD_KEYMASK) {
	*(volatile unsigned char *)(0xff110000)=0xFA; // Previous debug
			/* a 'real' key has changed */
			input_report_key(input, data&KD_KEYMASK, !(data&KD_DIRECTION));
			input_sync(input);
	*(volatile unsigned char *)(0xff110000)=data&KD_KEYMASK; // Previous debug
	*(volatile unsigned char *)(0xff110000)=data&KD_DIRECTION; // Previous debug
		}
	} 

	// *(volatile unsigned char *)(0xff110000)=0xFD; // Previous debug
	local_irq_restore(flags);

	return IRQ_HANDLED;
}

static int next_kbd_probe(struct platform_device *pdev) {
	struct input_dev *input;
	struct next_kbd *next_kbd;
	int ret;

	// *(volatile unsigned char *)(0xff110000)=0xF4; // Previous debug
	input = input_allocate_device();
	if (!input) {
		dev_err(&pdev->dev, "Unable to allocate input device for NeXT Keyboard\n");
		return -ENOMEM;
	}

	next_kbd = devm_kzalloc(&pdev->dev, sizeof(*next_kbd), GFP_KERNEL);
	if (!next_kbd) {
		return -ENOMEM;
	}

	next_kbd->input = input;

	input->name = pdev->name;
	input->phys = "next-kbd/input0";

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x000F;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input->keycode = next_kbd->keycodes;
	input->keycodesize = sizeof(next_kbd->keycodes[0]);
	input->keycodemax = ARRAY_SIZE(next_kbd->keycodes);

	for (int i = 0; i < ARRAY_SIZE(next_kbd->keycodes); i++) {
		next_kbd->keycodes[i] = tpl_next_kbd.keycodes[i];
		if (next_kbd->keycodes[i] != 0) {
			__set_bit(next_kbd->keycodes[i], input->keybit);
		}
	}
	__clear_bit(KEY_RESERVED, input->keybit);

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_REP, input->evbit);

	ret = input_register_device(input);
	if (ret) {
		dev_err(&pdev->dev, "unable to register input device\n");
		return ret;
	}

	platform_set_drvdata(pdev, input);

	if (request_irq(IRQ_AUTO_3, next_kbd_int, IRQF_SHARED, "NeXT keyboard/mouse", next_kbd)) {
		pr_err("Failed to register NeXT keyboard/mouse interrupt\n");
	}

	next_intmask_enable(NEXT_IRQ_KYBD_MOUSE-NEXT_IRQ_BASE);
	// *(volatile unsigned char *)(0xff110000)=0xF5; // Previous debug

	return 0;
}

static struct platform_driver next_kbd_driver = {
	.probe    = next_kbd_probe,
	.driver   = {
		.name = "next-kbd",
	},
};

// module_platform_driver(next_kbd_pd);
// module_platform_driver_probe(next_kbd_pd, next_kbd_probe);


static struct platform_device next_kbd_device = {
	.name	= "next-kbd",
};

static int next_kbd_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&next_kbd_driver);

	if (!ret) {
		ret = platform_device_register(&next_kbd_device);
		if (ret)
			platform_driver_unregister(&next_kbd_driver);
	}
	return ret;
}

module_init(next_kbd_init);
