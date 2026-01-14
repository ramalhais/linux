// SPDX-License-Identifier: GPL-2.0

// arch/m68k/next/kbd.c
// Copyright (C) 1998 Zach Brown <zab@zabbo.net>
// Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
// Deal with the keyboard/mouse interface
// ADB support for Turbo systems added 2025

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <asm/nextints.h>
#include <asm/nexthw.h>

MODULE_DESCRIPTION("Keyboard/Mouse driver for NeXT Computer/cube/station/turbo");
MODULE_ALIAS("platform:next-keyboard");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pedro Ramalhais <ramalhais@gmail.com>");

/* Flag to track whether we're on a Turbo system (uses ADB) */
static bool is_turbo;

/* ADB registers (for Turbo systems) */
static void __iomem *adb_regs;

/* shouldn't be global i'll wager :) */
struct mon {
	u32 csr;
	u32 data;
	u32 km_data;
	u32 snd_data;
} *mon;// = (struct mon *)NEXT_MON;

/* bits in csr */
#define SNDOUT_DMA_ENABLE	0x80000000	// rw
#define SNDOUT_DMA_REQUEST	0x40000000	// r
#define SNDOUT_DMA_UNDERRUN	0x20000000	// rw
#define SNDIN_DMA_ENABLE	0x08000000	// rw
#define SNDIN_DMA_REQUEST	0x04000000	// r
#define SNDIN_DMA_OVERRUN	0x02000000	// rw

#define KM_INT		0x00800000	// r
#define KM_HAVEDATA	0x00400000	// r
#define KM_OVERRUN	0x00200000	// rw
#define NMI_RECEIVED	0x00100000	// rw
#define KMS_INT         0x00080000	// r
#define KMS_RECEIVED	0x00040000	// r
#define KMS_OVERRUN	0x00020000	// rw

#define TX_DMA_PENDING	0x00008000	// r
#define TX_DMA		0x00004000	// r
#define TX_CPU_PENDING	0x00002000	// r
#define TX_CPU		0x00001000	// r
#define TX_RX_PEND	0x00000800	// r
#define TX_RX		0x00000400	// r
#define KMS_ENABLE	0x00000200	// rw
#define TX_LOOP		0x00000100	// rw

/* high bits in km_data */

#define KD_ADDRMASK (0xf << 24)

#define KD_KADDR (0 << 24)
#define KD_MADDR (1 << 24)


/* bits in km_data for keyboard */

#define KD_DIRECTION	0x0080
#define KD_CNTL		0x0100
#define KD_LSHIFT	0x0200
#define KD_RSHIFT	0x0400
#define KD_LCOMM	0x0800
#define KD_RCOMM	0x1000
#define KD_LALT		0x2000
#define KD_RALT		0x4000
#define KD_VALID	0x8000 /* only set for scancode keys ? */
#define KD_KEYMASK	0x007f // normal keys
#define KD_FLAGKEYS	0x7f00 // modifiers

#define DEFAULT_KEYB_REP_DELAY  (HZ/4)
#define DEFAULT_KEYB_REP_RATE   (HZ/25)

#define NR_CTRL_KEYS 7
#define CTRL_BASE_CODE 81

#define NEXT_MOUSE_LEFT_MASK	0x0001
#define NEXT_MOUSE_RIGHT_MASK	0x0100
#define NEXT_MOUSE_DX_MASK	0x00FE
#define NEXT_MOUSE_DY_MASK	0xFE00

unsigned int oldflagmap;

struct next_kbd {
	struct input_dev *input;
	struct input_dev *mouse;
	unsigned short keycodes[CTRL_BASE_CODE+6+1];
};

