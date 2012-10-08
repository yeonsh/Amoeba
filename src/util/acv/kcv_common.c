/*	@(#)kcv_common.c	1.1	96/02/27 12:42:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "acv.h"
#include "namelist.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "out.h"
#include "kcv_common.h"

/*
 * Routines common to the kcv kernel conversion programs.
 */

extern char *aout_getinfo();
extern char *progname;

void
convert(uxfile, amfile)
char *	uxfile;		/* unix file to convert */
char *	amfile;		/* name of amoeba file to be created */
{
    static struct aoutinfo ai;
    int	  fd;
    char *errstr;
    
    /* Open the file, read the header, and check format */
    if ((fd = open(uxfile, 0)) < 0)
	fatalperror("can't open %s", uxfile);
    if ((errstr = aout_getinfo(fd, &ai)) != 0)
	fatal(errstr);
    
    /*
    ** Check that the entry point is not lower that the text address;
    ** if this is so, the user probably forgot the -T flag to ld.
    */
    if (ai.ai_entrypoint < ai.ai_txt_virtual) {
	fatal("entry point (0x%lx) before text start (0x%x)",
		ai.ai_entrypoint, ai.ai_txt_virtual);
    }
    

    if (ai.ai_rom_size != 0 &&
	ai.ai_txt_virtual + ai.ai_txt_size != ai.ai_rom_virtual)
    {
	fatal("ROM doesn't follow text in address space");
    }

    dprintf(("text at offset %ld, addr 0x%lx, length %ld\n",
	     ai.ai_txt_offset, ai.ai_txt_virtual, ai.ai_txt_size));
    dprintf(("rom at offset %ld, addr 0x%lx, length %ld\n",
	     ai.ai_rom_offset, ai.ai_rom_virtual, ai.ai_rom_size));
    dprintf(("data at offset %ld, addr 0x%lx, length %ld\n",
	     ai.ai_dat_offset, ai.ai_dat_virtual, ai.ai_dat_size));
    dprintf(("bss addr 0x%lx, length %ld\n",
	     ai.ai_bss_virtual, ai.ai_bss_size));

    /* Symbol table segment */
    if (ai.ai_sym_tab != NULL || ai.ai_str_tab != NULL) {
	dprintf(("symbols size: %ld; string size: %ld (ignored)\n",
		 ai.ai_sym_size, ai.ai_str_size));
    }
    
    /* write the converted header */
    writefile(fd, amfile, &ai);

    (void) close(fd);
}

#define COPYSIZ	8192

void
fcopy(ifd, off, siz, ofd)
int ifd, ofd;
long off, siz;
{
    char buf[COPYSIZ];
    int writesiz;

    dprintf(("fcopy(%d, %ld, %ld, %d)\n", ifd, off, siz, ofd));

    if (lseek(ifd, off, 0)<0)
	fatal("seek failed");

    while (siz > 0) {
	writesiz = (siz >= COPYSIZ) ? COPYSIZ : siz;
	if (read(ifd, buf, writesiz) != writesiz)
	    fatal("short read");
	if (write(ofd, buf, writesiz) != writesiz)
	    fatal("write error");
	siz -= writesiz;
    }
}

/*VARARGS2*/
void
fatalperror(fmt, a1, a2, a3, a4)
char *	fmt;
char *	a1;
{
    perror(a1);
    fatal(fmt, a1, a2, a3, a4);
    /*NOTREACHED*/
}

/*VARARGS1*/
void
fatal(fmt, a1, a2, a3, a4)
char *	fmt;
{
    fprintf(stderr, "%s: ", progname);
    fprintf(stderr, fmt, a1, a2, a3, a4);
    fprintf(stderr, "\n");
    exit(1);
    /*NOTREACHED*/
}

void
usage()
{
    /* Print usage message and exit */
    fprintf(stderr, "usage: %s binary amoeba-kernel\n", progname);
    exit(2);
    /*NOTREACHED*/
}
