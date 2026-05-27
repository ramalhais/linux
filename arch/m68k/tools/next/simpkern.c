/* based on my machead.c
emits a simple mach-o object
with only a single text segment.

usage:

simpkern <infile> <filename>

simpkern takes <infile> which is literal
assembled code in binary format, with
no object file headers of any sort, and
tacks a valid mach-o header in front of
it and outputs to <outfile>
*/

/*
 * April 20, 1998: Little patch to compile on NeXT --Antonio
 */

/* #define HEADSIZE 28+56+68 */
/* #define CMDSIZE 56+68 */

/* #define LOADADDR 0x04380000 */
/* Hack hack */
#define LOADADDR 0x04001000 // NeXT physical memory starts at 0x04000000 (first slot?). linux kernel starts at offset 0x1000 (4KB space for stack?)

#define HEADSIZE sizeof(struct mach_header)+sizeof(struct segment_command)+sizeof(struct section)
#define CMDSIZE sizeof(struct segment_command)+sizeof(struct section)

#include "header.h"
#include "loadcommands.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#ifdef __LINUX__
#include <unistd.h>
#endif

FILE	*macho;

void lput(long l)
{

        fputc(l>>24,macho);
        fputc(l>>16,macho);
        fputc(l>>8,macho);
        fputc(l,macho);
}

void s16put(char *n)
{
        char name[16];
        int i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(name, n, sizeof(name));
#pragma GCC diagnostic pop
        for(i=0; i<sizeof(name); i++)
                fputc(name[i],macho);
}

long rnd(long v, long r)
{
        long c;

        if(r <= 0)
                return v;
        v += r - 1;
        c = v % r;
        if(c < 0)
                c += r;
        v -= c;
        return v;
}

void write_mach_header(struct mach_header *header)
{
lput(header->magic);
lput(header->cputype);
lput(header->cpusubtype);
lput(header->filetype);
lput(header->ncmds);
lput(header->sizeofcmds);
lput(header->flags);
}

void write_mach_segcmd(struct segment_command *segcmd)
{
lput(segcmd->cmd);
lput(segcmd->cmdsize);
s16put(segcmd->segname);
lput(segcmd->vmaddr);
lput(segcmd->vmsize);
lput(segcmd->fileoff);
lput(segcmd->filesize);
lput(segcmd->maxprot);
lput(segcmd->initprot);
lput(segcmd->nsects);
lput(segcmd->flags);
}

void write_mach_section(struct section *sect)
{
s16put(sect->sectname);
s16put(sect->segname);
lput(sect->addr);
lput(sect->size);
lput(sect->offset);
lput(sect->align);
lput(sect->reloff);
lput(sect->nreloc);
lput(sect->flags);
lput(sect->reserved1);
lput(sect->reserved2);
}

void help(void){
	printf("\n\nsimpkern <infile> <outfile>\n\n");
	exit(1);
}

int main(int argc, char *argv[]){

	long	real_code_size;
	char	outfile[81];
	char	infile[81];
	struct	stat	instat;
	struct	mach_header head;
	struct	segment_command texseg;
	struct	section texsec;
	FILE*	machi;
	void	*codebuf;
	int	i;

	if(argc!=3) help();

	strncpy(infile,argv[1],80);
	strncpy(outfile,argv[2],80);

	stat((const char *)&infile,&instat);
	real_code_size=instat.st_size;
	codebuf=malloc(real_code_size);

	head.magic=MH_MAGIC;
	head.cputype=CPU_TYPE_MC680x0;
	head.cpusubtype=CPU_TYPE_MC68040;
	head.filetype=MH_PRELOAD;
	head.ncmds=1;
	head.sizeofcmds=CMDSIZE;
	head.flags=MH_NOUNDEFS;

	texseg.cmd=LC_SEGMENT;
	texseg.cmdsize=sizeof(struct segment_command)+sizeof(struct section);
	strcpy(texseg.segname, SEG_TEXT);
	texseg.vmaddr=LOADADDR;
	texseg.vmsize=rnd(real_code_size,8192);
	texseg.fileoff=HEADSIZE;
	texseg.filesize=rnd(real_code_size,8192);
	texseg.maxprot=VM_PROT_DEFAULT;
	texseg.initprot=VM_PROT_DEFAULT;
	texseg.nsects=1;
	texseg.flags=0;

	strcpy(texsec.sectname, SECT_TEXT);
	strcpy(texsec.segname, SEG_TEXT);
	texsec.addr=LOADADDR;
	texsec.size=real_code_size;
	texsec.offset=HEADSIZE;
	texsec.align=2;
	texsec.reloff=0;
	texsec.nreloc=0;
	texsec.flags=0;
	texsec.reserved1=0;
	texsec.reserved2=0;

	machi=fopen(infile,"r");
	macho=fopen(outfile,"w");

	write_mach_header(&head);
	write_mach_segcmd(&texseg);
	write_mach_section(&texsec);
	/* and copy the binary code */
	if (!fread(codebuf,real_code_size,1,machi)) {
		printf("ERROR: Unable to read input file");
		exit(1);
	}
	fwrite(codebuf,real_code_size,1,macho);
	/* Fill in the final 8K page with NULLs */
	for(i=0;i<(8192-(real_code_size%8192));i++)
		fputc(0,macho);

	fclose(macho);

	exit(0);
}
