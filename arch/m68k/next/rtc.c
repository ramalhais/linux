/*
 *  linux/arch/m68k/next/rtc.c
 *
 *  Copyright (C) 1998 Zach Brown <zab@zabbo.net>
 *  Copyright (C) 2022 Pedro Ramalhais <ramalhais@gmail.com>
 *
 *  drive the NeXT clock chips.
 *
 */

#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>

#include <asm/irq.h>
#include <asm/nexthw.h>
#include <asm/nextints.h>

#include "rtc.h"

#define TIMER_HZ 1000000L

u_char rtc_read(u_char reg);
void rtc_write(u_char reg,u_char v);
void next_poweroff(int vec, void *blah2, struct pt_regs *fp);

/* the system control reg (?), through which we talk
	to the rtc's serial interface */
volatile unsigned int *scr2=(unsigned int *)NEXT_SCR2_BASE;

struct clockst {
	char *chipname;
	int secreg;
	int powerreg;
} rtcs[]={
	{"MC68HC68T1",RTC_COUNTER0,RTC_INTERRUPTCTL},
	// https://www.nextcomputers.org/NeXTfiles/Docs/Hardware/Datasheets/MCCS1850%20(2).pdf
	{"MCS1850",RTC_COUNTER3,RTC_CTL}
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

	// *(volatile unsigned long *)(0xff00f004)=0x93; // Previous debug
	if (!next_irq_pending(NEXT_IRQ_TIMER)) {
		// unsigned int intmask = next_get_intmask();
		// unsigned int intstat = next_get_intstat();
	// *(volatile unsigned long *)(0xff00f004)=0x94; // Previous debug
	// *(volatile unsigned long *)(0xff00f004)=intmask; // Previous debug
	// *(volatile unsigned long *)(0xff00f004)=0x95; // Previous debug
	// *(volatile unsigned long *)(0xff00f004)=intstat; // Previous debug
		// printk("Ignoring IRQ %d. Not for NeXT timer\n", irq);

		local_irq_restore(flags);
		return IRQ_NONE;
	}
	// *(volatile unsigned long *)(0xff00f004)=0x95; // Previous debug

	// write_timer_ticks(TIMER_HZ/HZ); // atempt to set the ticks back
	set_timer_csr_bits(TIM_RESTART); // retrigger timer
	clk_total += TIMER_HZ/HZ;
	legacy_timer_tick(1);
	// *(volatile unsigned long *)(0xff00f004)=0x96; // Previous debug

	// FIXME: how to mark IRQ as handled on the NeXT interrupt controller or RTC?

	local_irq_restore(flags);
	return IRQ_HANDLED;
}

// static irqreturn_t irq_catchall(int irq, void *dev_id)
// {
// 	unsigned long flags;

// 	local_irq_save(flags);

// 	printk("Interrupt %d ignored\n", irq);

// 	local_irq_restore(flags);

// 	return IRQ_HANDLED;
// }

void next_sched_init(void)
{
//	*(volatile unsigned long *)(0xff00f004)=0x97; // Previous debug

	// TEST
	// request_irq(IRQ_AUTO_7, irq_catchall, 0, "int7", NULL);
	// request_irq(IRQ_AUTO_6, irq_catchall, 0, "int6", NULL);
	// request_irq(IRQ_AUTO_5, irq_catchall, 0, "int5", NULL);
	// request_irq(IRQ_AUTO_3, irq_catchall, 0, "int3", NULL);

	// Not the best place for this, but if printed on arch/m68k/next/config.c,
	// you could only see it afterwards with dmesg
	printk("PROM Machine Info: %s (mach_type=0x%x next_board_rev=0x%x dmachip=0x%x diskchip=0x%x)",
		next_machine_names[prom_info.mach_type],
		prom_info.mach_type,
		prom_info.next_board_rev,
		prom_info.dmachip,
		prom_info.diskchip
	);

	/* could also get this from the prom i think */
	clocktype=(rtc_read(RTC_STATUS) & RTC_IS_NEW) ? N_C_NEW : N_C_OLD;
	printk("RTC: %s\n",rtcs[clocktype].chipname);

	*(volatile unsigned long *)(0xff00f004)=0x98; // Previous debug
	if (request_irq(IRQ_AUTO_6, next_tick, IRQF_TIMER|IRQF_SHARED, "NeXT timer tick", next_tick)) {
	*(volatile unsigned long *)(0xff00f004)=0x99; // Previous debug
		pr_err("Couldn't register timer interrupt\n");
	}

	*(volatile unsigned long *)(0xff00f004)=0x9A; // Previous debug
	next_intmask_enable(NEXT_IRQ_TIMER-NEXT_IRQ_BASE);
	*(volatile unsigned long *)(0xff00f004)=0x9B; // Previous debug

	if (__timer_csr) {	// Reading CSR clears the interrupt
		set_timer_csr(0);
	}

	// *(volatile unsigned long *)(0xff00f004)=0x9C; // Previous debug
	write_timer_ticks(TIMER_HZ/HZ);
	// *(volatile unsigned long *)(0xff00f004)=0x9D; // Previous debug
	set_timer_csr_bits(TIM_ENABLE|TIM_RESTART);

	// *(volatile unsigned long *)(0xff00f004)=0x9E; // Previous debug
	clocksource_register_hz(&next_clk, TIMER_HZ);
	*(volatile unsigned long *)(0xff00f004)=0x9F; // Previous debug
}

