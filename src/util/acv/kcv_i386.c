/*	@(#)kcv_i386.c	1.3	96/02/27 12:42:22 */
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

char * progname = "KCV-i386";	/* Program name -- overridden by argv[0] */

main(argc, argv)
int	argc;
char **	argv;
{
    progname = argv[0];
    
    if (argc != 3) {
	usage();
    }

    /* Args are <unixfile> <amoebafile> */
    convert(argv[1], argv[2]);

    exit(0); /* Convert will call exit(1) on failure */
}

/* from modules/object.h: */
#define Xput2(i, c)     (((c)[0] = (i)), ((c)[1] = (i) >> 8))
#define put2(i, c)      { register int j = (i); Xput2(j, c); }
#define put4(l, c)      { register long x=(l); \
                          Xput2((int)x,c); \
                          Xput2((int)(x>>16),(c)+2); \
                        }

/*
** Create the amoeba i386 kernel file (ACK format binary).
*/

void
writefile(fd, amfile, ai)
int		 fd;		/* In: file descriptor for Unix file */
char *		 amfile;	/* In: name of amoeba a.out file to produce */
struct aoutinfo	*ai;		/* In: structure with info */
{
    struct outhead head;
    struct outsect sects[MAXSECT], *sect;
    int		   ofd;
    long	   foff;
    unsigned int   sectno;
    
    if ((ofd = creat(amfile, 0666))<0) {
	fatal("Can't create %s", amfile);
    }

    /* Construct the ACK header */
    head.oh_magic = O_MAGIC;
    head.oh_stamp = O_STAMP;
    head.oh_flags = 0;		/* no unresolved references */
    head.oh_nsect = 4;		/* text, rom, data and bss */
    head.oh_nrelo = 0;		/* no relocation */
    head.oh_nname = 0;		/* no names */
    head.oh_nemit = 0;		/* ? */
    head.oh_nchar = 0;		/* no strings */

    /* write the ACK header */
    {
	char buf[SZ_HEAD];
	register char *p = &buf[0];

	put2(head.oh_magic, p);        p += 2;
	put2(head.oh_stamp, p);        p += 2;
	put2(head.oh_flags, p);        p += 2;
	put2(head.oh_nsect, p);        p += 2;
	put2(head.oh_nrelo, p);        p += 2;
	put2(head.oh_nname, p);        p += 2;
	put4(head.oh_nemit, p);        p += 4;
	put4(head.oh_nchar, p);
	if (write(ofd, buf, SZ_HEAD) != SZ_HEAD) {
	    fatal("could not write ACK header");
	}
    }
    
    /* The text segment starts right after the ACK headers */
    foff = SZ_HEAD + head.oh_nsect * SZ_SECT;
    
    /* Construct the ACK sections headers */
    sect = &sects[0];

    /* text */
    sect->os_base = ai->ai_txt_virtual;
    sect->os_size = ai->ai_txt_size;
    sect->os_foff = foff;
    sect->os_flen = ai->ai_txt_size;
    sect->os_lign = 4;	/* check this */
    dprintf(("in ack binary: text at offset %ld, size %ld, virtual 0x%lx\n",
	     foff, sect->os_size, sect->os_base));
    foff += sect->os_flen;

    /* rom */
    sect++;
    if (ai->ai_rom_size <= 0) {
	dprintf(("no rom; creating dummy segment\n"));
	ai->ai_rom_virtual = 0;
	ai->ai_rom_size = 0;
    }
    sect->os_base = ai->ai_rom_virtual;
    sect->os_size = ai->ai_rom_size;
    sect->os_foff = foff;
    sect->os_flen = ai->ai_rom_size;
    sect->os_lign = 4;	/* check this */
    dprintf(("in ack binary: rom at offset %ld, size %ld, virtual 0x%lx\n",
	     foff, sect->os_size, sect->os_base));
    foff += sect->os_flen;

    /* data */
    sect++;
    sect->os_base = ai->ai_dat_virtual;
    sect->os_size = ai->ai_dat_size;
    sect->os_foff = foff;
    sect->os_flen = ai->ai_dat_size;
    sect->os_lign = 4;	/* check this */
    dprintf(("in ack binary: data at offset %ld, size %ld, virtual 0x%lx\n",
	     foff, sect->os_size, sect->os_base));
    foff += sect->os_flen;

    /* bss */
    sect++;
    sect->os_base = ai->ai_bss_virtual;
    sect->os_size = ai->ai_bss_size;
    sect->os_foff = 0;	/* not in the file */
    sect->os_flen = 0;
    sect->os_lign = 4;	/* check this */
    dprintf(("in ack binary: bss size %ld, virtual 0x%lx\n",
	     sect->os_size, sect->os_base));
    foff += sect->os_flen;

    /* Write the ACK section headers */
    for (sectno = 0; sectno < head.oh_nsect; sectno++) {
	char buf[SZ_SECT];
	char *p = &buf[0];

	sect = &sects[sectno];
	put4(sect->os_base, p); p += 4;
	put4(sect->os_size, p); p += 4;
	put4(sect->os_foff, p); p += 4;
	put4(sect->os_flen, p); p += 4;
	put4(sect->os_lign, p); p += 4;
	if (write(ofd, buf, SZ_SECT) != SZ_SECT) {
	    fatal("could not write section header %d", sectno);
	}
    }

    /* Finally write the text, rom and data sections contents */
    fcopy(fd, ai->ai_txt_offset, ai->ai_txt_size, ofd);
    if (ai->ai_rom_size > 0) {
	fcopy(fd, ai->ai_rom_offset, ai->ai_rom_size, ofd);
    }
    fcopy(fd, ai->ai_dat_offset, ai->ai_dat_size, ofd);

    (void) close(ofd);
}

