#include <stdint.h>
#include <stdio.h>

#define NUMSIMMS 4

//struct prom_info {
struct __attribute__ ((packed)) prom_info {
	uint8_t	simm_something[NUMSIMMS];
	uint8_t	prom_flags;
	uint8_t	_pad1;
	uint32_t	stuff[4];

	struct nvram_settings {
		uint32_t	ni_reset; //,ni_alt_cons,ni_allow_eject,ni_vol_r,ni_brightness,ni_hw_pwd,ni_vol_l,ni_spkren,ni_lowpass,ni_boot_anyni_any_cmd;
		#define NVRAM_HW_PASSWD 6
		uint8_t		ni_ep[NVRAM_HW_PASSWD];
		uint16_t	ni_simm;
		char		ni_adobe[2];
		uint8_t		ni_pot[3];
		uint8_t 	ni_new_clock_chip; //,ni_auto_poweroni,ni_use_console_slot,ni_console_slot,unknown;
		#define	NVRAM_BOOTCMD	12
		char 		ni_bootcmd[NVRAM_BOOTCMD];
		uint16_t	ni_cksum;
	} nvram;

	uint8_t inetntoa[18],inputline[128];

	struct  mem_layout {
		uint32_t start,end;
	} simm_info[NUMSIMMS];

	uint32_t	base_ptr,brk_ptr;
	uint32_t	bootdev_ptr,bootarg_ptr,bootinfo_ptr,bootfile_ptr;
	uint8_t	bootfile[64];
	//uint32_t	boot_mode;
	enum SIO_ARGS {A,B,C} boot_mode;
	//uint8_t	km_mon_stuff[4+4+2+2+2+2+2+2+4+4+4+2+4+3*2+2]; 2B more than real?
	struct km_mon {
		//union	kybd_event kybd_event; //5?
		uint32_t kybd_event;
		//union	kybd_event autorepeat_event; //5?
		uint32_t autorepeat_event;
						     //+38 below
		int16_t	x, y;
		int16_t	nc_tm, nc_lm, nc_w, nc_h;
		int32_t	store;
		int32_t	fg, bg;
		int16_t	ansi;
		int32_t	cp;
		#define	KM_NP		3
		int16_t	p[KM_NP];
		volatile int16_t flags;
	} km_mon_stuff;
	/*

	int32_t	moninit;
	uint32_t	sio_ptr;
	int32_t	time;
	uint32_t	sddp_ptr,dgp_ptr,s5cp_ptr,odc_ptr,odd_ptr;
	uint8_t	radix;

	uint32_t	dmachip,diskchip;
	uint32_t	stat_ptr,mask_ptr;

	uint32_t	nofault_func;
	uint8_t	fmt;
	int32_t	addr[8],na;
	uint32_t	mousex,mousey;
	uint32_t	cursor[2][32];

	uint32_t	getc_func,try_getc_func,putc_func,
		alert_func,alert_conf_func,alloc_func,
		boot_slider_func;
	uint32_t	event_ptr;

	uint32_t	event_high;

	struct anim {
		uint16_t	x,y;
		uint32_t	icon_ptr;
		uint8_t	next;
		uint8_t	time;
	} *anim_ptr;
	int32_t	anim_time;

	uint32_t	scsi_intr_func;
	uint32_t	scsi_intr_arg;

	int16_t	minor,seq;

	uint32_t	anim_run_func;

	int16_t	major;

	uint32_t	client_ether_addr_ptr;

	int32_t	cons_i,cons_o;

	uint32_t	test_msg_ptr;

	struct nextfb_prominfo {
		uint32_t        pixels_pword;
		uint32_t        line_length;
		uint32_t        vispixx;
		uint32_t        realpixx;
		uint32_t        height;
		uint32_t        flags;
		uint32_t        whitebits[4];
		uint8_t         slot;
		uint8_t         fbnum;
		uint8_t         laneid;
		uint32_t        before_code;
		uint32_t        after_code;
		struct nextfb_businfo {
			uint32_t        phys;
			uint32_t        virt;
			uint32_t        len;
		} frames[6];
		uint32_t        stack;
	} fbinfo;

	uint32_t	fdgp_ptr;
	uint8_t	mach_type,next_board_rev;

	uint32_t	as_tune_func;
	int32_t	moreflags;*/
};

int main() {
/*	struct prom_info pi = {
		.moreflags=1
	};
	pi.as_tune_func=2;*/
	printf("size=%ld 0x%lx",sizeof(struct prom_info),sizeof(struct prom_info));
	return 0;
}
