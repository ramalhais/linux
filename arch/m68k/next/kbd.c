/*
 *  linux/arch/m68k/next/kbd.c
 *
 *  Copyright (C) 1998 Zach Brown <zab@zabbo.net>
 *
 *  deal with the keyboard/mouse interface
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/mm.h>

#include <asm/nextints.h>
#include <asm/nexthw.h>
#include <asm/irq.h>

#include <linux/kbd_ll.h>

#include "nextmap.c"

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

/* this has to map the scan codes assigned in
	nextmap.map 
keycode 100 = Control
keycode 101 = Shift
keycode 102 = Shift
keycode 103 = Alt
keycode 104 = Alt
	*/

unsigned int flagscans[7]={
	100,101,102,0,0,103,104
};	

static unsigned int oldflagmap=0;

void next_kbd_int(int irq, void *dev_id, struct pt_regs *regs)
{
	__u32 csr=mon->csr;
	__u32 data;

	unsigned int mask,index,changed,scan;
	
	/* ack the int */
	mon->csr=(csr & ~KM_INT);

	if(!(csr & KM_HAVEDATA)) return;

	data=mon->km_data;

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
			for(mask=KD_CNTL,index=0;index<7;mask<<=1,index++) {
	
				if(!(changed&mask)) continue;
				if(!(scan=flagscans[index])) continue;
				handle_scancode(scan | (data&mask ? 0: 0x80));

			}
		}
		if(data & KD_VALID) {  
			/* a 'real' key has changed */
			/* luckly the next and kernel use the same 'up/down' flag :) */
			handle_scancode(data&(KD_KEYMASK|KD_DIRECTION));
		}
				
	} 
}

int next_kbd_init(void) {

/*        plain_map, shift_map, altgr_map, 0,
        ctrl_map, shift_ctrl_map, 0, 0,
        alt_map, 0, 0, 0,
        ctrl_alt_map,   0 */

	memcpy(key_maps[0], nextplain_map, sizeof(plain_map));
	memcpy(key_maps[1], nextshift_map, sizeof(plain_map));
	memcpy(key_maps[2], nextaltgr_map, sizeof(plain_map));
	memcpy(key_maps[4], nextctrl_map, sizeof(plain_map));
	memcpy(key_maps[5], nextshift_ctrl_map, sizeof(plain_map));
	memcpy(key_maps[8], nextalt_map, sizeof(plain_map));
	memcpy(key_maps[12], nextctrl_alt_map, sizeof(plain_map));

	request_irq(NEXT_IRQ_KYBD_MOUSE, next_kbd_int, IRQ_FLG_LOCK, "keyboard/mouse", next_kbd_int);
	return 1;
}

