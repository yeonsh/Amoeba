/*	@(#)isamkimage.c	1.3	96/02/27 10:05:50 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * isamkimage.c
 *
 * This program is used to create bootable images from a kernel binary
 * for an ISA or MCA based machine.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <out.h>
#include <stdio.h>
#include <amoeba.h>
#include <byteorder.h>

struct kernelinfo {
    uint32 ki_textsize;		/* text and rom size */
    uint32 ki_textbase;		/* text and rom base */
    uint32 ki_datasize;		/* data size */
    uint32 ki_database;		/* data base */
    uint32 ki_bsssize;		/* bss size */
    uint32 ki_bssbase;		/* bss base */
} kernelinfo;

int verbose = 0;

void copysegment();

main(argc, argv)
    int argc;
    char **argv;
{
    struct outhead outhead;
    struct outsect outsect;
    extern char *optarg;
    extern int optind;
    uint8 length = 0;
    int kernelsize;
    int kfd, rfd;
    int opt;
    unsigned short i;
    int size = -1;

    while ((opt = getopt(argc, argv, "vs:")) != EOF) {
	switch (opt) {
	case 's':
	    size = atoi(optarg);
	    break;
	case 'v':
	    verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (optind + 1 >= argc) usage(argv[0]);

    if ((kfd = open(argv[optind], 0)) < 0) {
	perror(argv[optind]);
	exit(1);
    }

    read(kfd, &outhead, sizeof(struct outhead));
    dec_s_le(&outhead.oh_magic);
    if (outhead.oh_magic != O_MAGIC) {
	printf("Kernel has bad magic\n");
	exit(1);
    }
    dec_s_le(&outhead.oh_stamp);
    if (verbose) printf("Version stamp %o:\n", outhead.oh_stamp);

    kernelinfo.ki_textsize = kernelinfo.ki_textbase = 0;
    kernelinfo.ki_datasize = kernelinfo.ki_database = 0;
    kernelinfo.ki_bsssize = kernelinfo.ki_bssbase = 0;

    /*
     * Go through all the sections, extracting the relevant parts
     */
    dec_s_le(&outhead.oh_nsect);
    for (i = 0; i < outhead.oh_nsect; i++) {
	read(kfd, &outsect, sizeof(struct outsect));
	dec_l_le(&outsect.os_base);
	dec_l_le(&outsect.os_size);
	dec_l_le(&outsect.os_lign);
	if (verbose)
	    printf("Section %d: base 0x%lx, size 0x%lx, align 0x%lx\n",
		i, outsect.os_base, outsect.os_size, outsect.os_lign);
	switch (i) {
	case 0:	/* text section */
	    kernelinfo.ki_textsize += outsect.os_size;
	    kernelinfo.ki_textbase += outsect.os_base;
	    break;
	case 1:	/* rom section */
	    kernelinfo.ki_textsize += outsect.os_size;
	    break;
	case 2: /* data section */
	    kernelinfo.ki_datasize += outsect.os_size;
	    kernelinfo.ki_database += outsect.os_base;
	    break;
	case 3:	/* bss section */
	    kernelinfo.ki_bsssize += outsect.os_size;
	    kernelinfo.ki_bssbase += outsect.os_base;
	    break;
	default:
	    break;
	}
    }
    kernelinfo.ki_bsssize = (kernelinfo.ki_bsssize + 0x0F) & ~0x0F;

    /*
     * Bound check. Note that the following expression actually
     * describes textsize + textpadding + datasize. Data padding
     * does not have to be stored in the image.
     */
    kernelsize =
	kernelinfo.ki_datasize+kernelinfo.ki_database-kernelinfo.ki_textbase;
    if (size > 0 && kernelsize > (size * 1024)) {
	fprintf(stderr, "%s: kernel size (%d bytes) exceeds pad size\n",
	    argv[0], kernelsize);
	exit(1);
    }

    close(creat(argv[optind+1], 0666));
    if ((rfd = open(argv[optind+1], 2)) < 0) {
	perror(argv[optind+1]);
	exit(1);
    }

    /*
     * Load text segment (padding is essential)
     */
    copysegment(kfd, rfd, kernelinfo.ki_textsize,
	kernelinfo.ki_database-kernelinfo.ki_textbase-kernelinfo.ki_textsize);

    /*
     * Load data segment pad when requested
     */
    if (size > 0)
	copysegment(kfd, rfd, kernelinfo.ki_datasize, (size*1024)-kernelsize);
    else
	copysegment(kfd, rfd, kernelinfo.ki_datasize, 0);
	
    close(kfd);
    close(rfd);
    exit(0);
}

usage(program)
    char *program;
{
    fprintf(stderr, "Usage: %s [-v] [-s pad] kernel bootable-image\n", program);
    exit(1);
}

/*
 * Copy a segment, keeping into account the padding that's needed.
 */
void
copysegment(ifd, ofd, size, pad)
    int ifd, ofd, size, pad;
{
    register int i, nbytes;
    char buf[BUFSIZ];
    uint8 zero;

    for (; size > 0; size -= nbytes) {
	nbytes = size > BUFSIZ ? BUFSIZ : size;
	if (read(ifd, buf, nbytes) != nbytes) {
	    perror("read");
	    exit(1);
	}
	write(ofd, buf, nbytes);
    }
    for (i = 0, zero = 0; i < pad; i++)
	write(ofd, &zero, sizeof(zero));
}
