/*	@(#)kcv_elf.c	1.1	96/02/27 12:42:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "acv.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "my_aout.h"
#include "kcv_common.h"

char * progname = "KCV-ELF";	/* Program name -- overridden by argv[0] */

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

/*
** Create the amoeba sparc kernel file (a.out format binary).
*/

void
writefile(fd, amfile, ai)
int		 fd;		/* In: file descriptor for Unix file */
char *		 amfile;	/* In: name of amoeba a.out file to produce */
struct aoutinfo	*ai;		/* In: structure with info */
{
    struct exec	exec_hdr;
    int		ofd;
    
    if ((ofd = creat(amfile, 0666)) < 0) {
	fatal("Can't create %s", amfile);
    }

    if (ai->ai_dat_offset > ai->ai_txt_offset + ai->ai_txt_size) {
	int extra = ai->ai_dat_offset - (ai->ai_txt_offset + ai->ai_txt_size);

	dprintf(("datoff %ld != txtoff %ld + txt siz %ld; extra %d\n",
		 ai->ai_dat_offset, ai->ai_txt_offset, ai->ai_txt_size,
		 extra));
	ai->ai_txt_size += extra;
    }

    /* construct the a.out style exec header */
    exec_hdr.a_magic = (0403 << 16) | 0407; /* SPARC/OMAGIC */
    exec_hdr.a_text = ai->ai_txt_size;
    exec_hdr.a_data = ai->ai_dat_size;
    exec_hdr.a_bss = ai->ai_bss_size;
    exec_hdr.a_syms = 0;	/* strip it */
    exec_hdr.a_entry = ai->ai_entrypoint;
    exec_hdr.a_trsize = 0;	/* no text relocation */
    exec_hdr.a_drsize = 0;	/* no data relocation */
    if (write(ofd, (char *) &exec_hdr, sizeof(exec_hdr)) != sizeof(exec_hdr)) {
	fatal("Can't write to %s", amfile);
    }

    /* Put the text and data sections right after the exec header */
    fcopy(fd, ai->ai_txt_offset, ai->ai_txt_size, ofd);
    fcopy(fd, ai->ai_dat_offset, ai->ai_dat_size, ofd);

    (void) close(ofd);
}

