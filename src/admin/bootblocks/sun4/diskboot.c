/*	@(#)diskboot.c	1.2	94/04/06 11:36:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <mon/openprom.h>
#include "kbootdir.h"

#define LOADADDR	((char *) 0x180000)
#define AOUTHDRSIZ	32
#define SECSIZE		512

struct sunromvec *romp;

struct kerneldir kd;

putchar(c)
char c;
{

	if (romp->op_romvec_version <= 1) {
		(*romp->v_printf)("%c", c);
	} else {
		(*romp->op2_write)(*romp->op2_stdout, &c, 1);
	}
}

puts(s)
char *s;
{

	while (*s)
		putchar(*s++);
}

strcmp(s1, s2)
char *s1, *s2;
{

	while (*s1 == *s2) {
		if (*s1 == 0)
			return 0;
		s1++; s2++;
	}
	return *s1 - *s2;
}

char *
strchr(s, c)
char *s;
{	

	while (*s) {
		if (*s == c)
			return s;
		s++;
	}
	return 0;
}

strcpy(s1, s2)
char *s1, *s2;
{

	while (*s1++ = *s2++)
		;
}

clearbss() {
	char *p;
	extern char edata, end;

	for (p = &edata; p <= &end; p++)
		*p =0;
}

error(s)
char *s;
{

	puts("Fatal error: "); puts(s); puts("\r\n");
	(*romp->op_boot)(0);
}

#ifdef notdef
putd(d)
unsigned d;
{
	unsigned l;

	l = d>>4;
	if (l != 0)
		putd(l);
	putchar("0123456789ABCDEF"[d&0xF]);
}

deb(s, n) {

	puts(s);
	puts(" = ");
	putd(n);
	puts("\r\n");
}
#else
#define deb(a,b)	/* nothing */
#endif

ihandle_t diskhandle;

opendisk()
{

	if (romp->op_romvec_version >= 2) {
		diskhandle = (*romp->op2_open)(*romp->op2_bootpath);
		deb("diskhandle", diskhandle);
	} else {
		diskhandle = (*romp->v_open)(*romp->v_bootcommand);
		deb("diskhandle", diskhandle);
	}
}

closedisk()
{

	if (romp->op_romvec_version >= 2) {
		(*romp->op2_close)(diskhandle);
	} else {
		(*romp->v_close)(diskhandle);
	}
}

readblocks(startsector, nsectors, memaddr)
char *memaddr;
{
	int n;

	if (romp->op_romvec_version >= 2) {
		n = (*romp->op2_seek)(diskhandle, 0, startsector*512);
		deb("seek", n);
		n = (*romp->op2_read)(diskhandle, memaddr, nsectors*512);
		deb("read", n);
	} else {
		n = (*romp->v_read_blocks)(diskhandle, nsectors, startsector, memaddr);
		deb("read_blocks", n);
		deb("memaddr", *((long *) memaddr));
	}
}

char *
parameter()
{
	static char buffer[256];
	char *closeparen;

	if (romp->op_romvec_version >= 2) {
		return (*romp->op2_bootargs);
	} else {
		closeparen = strchr(*romp->v_bootcommand, ')');
		if (closeparen == 0)
			error("No ) in bootcommand");
		strcpy(buffer, closeparen+1);
		closeparen[1] = 0;
		return buffer;
	}
}

whichkernel(name)
char *name;
{
	int i;

	if (*name == 0)
		return 0;	/* Take the first */
	for (i=0; i<KD_NENTRIES; i++)
		if (strcmp(name, kd.kd_entry[i].kde_name) == 0)
			return i;
	puts("Available kernels: ");
	for (i=0; i<KD_NENTRIES && kd.kd_entry[i].kde_secnum; i++) {
		puts(kd.kd_entry[i].kde_name);
		puts(" ");
	}
	puts("\r\nUsing default\r\n");
	return 0;
}

char *
readkernel(romppar, sectorno) 
struct sunromvec *romppar;
{
	int *kerneldata;
	char *kernelname;
	int kernelentry;

	clearbss();
	romp = romppar;
	kernelname = parameter();
	opendisk();
	readblocks(sectorno, 1, (char *) &kd);
	if (kd.kd_magic != KD_MAGIC)
		error("bad magic number in kerneldir");
	kernelentry = whichkernel(kernelname);
	readblocks(sectorno+kd.kd_entry[kernelentry].kde_secnum,
		kd.kd_entry[kernelentry].kde_size, LOADADDR-AOUTHDRSIZ);
	closedisk();
	kerneldata =(int *) LOADADDR;
	deb("word 0", *kerneldata++);
	deb("word 1", *kerneldata++);
	deb("word 2", *kerneldata++);
	deb("word 3", *kerneldata++);
	deb("word 4", *kerneldata++);
	return LOADADDR;
}