// Keyboard pics
// https://web.archive.org/web/20220131154708if_/http://xahlee.info/kbd/i/NeXT_keyboard_kTcqz.jpg
// https://web.archive.org/web/20220203021711if_/http://xahlee.info/kbd/i/NeXT_Computer_keyboard_66682.jpg
struct next_kbd tpl_next_kbd = {
	.keycodes = {
		[1] = KEY_PAGEDOWN,
		[2] = KEY_END,
		[3] = KEY_BACKSLASH,
		[4] = KEY_RIGHTBRACE,
		[5] = KEY_LEFTBRACE,
		[6] = KEY_I,
		[7] = KEY_O,
		[8] = KEY_P,
		[9] = KEY_LEFT,
		[11] = KEY_KP0,
		[12] = KEY_KPDOT,
		[13] = KEY_KPENTER,
		[15] = KEY_DOWN,
		[16] = KEY_RIGHT,
		[17] = KEY_KP1,
		[18] = KEY_KP4,
		[19] = KEY_KP6,
		[20] = KEY_KP3,
		[21] = KEY_KPPLUS,
		[22] = KEY_UP,
		[23] = KEY_KP2,
		[24] = KEY_KP5,
		[25] = KEY_PAGEUP,
		[26] = KEY_HOME,
		[27] = KEY_BACKSPACE,
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
		[38] = KEY_NUMLOCK,
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

/*
 * Apple Desktop Bus (ADB) keycode to Linux keycode mapping table.
 * Used on NeXTstation Turbo systems which use ADB instead of KMS.
 * Keycodes derived from Previous emulator's adb.c
 */
static const unsigned short adb_keycodes[128] = {
	[0x00] = KEY_A,
	[0x01] = KEY_S,
	[0x02] = KEY_D,
	[0x03] = KEY_F,
	[0x04] = KEY_H,
	[0x05] = KEY_G,
	[0x06] = KEY_Z,
	[0x07] = KEY_X,
	[0x08] = KEY_C,
	[0x09] = KEY_V,
	[0x0a] = KEY_102ND,		/* APPLEKEY_LESS - non-US backslash */
	[0x0b] = KEY_B,
	[0x0c] = KEY_Q,
	[0x0d] = KEY_W,
	[0x0e] = KEY_E,
	[0x0f] = KEY_R,
	[0x10] = KEY_Y,
	[0x11] = KEY_T,
	[0x12] = KEY_1,
	[0x13] = KEY_2,
	[0x14] = KEY_3,
	[0x15] = KEY_4,
	[0x16] = KEY_6,
	[0x17] = KEY_5,
	[0x18] = KEY_EQUAL,
	[0x19] = KEY_9,
	[0x1a] = KEY_7,
	[0x1b] = KEY_MINUS,
	[0x1c] = KEY_8,
	[0x1d] = KEY_0,
	[0x1e] = KEY_RIGHTBRACE,
	[0x1f] = KEY_O,
	[0x20] = KEY_U,
	[0x21] = KEY_LEFTBRACE,
	[0x22] = KEY_I,
	[0x23] = KEY_P,
	[0x24] = KEY_ENTER,
	[0x25] = KEY_L,
	[0x26] = KEY_J,
	[0x27] = KEY_APOSTROPHE,
	[0x28] = KEY_K,
	[0x29] = KEY_SEMICOLON,
	[0x2a] = KEY_BACKSLASH,
	[0x2b] = KEY_COMMA,
	[0x2c] = KEY_SLASH,
	[0x2d] = KEY_N,
	[0x2e] = KEY_M,
	[0x2f] = KEY_DOT,
	[0x30] = KEY_TAB,
	[0x31] = KEY_SPACE,
	[0x32] = KEY_GRAVE,
	[0x33] = KEY_BACKSPACE,
	/* 0x34 unused */
	[0x35] = KEY_ESC,
	[0x36] = KEY_LEFTCTRL,
	[0x37] = KEY_LEFTMETA,		/* Apple/Command key */
	[0x38] = KEY_LEFTSHIFT,
	[0x39] = KEY_CAPSLOCK,
	[0x3a] = KEY_LEFTALT,		/* Option key */
	[0x3b] = KEY_LEFT,
	[0x3c] = KEY_RIGHT,
	[0x3d] = KEY_DOWN,
	[0x3e] = KEY_UP,
	/* 0x3f - 0x40 unused */
	[0x41] = KEY_KPDOT,
	/* 0x42 unused */
	[0x43] = KEY_KPASTERISK,
	/* 0x44 unused */
	[0x45] = KEY_KPPLUS,
	/* 0x46 - 0x4a unused */
	[0x4b] = KEY_KPSLASH,
	[0x4c] = KEY_KPENTER,
	/* 0x4d unused */
	[0x4e] = KEY_KPMINUS,
	/* 0x4f - 0x50 unused */
	[0x51] = KEY_KPEQUAL,
	[0x52] = KEY_KP0,
	[0x53] = KEY_KP1,
	[0x54] = KEY_KP2,
	[0x55] = KEY_KP3,
	[0x56] = KEY_KP4,
	[0x57] = KEY_KP5,
	[0x58] = KEY_KP6,
	[0x59] = KEY_KP7,
	/* 0x5a unused */
	[0x5b] = KEY_KP8,
	[0x5c] = KEY_KP9,
	/* 0x5d - 0x71 unused */
	[0x72] = KEY_INSERT,		/* Help key */
	[0x73] = KEY_VOLUMEDOWN,	/* Volume Up (mapped to F5 on NeXT) */
	[0x74] = KEY_BRIGHTNESSUP,
	/* 0x75 - 0x76 unused */
	[0x77] = KEY_VOLUMEUP,		/* Volume Down (mapped to F6 on NeXT) */
	/* 0x78 unused */
	[0x79] = KEY_BRIGHTNESSDOWN,
	/* 0x7a unused */
	[0x7b] = KEY_RIGHTSHIFT,
	[0x7c] = KEY_RIGHTALT,		/* Right Option */
	/* 0x7d - 0x7e unused */
	[0x7f] = KEY_POWER,
};

/*
 * ADB helper functions for Turbo systems
 */

/* Send an ADB command and wait for completion */
static bool adb_send_command(u8 addr, u8 cmd, u8 reg)
{
	u32 command = addr | cmd | reg;
	u32 status;
	int timeout = 1000;

	/* Write command */
	writel(command, adb_regs + ADB_CMD);

	/* Trigger command execution */
	writel(ADB_CTRL_XMIT_CMD, adb_regs + ADB_CTRL);

	/* Wait for completion (data pending or timeout) */
	while (timeout-- > 0) {
		status = readl(adb_regs + ADB_STATUS);
		if (status & (ADB_STAT_DATAPEND | ADB_STAT_TIMEOUT))
			break;
		udelay(1);
	}

	/* Clear interrupt status */
	writel(ADB_INT_ACCESS, adb_regs + ADB_INTSTATUS);

	return (status & ADB_STAT_DATAPEND) != 0;
}

/* Read keyboard data via ADB Talk command */
static bool adb_read_keyboard(u8 *key1, u8 *key2)
{
	u32 data0;

	/* Send Talk command to keyboard (address 2, register 0) */
	if (!adb_send_command(ADB_ADDR_KBD, ADB_CMD_TALK, 0))
		return false;

	/* Read data */
	data0 = readl(adb_regs + ADB_DATA0);
	*key1 = (data0 >> 24) & 0xFF;
	*key2 = (data0 >> 16) & 0xFF;

	return true;
}

/* Read mouse data via ADB Talk command */
static bool adb_read_mouse(s8 *dx, s8 *dy, bool *left, bool *right)
{
	u32 data0;
	u8 byte0, byte1;

	/* Send Talk command to mouse (address 3, register 0) */
	if (!adb_send_command(ADB_ADDR_MOUSE, ADB_CMD_TALK, 0))
		return false;

	/* Read data */
	data0 = readl(adb_regs + ADB_DATA0);
	byte0 = (data0 >> 24) & 0xFF;
	byte1 = (data0 >> 16) & 0xFF;

	/*
	 * ADB mouse data format (from emulator):
	 * byte0: bit 7 = left button (0=pressed), bits 0-6 = Y delta (signed 7-bit)
	 * byte1: bit 7 = right button (0=pressed), bits 0-6 = X delta (signed 7-bit)
	 */
	*left = !(byte0 & 0x80);
	*right = !(byte1 & 0x80);

	/* Convert 7-bit signed to 8-bit signed */
	*dy = (byte0 & 0x40) ? (byte0 | 0x80) : (byte0 & 0x3F);
	*dx = (byte1 & 0x40) ? (byte1 | 0x80) : (byte1 & 0x3F);

	return true;
}

/* Process an ADB keyboard event */
static void adb_process_key(struct input_dev *input, u8 keycode)
{
	unsigned short linux_key;
	bool is_release;

	if (keycode == 0xFF)
		return;  /* No key */

	/* Bit 7 indicates key release */
	is_release = (keycode & 0x80) != 0;
	keycode &= 0x7F;

	if (keycode >= ARRAY_SIZE(adb_keycodes))
		return;

	linux_key = adb_keycodes[keycode];
	if (linux_key)
		input_report_key(input, linux_key, !is_release);
}

/* ADB interrupt handler for Turbo systems */
static irqreturn_t next_adb_int(int irq, void *dev_id)
{
	struct next_kbd *kbd = dev_id;
	struct input_dev *input = kbd->input;
	struct input_dev *mouse = kbd->mouse;
	u32 status, intstatus;
	u8 key1, key2;
	s8 dx, dy;
	bool left, right;
	unsigned long flags;

	local_irq_save(flags);

	/* Read and clear interrupt status */
	intstatus = readl(adb_regs + ADB_INTSTATUS);
	writel(intstatus, adb_regs + ADB_INTSTATUS);

	status = readl(adb_regs + ADB_STATUS);

	/* Check if any device has pending data */
	if (!(status & ADB_STAT_REQUEST)) {
		local_irq_restore(flags);
		return IRQ_HANDLED;
	}

	/* Try to read keyboard data */
	if (adb_read_keyboard(&key1, &key2)) {
		adb_process_key(input, key1);
		adb_process_key(input, key2);
		input_sync(input);
	}

	/* Try to read mouse data */
	if (adb_read_mouse(&dx, &dy, &left, &right)) {
		if (dx)
			input_report_rel(mouse, REL_X, dx);
		if (dy)
			input_report_rel(mouse, REL_Y, dy);
		input_report_key(mouse, BTN_LEFT, left);
		input_report_key(mouse, BTN_RIGHT, right);
		input_sync(mouse);
	}

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

/*
 * KMS interrupt handler for non-Turbo systems (original implementation)
 */
static irqreturn_t next_kbd_int(int irq, void *dev_id)
{
	struct next_kbd *kbd = dev_id;
	struct input_dev *input = kbd->input;
	struct input_dev *mouse = kbd->mouse;
	u32 csr, csr_new;
	u32 data;
	unsigned long flags;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_KYBD_MOUSE)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }


	// ack the int
	// According to Previous, it's readonly. Should just need to read the data to clear the interrupt.
	csr = mon->csr;
	csr_new = csr&~(KM_INT|KMS_INT);
	if (csr_new&(KM_OVERRUN|NMI_RECEIVED|KMS_OVERRUN)) {
		csr_new &= ~(KM_OVERRUN|NMI_RECEIVED|KMS_OVERRUN);
	}
	mon->csr = csr_new;

	data = mon->km_data;

	// pr_err("NeXT Keyboard and Mouse interrupt, csr=0x%08x csr_new=0x%08x csr_new=0x%08x data=0x%08x\n", csr, csr_new, mon->csr, data);
	// 400e0200(sound enable, KMS int+recv+overrun, KMS enable) continuous:40ae0200 (sound enable, KM int+overrun, KMS int+recv+overrun,KMS enable)
	// csr 40ce0200 csr_new 40440200 real csr: 400e0200
	// csr 40ae0200 csr_new 40040200 real csr: 40ae0200
	if (!(csr&KM_HAVEDATA)) {
		goto bail;
		// pr_err("NeXT Keyboard and Mouse interrupt, no KM_HAVEDATA. csr=0x%08x csr_new=0x%08x csr_new=0x%08x data=0x%08x\n", csr, csr_new, mon->csr, data);
	}
	// if (!(csr&KMS_RECEIVED)) {
	// 	pr_err("NeXT Keyboard and Mouse interrupt, no KMS_RECEIVED. Ignoring. csr=0x%08x csr_new=0x%08x csr_new=0x%08x data=0x%08x\n", csr, csr_new, mon->csr, data);
	// 	goto bail;
	// }


	if ((data & KD_ADDRMASK) == KD_KADDR) {
		unsigned int changed;
		// keyboard reporting

		// the next only has a bitmap of which ctl keys
		// are currently down, so we just triggers ups and downs
		// to match how we last thought the world was. We do this
		// first so real keys pressed after we get out of sync
		// don't suffer.

		changed = oldflagmap;
		oldflagmap = data&KD_FLAGKEYS;

		changed ^= oldflagmap;
		if ((changed)) {
			for (int index = 0; index < NR_CTRL_KEYS; index++) {
				unsigned int scan, is_pressed;

				if (!(changed&(KD_CNTL<<index)))
					continue;

				scan = CTRL_BASE_CODE+index;
				is_pressed = !(data&KD_DIRECTION); // FIXME: could try sending data&KD_FLAGKEYS&mask instead
				input_report_key(input, kbd->keycodes[scan], is_pressed);
			}
		}

		if (data&KD_VALID && data&KD_KEYMASK) {
			unsigned int scan, is_pressed;

			/* a 'real' key has changed */
			scan = data&KD_KEYMASK;
			is_pressed = !(data&KD_DIRECTION);
			input_report_key(input, kbd->keycodes[scan], is_pressed);
		}

		input_sync(input);
	} else if ((data & KD_ADDRMASK) == KD_MADDR) {
		// Mouse
		if (data&NEXT_MOUSE_DX_MASK)
			input_report_rel(mouse, REL_X, (data&NEXT_MOUSE_DX_MASK) > 0x0f ? 1 : -1);
		if ((data&NEXT_MOUSE_DY_MASK)>>8)
			input_report_rel(mouse, REL_Y, ((data&NEXT_MOUSE_DY_MASK)>>8) > 0x0f ? 1 : -1);
		input_report_key(mouse, BTN_LEFT, !(data&NEXT_MOUSE_LEFT_MASK));
		input_report_key(mouse, BTN_RIGHT, !(data&NEXT_MOUSE_RIGHT_MASK));
		input_sync(mouse);
	}

bail:
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static int next_kbd_probe(struct platform_device *pdev)
{
	struct input_dev *input;
	struct input_dev *mouse_dev;
	struct next_kbd *next_kbd;
	int ret;
	int i;

	/* Detect machine type */
	is_turbo = NEXT_IS_TURBO;

	dev_info(&pdev->dev, "NeXT keyboard/mouse driver, %s mode\n",
		 is_turbo ? "ADB (Turbo)" : "KMS");

	input = input_allocate_device();
	if (!input) {
		dev_err(&pdev->dev, "Failed to allocate input device for NeXT Keyboard\n");
		return -ENOMEM;
	}

	next_kbd = devm_kzalloc(&pdev->dev, sizeof(*next_kbd), GFP_KERNEL);
	if (!next_kbd) {
		input_free_device(input);
		return -ENOMEM;
	}

	next_kbd->input = input;

	input->name = is_turbo ? "NeXT ADB Keyboard" : "NeXT Keyboard";
	input->phys = "next-kbd/keyboard";

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x000F;
	input->id.product = is_turbo ? 0x0003 : 0x0001;
	input->id.version = 0x0100;

	if (is_turbo) {
		/*
		 * For Turbo systems with ADB, set up keybits from the
		 * ADB keycode table
		 */
		for (i = 0; i < ARRAY_SIZE(adb_keycodes); i++) {
			if (adb_keycodes[i] != 0)
				__set_bit(adb_keycodes[i], input->keybit);
		}
	} else {
		/*
		 * For non-Turbo systems with KMS, use the original
		 * keycode table
		 */
		input->keycode = next_kbd->keycodes;
		input->keycodesize = sizeof(next_kbd->keycodes[0]);
		input->keycodemax = ARRAY_SIZE(next_kbd->keycodes);

		for (i = 0; i < ARRAY_SIZE(next_kbd->keycodes); i++) {
			next_kbd->keycodes[i] = tpl_next_kbd.keycodes[i];
			if (next_kbd->keycodes[i] != 0)
				__set_bit(next_kbd->keycodes[i], input->keybit);
		}
	}
	__clear_bit(KEY_RESERVED, input->keybit);

	__set_bit(EV_KEY, input->evbit);
	__set_bit(EV_REP, input->evbit);

	ret = input_register_device(input);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register input device for NeXT Keyboard\n");
		input_free_device(input);
		return ret;
	}

	platform_set_drvdata(pdev, next_kbd);

	mouse_dev = input_allocate_device();
	if (!mouse_dev) {
		dev_err(&pdev->dev, "Failed to allocate input device for NeXT Mouse\n");
		input_unregister_device(input);
		return -ENOMEM;
	}

	next_kbd->mouse = mouse_dev;

	mouse_dev->name = is_turbo ? "NeXT ADB Mouse" : "NeXT Mouse";
	mouse_dev->phys = "next-kbd/mouse";
	mouse_dev->id.bustype = BUS_HOST;
	mouse_dev->id.vendor = 0x000F;
	mouse_dev->id.product = is_turbo ? 0x0004 : 0x0002;
	mouse_dev->id.version = 0x0100;

	mouse_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	mouse_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	mouse_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) |
		BIT_MASK(BTN_MIDDLE) | BIT_MASK(BTN_RIGHT);

