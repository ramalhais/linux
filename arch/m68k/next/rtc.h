// arch/m68k/next/rtc.h
// Deal with the RTC and timer chip
// Copyright (C) 1998 Zach Brown <zab@zabbo.net>

#include <asm/nexthw.h>

// NeXT clock registers
// there are two models, the 'old' MC68HC68T1 and 'new' MCS1850.
// I don't know if mot publishes a doc on the 1850 anymore :(

// bit to power down in control reg
#define RTC_POFF	0x40
// RTC_STATUS flags
#define RTC_IS_NEW	0x80

// tell the rtc to write this reg.
#define RTC_TOWRITE	0x80


// scr2 regs to talk to the rtc
#define SCR2_RTC_DATA	0x00000400
#define SCR2_RTC_CLOCK	0x00000200

// new rtc registers, anyone have docs? - Z
#define SCR2_RTC_ENABLE	0x00000100

// RTC register addresses
#define RTC_RAM		0x00

// 32bit second counter
#define RTC_COUNTER0	0x20
#define RTC_COUNTER1	0x21
#define RTC_COUNTER2	0x22
#define RTC_COUNTER3	0x23

#define RTC_STATUS	0x30
#define RTC_CTL		0x31
#define RTC_INTERRUPTCTL 0x32
// old rtc specific regs

// it stores 8bit regs in bcd...

#define from_bcd(x) (((x>>4)*10)+(x&0xf))

#define R_O_SEC		0x20
#define R_O_MIN		0x21
#define R_O_HOUR	0x22
#define R_O_DAYOFWEEK	0x23 // (sunday = 1)
#define R_O_DAYOFMONTH	0x24
#define R_O_MONTH	0x25
#define R_O_YEAR	0x26 // year (0 - 99)

// timer...

/* control flags */
#define TIM_ENABLE	0x80
#define TIM_RESTART	0x40

// register offsets
#define TIMER_R_MSB (0)
#define TIMER_R_LSB (1)
#define TIMER_R_CSR (4)

// silly, silly

#define __timer_msb *(volatile unsigned char *)(NEXT_TIMER_BASE+TIMER_R_MSB)
#define __timer_lsb *(volatile unsigned char *)(NEXT_TIMER_BASE+TIMER_R_LSB)
#define __timer_csr *(volatile unsigned char *)(NEXT_TIMER_BASE+TIMER_R_CSR)

#define write_timer_ticks(x)  __timer_msb=((x)>>8)&0xff; \
	__timer_lsb=(x)&0xff

#define read_timer_ticks() (__timer_lsb+(__timer_msb<<8))

#define set_timer_csr(x) __timer_csr=(x)

#define set_timer_csr_bits(x) __timer_csr|=(x)

extern void next_sched_init(void);
extern void next_poweroff(void);
extern int next_hwclk(int, struct rtc_time*);
