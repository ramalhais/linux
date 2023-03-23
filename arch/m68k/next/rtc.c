// arch/m68k/next/rtc.c
// Copyright (C) 1998 Zach Brown <zab@zabbo.net>
// Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
// Drive the NeXT clock chips.

#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
// #include <linux/delay.h>

#include <asm/irq.h>
#include <asm/nexthw.h>
#include <asm/nextints.h>

#include "rtc.h"

#define TIMER_HZ 1000000L

u_char rtc_read(u_char reg);
void rtc_write(u_char reg, u_char v);
// void next_poweroff(int vec, void *blah2, struct pt_regs *fp);

// the system control reg (?), through which we talk to the rtc's serial interface
volatile unsigned int *scr2 = (unsigned int *)NEXT_SCR2_BASE;

struct clockst {
	char *chipname;
	int secreg;
	int powerreg;
} rtcs[] = {
	{"MC68HC68T1", RTC_COUNTER0, RTC_INTERRUPTCTL},
	// https://www.nextcomputers.org/NeXTfiles/Docs/Hardware/Datasheets/MCCS1850%20(2).pdf
	{"MCS1850", RTC_COUNTER3, RTC_CTL}
};

#define N_C_OLD 0
#define N_C_NEW 1
int clocktype;

static u64 next_read_clk(struct clocksource *cs);