	ret = input_register_device(mouse_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register input device for NeXT Mouse\n");
		input_free_device(mouse_dev);
		input_unregister_device(input);
		return ret;
	}

	if (is_turbo) {
		/*
		 * Turbo systems use ADB via the TMC (Turbo Memory Controller).
		 * The ADB controller is at a different address and uses a
		 * different interrupt (INT_DISK is reused for ADB on Turbo).
		 */
		adb_regs = ioremap(NEXT_ADB_BASE, 0x90);
		if (!adb_regs) {
			dev_err(&pdev->dev, "Failed to map ADB registers\n");
			input_unregister_device(mouse_dev);
			input_unregister_device(input);
			return -ENOMEM;
		}

		/* Reset ADB and enable interrupt mask */
		writel(ADB_CTRL_RESET_ADB, adb_regs + ADB_CTRL);
		udelay(100);
		writel(ADB_INT_ACCESS, adb_regs + ADB_INTMASK);

		/*
		 * Note: On Turbo, ADB uses INT_DISK (IRQ 13).
		 * For now, we use the same keyboard/mouse IRQ and rely on
		 * polling in the interrupt handler.
		 */
		if (request_irq(NEXT_IRQ_KYBD_MOUSE, next_adb_int, 0,
				"NeXT ADB Keyboard and Mouse", next_kbd)) {
			dev_err(&pdev->dev, "Failed to register ADB interrupt\n");
			iounmap(adb_regs);
			input_unregister_device(mouse_dev);
			input_unregister_device(input);
			return -ENOMEM;
		}

		dev_info(&pdev->dev, "ADB initialized at 0x%08x\n", NEXT_ADB_BASE);
	} else {
		/*
		 * Non-Turbo systems use KMS (Keyboard/Mouse/Sound) interface.
		 */
		mon = ioremap(NEXT_MON, sizeof(struct mon));
		if (!mon) {
			dev_err(&pdev->dev, "Failed to map KMS registers\n");
			input_unregister_device(mouse_dev);
			input_unregister_device(input);
			return -ENOMEM;
		}

		if (request_irq(NEXT_IRQ_KYBD_MOUSE, next_kbd_int, 0,
				"NeXT Keyboard and Mouse", next_kbd)) {
			dev_err(&pdev->dev, "Failed to register KMS interrupt\n");
			iounmap(mon);
			input_unregister_device(mouse_dev);
			input_unregister_device(input);
			return -ENOMEM;
		}

		/* Enable KMS */
		mon->csr |= KMS_ENABLE;

		dev_info(&pdev->dev, "KMS initialized at 0x%08lx\n", NEXT_MON);
	}

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