/* usec timer, way cool */

unsigned long next_gettimeoffset(void)
{
	/* oh, we have run out of ticks */
	if(next_irq_pending(NEXT_IRQ_TIMER)) {
		return TIMER_HZ/HZ;
	}

	return ((TIMER_HZ/HZ)-read_timer_ticks());
}

static u64 next_read_clk(struct clocksource *cs)
{
	return clk_total + next_gettimeoffset();
}

void next_gettod(int *yearp, int *monp, int *dayp, int *hourp, int *minp, int *secp)
{

	unsigned int tmp;	

	if (clocktype == N_C_OLD ) {
		*secp=from_bcd(rtc_read(R_O_SEC));
		printk("time: s: %d ",*secp);	

		*minp=from_bcd(rtc_read(R_O_MIN));
		printk("m: %d ",*minp);	

		tmp=from_bcd(rtc_read(R_O_HOUR));
		printk("h: [%x] ",tmp);
		if(tmp&0x80) {
			/* we're in twelve hour mode */
			*hourp=(tmp&0xf) + 12*((tmp&0x2)>>5); /* bit 5 set is PM */
		} else {
			*hourp=from_bcd(tmp&0x3f);
		}
		printk("%d ",*hourp);	

		*dayp=from_bcd(rtc_read(R_O_DAYOFMONTH));
		printk("d: %d ",*dayp);	

		*monp=from_bcd(rtc_read(R_O_MONTH));
		printk("m: %d ",*monp);	

		tmp=from_bcd(rtc_read(R_O_YEAR));
		printk("y: [%x] ",tmp);
		if(tmp<70) *yearp=tmp+2000;
		else *yearp=tmp+1900;
		printk("y: %d\n",*yearp);	
	}
}

void next_poweroff(int vec, void *blah2, struct pt_regs *fp) 
{
	int seconds,reg=rtcs[clocktype].secreg;

	seconds = rtc_read(reg);
	while(seconds == rtc_read(reg));

	rtc_write(rtcs[clocktype].powerreg,
		 rtc_read(rtcs[clocktype].powerreg)|(RTC_POFF));

	printk("powering down..");
	for(;;); /* called from do_ints so ints should be masked, kinda */
}

/* i have no idea if this will be the required 1us or not */
#define LONG_DELAY()                            \
    do {                                        \
        int i;                                  \
        for( i = 0x7ff; i > 0; --i )       \
		do {} while (0);		\
    } while(0)

void rtc_write_reg(u_char reg,int towrite) 
{
	int i,scrtmp;

	/* tell it we're coming */
	*scr2=(*scr2 & ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK)) | SCR2_RTC_ENABLE;
	LONG_DELAY();

	/* wether we're reading or writing to this reg */
	if(towrite) reg|=RTC_TOWRITE;

	/* strobe in the register */
	for(i=8;i;i--) {
		scrtmp=*scr2&~(SCR2_RTC_DATA|SCR2_RTC_CLOCK);
		/* the bit is set */
		if(reg&0x80) scrtmp|=SCR2_RTC_DATA;

		/* strobe this bit in */
                *scr2=scrtmp;
		LONG_DELAY();

                *scr2=scrtmp | SCR2_RTC_CLOCK;
                LONG_DELAY();

                *scr2=scrtmp;
		LONG_DELAY();

                reg<<=1;
        }
	LONG_DELAY();
}

u_char rtc_read(u_char reg)
{
	int i;
	u_int scrtmp;
	u_char ret=0;

	/* chose the register */
	rtc_write_reg(reg,0);

	/* and grab the byte */
        for(i=8;i;i--) {
                ret<<=1;
        
                scrtmp=*scr2 & ~(SCR2_RTC_DATA | SCR2_RTC_CLOCK);

                *scr2=scrtmp | SCR2_RTC_CLOCK;
                LONG_DELAY();

                *scr2=scrtmp;
                LONG_DELAY();

                if (*scr2 & SCR2_RTC_DATA) ret|=1;
        }

	/* ok, we're done..  */
        *scr2 &= ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK|SCR2_RTC_ENABLE);
        LONG_DELAY();

        return ret;
}

void rtc_write(u_char reg, u_char data)
{
        int i;
        u_int scrtmp;
    
	/* chose a reg */
	rtc_write_reg(reg,1);

	/* now write the data.. */
        for(i=8;i;i--) {
                scrtmp=*scr2 & ~(SCR2_RTC_DATA | SCR2_RTC_CLOCK);
                if(data&0x80) scrtmp|=SCR2_RTC_DATA;
		data<<=1;

                *scr2=scrtmp;
                LONG_DELAY();

                *scr2=scrtmp | SCR2_RTC_CLOCK;
                LONG_DELAY();

                *scr2=scrtmp;
                LONG_DELAY();
        }

	/* ok, we're done.. */
        *scr2 &= ~(SCR2_RTC_DATA|SCR2_RTC_CLOCK|SCR2_RTC_ENABLE);
        LONG_DELAY();
}