static struct clocksource next_clk = {
	.name   = "timer",
	.rating = 250,
	.read   = next_read_clk,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static u32 clk_total;

static irqreturn_t next_tick(int irq, void *dev_id)
{
	unsigned long flags;

	local_irq_save(flags);

	// if (!next_irq_pending(NEXT_IRQ_TIMER)) {
	// 	local_irq_restore(flags);
	// 	return IRQ_NONE;
	// }

	// write_timer_ticks(TIMER_HZ/HZ); // atempt to set the ticks back
	set_timer_csr_bits(TIM_RESTART); // retrigger timer
	clk_total += TIMER_HZ/HZ;
	legacy_timer_tick(1);

	// FIXME: how to mark IRQ as handled on the NeXT interrupt controller or RTC?

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

uint16_t next_nvram_checksum(struct nvram_settings *nv)
{
	uint16_t *nvp = (uint16_t *)nv;
	uint32_t sum = 0;

	for (int i = 0; i < (sizeof(*nv)-2)>>1; i++) {
		sum += nvp[i];
		#define ONEWORD 0x00010000
		if (sum >= ONEWORD) {	/* wrap carries into low bit */
			sum -= ONEWORD;
			sum++;
		}
	}

	return (sum & 0xffff);
}

void next_nvram_print(struct nvram_settings *nv)
{
	uint16_t checksum;

	print_hex_dump(KERN_DEBUG, "NeXT RTC NVRAM: ", DUMP_PREFIX_ADDRESS,
		16, 1, nv, sizeof(*nv), true);

	pr_info("flags[reset=0x%hx alt_cons=0x%hx allow_eject=0x%hx vol_r=0x%hx brightness=0x%hx hw_pwd=0x%hx vol_l=0x%hx spkren=0x%hx lowpass=0x%hx boot_any=0x%hx any_cmd=0x%hx]\n",
		(nv->flags&NV_RESET) >> 24,
		(nv->flags&NV_ALT_CONS) >> 24,
		(nv->flags&NV_ALLOW_EJECT) >> 24,
		(nv->flags&NV_VOL_R) >> 20,
		(nv->flags&NV_BRIGHTNESS) >> 12,
		(nv->flags&NV_HW_PWD) >> 8,
		(nv->flags&NV_VOL_L) >> 4,
		nv->flags&NV_SPKREN,
		nv->flags&NV_LOWPASS,
		nv->flags&NV_BOOT_ANY,
		nv->flags&NV_ANY_CMD
	);
	pr_info("eaddr=%pM\n", nv->eaddr);
	pr_info("simm_something=0x%x\n", nv->simm_something);
	pr_info("adobe=0x%hx 0x%hx\n", nv->adobe[0], nv->adobe[1]);
	pr_info("pot=0x%hx 0x%hx 0x%hx\n", nv->pot[0], nv->pot[1], nv->pot[2]);
	pr_info("moreflags[new_clock_chip=0x%hx auto_powerdown=0x%hx use_console_slot=0x%hx console_slot=0x%hx]\n",
		nv->moreflags&NV_NEW_CLOCK_CHIP,
		nv->moreflags&NV_AUTO_POWERON,
		nv->moreflags&NV_USE_CONSOLE_SLOT,
		nv->moreflags&NV_CONSOLE_SLOT
	);
	pr_info("bootcmdline=%s\n", nv->bootcmdline);
	pr_info("checksum=0x%x\n", nv->checksum);
	pr_info("checksum_inv=0x%x\n", (~nv->checksum)&0xffff);

	checksum = next_nvram_checksum(nv);
	pr_info("calculated_checksum=0x%x\n", checksum);
	pr_info("calculated_checksum_inv=0x%x\n", (~checksum)&0xffff);
}

void next_nvram_read(int offset, int size, uint8_t *data)
{
	for (int i = 0; i < size; i++)
		data[i] = rtc_read(RTC_RAM+offset+i);
}

void next_nvram_write(int offset, int size, uint8_t *data)
{
	for (int i = 0; i < size; i++)
		rtc_write(RTC_RAM+offset+i, data[i]);
}

void next_nvram_fix(void)
{
	struct nvram_settings nv;

	next_nvram_read(0, sizeof(nv), (char *)&nv);
	if (nv.flags&NV_ALT_CONS && !nv.pot[0] && !nv.pot[1] && !nv.pot[2] && !nv.bootcmdline[0])
		return;

	pr_info("Fixing NeXT RTC NVRAM to enable serial console, disable power-on tests and disable auto-boot\n");
	next_nvram_print(&nv);

	nv.flags |= NV_ALT_CONS;
	nv.pot[0] = nv.pot[1] = nv.pot[2] = 0;
	nv.bootcmdline[0] = 0;

	nv.checksum = ~next_nvram_checksum(&nv);
	next_nvram_write(0, sizeof(nv), (uint8_t *)&nv);

	next_nvram_read(0, sizeof(nv), (uint8_t *)&nv);
	next_nvram_print(&nv);
}

void next_sched_init(void)
{
	// Not the best place for this, but if printed on arch/m68k/next/config.c,
	// you could only see it afterwards with dmesg
	pr_info("PROM Machine Info: %s (mach_type=0x%x next_board_rev=0x%x dmachip=0x%x diskchip=0x%x)",
		next_machine_names[prom_info.mach_type],
		prom_info.mach_type,
		prom_info.next_board_rev,
		prom_info.dmachip,
		prom_info.diskchip
	);

	next_nvram_fix();

	/* could also get this from the prom i think */
	clocktype = (rtc_read(RTC_STATUS) & RTC_IS_NEW) ? N_C_NEW : N_C_OLD;
	pr_info("RTC: %s\n", rtcs[clocktype].chipname);

// #define NEXT_DEBUG(val) *(volatile unsigned long *)(0xff00f004)=val
// NEXT_DEBUG(0x20);

	if (request_irq(NEXT_IRQ_TIMER, next_tick, IRQF_TIMER, "Timer", next_tick)) {
// NEXT_DEBUG(0x21);
		pr_err("Failed to register NeXT timer interrupt\n");
	}
	// if (request_irq(IRQ_AUTO_6, next_tick, IRQF_TIMER|IRQF_SHARED, "NeXT timer tick", next_tick)) {
	// 	pr_err("Failed to register NeXT timer tick interrupt\n");
	// }

	// next_intmask_enable(NEXT_IRQ_TIMER-NEXT_IRQ_BASE);

	if (__timer_csr) {	// Reading CSR clears the interrupt
		set_timer_csr(0);
	}

	write_timer_ticks(TIMER_HZ/HZ);
	set_timer_csr_bits(TIM_ENABLE|TIM_RESTART);
	clocksource_register_hz(&next_clk, TIMER_HZ);
}

/* usec timer, way cool */

unsigned long next_gettimeoffset(void)
{
	/* oh, we have run out of ticks */
	if (next_irq_pending(NEXT_IRQ_TIMER))
		return TIMER_HZ/HZ;

	return ((TIMER_HZ/HZ)-read_timer_ticks());
}

static u64 next_read_clk(struct clocksource *cs)
{
	return clk_total + next_gettimeoffset();
}

int next_hwclk_old(int op, struct rtc_time *t)
{
	if (!op) {
		t->tm_sec	= bcd2bin(rtc_read(R_O_SEC));
		t->tm_min	= bcd2bin(rtc_read(R_O_MIN));
		t->tm_hour	= bcd2bin(rtc_read(R_O_HOUR));
		t->tm_wday	= bcd2bin(rtc_read(R_O_DAYOFWEEK));
		t->tm_mday	= bcd2bin(rtc_read(R_O_DAYOFMONTH));
		t->tm_mon	= bcd2bin(rtc_read(R_O_MONTH)) - 1;
		t->tm_year	= bcd2bin(rtc_read(R_O_YEAR));
		if (t->tm_year < 70)
			t->tm_year += 100;
	} else {
		rtc_write(R_O_SEC,		bin2bcd(t->tm_sec));
		rtc_write(R_O_MIN,		bin2bcd(t->tm_min));
		rtc_write(R_O_HOUR,		bin2bcd(t->tm_hour));
		rtc_write(R_O_DAYOFWEEK,	bin2bcd(t->tm_wday));
		rtc_write(R_O_DAYOFMONTH,	bin2bcd(t->tm_mday));
		rtc_write(R_O_MONTH,		bin2bcd(t->tm_mon + 1));

		if (t->tm_year < 100)
			rtc_write(R_O_YEAR, bin2bcd(t->tm_year));
		else
			rtc_write(R_O_YEAR, bin2bcd(t->tm_year - 100));
	}

	return 0;
}

int next_hwclk_new(int op, struct rtc_time *t)
{
	time64_t now;

	if (!op) {
		now = rtc_read(R_O_SEC)<<24 | rtc_read(R_O_MIN)<<16 | rtc_read(R_O_HOUR)<<8 | rtc_read(R_O_DAYOFWEEK);
		rtc_time64_to_tm(now, t);
	} else {
		now = rtc_tm_to_time64(t);
		rtc_write(R_O_SEC,		now>>24&0xff);
		rtc_write(R_O_MIN,		now>>16&0xff);
		rtc_write(R_O_HOUR,		now>>8&0xff);
		rtc_write(R_O_DAYOFWEEK,	now&0xff);
	}

	return 0;
}

int next_hwclk(int op, struct rtc_time *t)
{
	if (clocktype == N_C_OLD)
		return next_hwclk_old(op, t);
	else
		return next_hwclk_new(op, t);
}

void next_poweroff(void)
{
	int seconds;
	int reg = rtcs[clocktype].secreg;

	seconds = rtc_read(reg);
	while (seconds == rtc_read(reg))
		;

	// mdelay(850);

	rtc_write(rtcs[clocktype].powerreg,
		 rtc_read(rtcs[clocktype].powerreg)|(RTC_POFF));

	// pr_info("Powering down........");
	// local_irq_disable();
	// for (;;)
	// 	;
}

// i have no idea if this will be the required 1us or not
#define LONG_DELAY()                        \
	do {                                \
		int i;                      \
		for (i = 0x7ff; i > 0; --i) \
			do {} while (0);    \
	} while (0)

void rtc_write_reg(u_char reg, int towrite)
{
	int i, scrtmp;

	// tell it we're coming
	*scr2 = (*scr2 & ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK)) | SCR2_RTC_ENABLE;
	LONG_DELAY();

	// wether we're reading or writing to this reg
	if (towrite)
		reg |= RTC_TOWRITE;

	// strobe in the register
	for (i = 8; i; i--) {
		scrtmp = *scr2&~(SCR2_RTC_DATA|SCR2_RTC_CLOCK);
		// the bit is set
		if (reg&0x80)
			scrtmp |= SCR2_RTC_DATA;

		// strobe this bit in
		*scr2 = scrtmp;
		LONG_DELAY();

		*scr2 = scrtmp | SCR2_RTC_CLOCK;
		LONG_DELAY();

		*scr2 = scrtmp;
		LONG_DELAY();

		reg <<= 1;
	}

	LONG_DELAY();
}

u_char rtc_read(u_char reg)
{
	int i;
	u_int scrtmp;
	u_char ret = 0;

	// chose the register
	rtc_write_reg(reg, 0);

	// and grab the byte
	for (i = 8; i; i--) {
		ret <<= 1;

		scrtmp = *scr2 & ~(SCR2_RTC_DATA | SCR2_RTC_CLOCK);

		*scr2 = scrtmp | SCR2_RTC_CLOCK;
		LONG_DELAY();

		*scr2 = scrtmp;
		LONG_DELAY();

		if (*scr2 & SCR2_RTC_DATA)
			ret |= 1;
	}

	// ok, we're done...
	*scr2 &= ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK|SCR2_RTC_ENABLE);
	LONG_DELAY();

	return ret;
}

void rtc_write(u_char reg, u_char data)
{
	int i;
	u_int scrtmp;

	// chose a reg
	rtc_write_reg(reg, 1);

	// now write the data...
	for (i = 8; i; i--) {
		scrtmp = *scr2 & ~(SCR2_RTC_DATA | SCR2_RTC_CLOCK);
		if (data&0x80)
			scrtmp |= SCR2_RTC_DATA;

		data <<= 1;

		*scr2 = scrtmp;
		LONG_DELAY();

		*scr2 = scrtmp | SCR2_RTC_CLOCK;
		LONG_DELAY();

		*scr2 = scrtmp;
		LONG_DELAY();
	}

	// ok, we're done...
	*scr2 &= ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK|SCR2_RTC_ENABLE);
	LONG_DELAY();
}
