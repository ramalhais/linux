#define MG_PUTC_OFFSET	734
#define PREVIOUS_DEBUG	0x0200f004

void mr_putl(unsigned long val);
void puts(char *s);
void (*mg_putc_global)(unsigned long);
void main();
void main2();
void main3();

void _start() {
    main();
//    main2();
//    main3();

    while(1) {
    }
}

void main2() {
    unsigned long putc = 16806308; // 0x10071a4
    asm("movel #16806308,%a0");
    asm("movel #65,%sp@-");
    asm("jsr %a0@");
    asm("addql #4,%sp");

    asm("movel %0,%d0" : : "r" (putc));
    asm("movel %d0,%a0");
    asm("movel #66,%sp@-");
    asm("jsr %a0@");
    asm("addql #4,%sp");

    asm("movel %0,%d0" : : "r" (putc));
    asm("movel %d0,%a0");
    asm("movel #67,%sp@-");
    asm("jsr %a0@");
    asm("addql #4,%sp");

}

void main3() {
    //unsigned long *mr315 = (unsigned long *)0x02106000+0x15; // explodes qemu in assert
    //unsigned long *mr315 = (unsigned long *)0x2004200;
    unsigned long *mr315 = (unsigned long *)PREVIOUS_DEBUG;
    *mr315 = 0x77;

    // netbsd
    // clear and disable on-chip cache(s)
//    asm("movl #0x0808,%d0\n"
//        "movc %d0,%cacr");
    // make sure we disallow interrupts
    // asm("movew   #0x2600,%sr");

    // Tests:
    // asm("movew #0x2700,%sr");
    // asm(".chip 68040\n"
    //     "movl #0x0,%d0\n"
    //     "movc %d0,%tc\n"
    //     "movc %d0,%cacr\n"
    //     ".chip 68k\n");

    // save vbr. it contains:
    // mg(monitor global) long at byte 4
    // vector for trap #13 long at byte 180
    // vector for int 7 long at byte 124
    void *vbr;
    *mr315 = 1;
    asm("movec %%vbr,%0" : "=r" (vbr));
    *mr315 = *(unsigned long *)vbr;

    // disable interrupts

    *mr315 = 2;
    void *mg = (char *)*(unsigned long *)(vbr+4); // 4 bytes over
    *mr315 = (unsigned long)mg;
    *mr315 = *((unsigned long *)mg);

    *mr315 = 3;
#define	MON(type, off) (*(type *)((unsigned long) (mg) + off))
typedef int (*putcptr)(int);
    void (*mg_putc)(char) = (void (*)(char))*(unsigned long *)(mg+MG_PUTC_OFFSET);
    void (*mg_putc_global)(unsigned long) = (void (*)(unsigned long))*(unsigned long *)(mg+MG_PUTC_OFFSET);
    *mr315 = (unsigned long)mg_putc;
    *mr315 = (unsigned long)*mg_putc;

    *mr315 = 4;
    char s[]="Esta a bombar caralho!\n";
    *mr315 = 5;
    char *c=(char *)s;
    *mr315 = 6;

    while(*c) {
	*mr315 = *c;
	// mr_putl(*c);
	mg_putc(*c);
//        MON(putcptr,MG_PUTC_OFFSET)(*c);
	c++;
    }

    *mr315 = 7;
    puts((char *)&s);

    *mr315 = 8;
    char s2[]="E esta?\n";
    *mr315 = 9;
    //puts(s2);
    *mr315 = 10;

    while(1) {
    }
    //asm("rts");
}

// Must be first function implementation, so that entrypoint matches
void main() {
    // void *mr315 = (void *)0x02106000+0x15; // explodes qemu in assert
    unsigned long *mr315 = (unsigned long *)PREVIOUS_DEBUG;

    *mr315 = 0x77;

    // netbsd
    // clear and disable on-chip cache(s)
//    asm("movl #0x0808,%d0\n"
//        "movc %d0,%cacr");
    // make sure we disallow interrupts
    // asm("movew   #0x2600,%sr");

    // Tests:
    // asm("movew #0x2700,%sr");
    // asm(".chip 68040\n"
    //     "movl #0x0,%d0\n"
    //     "movc %d0,%tc\n"
    //     "movc %d0,%cacr\n"
    //     ".chip 68k\n");

    // save vbr. it contains:
    // mg(monitor global) long at byte 4
    // vector for trap #13 long at byte 180
    // vector for int 7 long at byte 124
    void *vbr;
    *mr315 = 1;
    asm("movec %%vbr,%0" : "=r" (vbr));
    *mr315 = *(unsigned long *)vbr;

    // disable interrupts

    *mr315 = 2;
    void *mg = (char *)*(unsigned long *)(vbr+4); // 4 bytes over
    *mr315 = (unsigned long)mg;
    *mr315 = *((unsigned long *)mg);

    *mr315 = 3;
#define	MON(type, off) (*(type *)((unsigned long) (mg) + off))
typedef int (*putcptr)(int);
    void (*mg_putc)(char) = (void (*)(char))*(unsigned long *)(mg+MG_PUTC_OFFSET);
    void (*mg_putc_global)(char) = (void (*)(char))*(unsigned long *)(mg+MG_PUTC_OFFSET);
    *mr315 = (unsigned long)mg_putc;
    *mr315 = (unsigned long)*mg_putc;

    *mr315 = 4;
    char s[]="Esta a bombar caralho!\n";
    *mr315 = 5;
//    puts((char *)&s);
    *mr315 = 6;
    char *c=(char *)s;
    *mr315 = 7;

    while(*c) {
		*mr315 = *c;
		// mr_putl(*c);
		mg_putc(*c);
//		MON(putcptr,MG_PUTC_OFFSET)(*c);
		c++;
    }

    *mr315 = 8;
    char s2[]="E esta?\n";
    *mr315 = 9;
    //puts(s2);
    *mr315 = 10;

    while(1) {
    }
    //asm("rts");
}

void mr_putl(unsigned long val) {
    // *mr315 = val;
}

void puts(char *s) {
    unsigned long *mr315 = (unsigned long *)PREVIOUS_DEBUG;

    *mr315 = 0x78;

    char *c=s;
    mg_putc_global(*c);
    *mr315 = 0x79;
    while(*c) {
		*mr315 = *c;
		// mr_putl(*c);
		mg_putc_global(*c);
//		MON(putcptr,MG_PUTC_OFFSET)(*c);
		c++;
    }

    *mr315 = 0x80;

//    unsigned long *mr315 = (unsigned long *)0x02004100+0x100;
//    *mr315 = 0xf;
//     char *c=(char *)s;
//     while(*c) {
//         // mr_putl(*c);
//     	// mg_putc(*c);
// //        MON(putcptr,MG_PUTC_OFFSET)(*c);
//         c++;
//     }
}
