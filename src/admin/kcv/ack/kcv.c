/*	@(#)kcv.c	1.3	96/02/27 10:16:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <out.h>

#ifndef SEEK_SET
#define SEEK_SET	0
#endif

/*
 * kcv: converts an ACK format Amoeba kernel to a binary suitable
 * for downloading or (after stripping off the 120 bytes ACK header)
 * booting with tftp.
 *
 * The main problem with the ACK binary we get from led, is that sometimes
 * it ``optimises'' the output file by leaving out a few alignment bytes.
 * This program sticks them back in again, so that the file image after
 * the ACK header can be loaded directly into memory (tftp for Sun3's
 * depends on this, because it doesn't have an XXX.out header telling
 * about offsets, sizes, etc).
 *
 * For download, we also round up the "os_size" fields to the next segment,
 * because (strangely enough) it uses this rather than "os_flen" to determine
 * the amount of bytes to download.
 *
 * NB. kcv doesn't try to preserve the namelist of the ACK kernel, currently.
 */

#ifdef __STDC__
void convert(struct outhead *headp, char *in_name, char *out_name);
void fatal(char *format, ...);
void verbose(char *format, ...);
#else
void convert();
void fatal();
void verbose();
#endif

char *prog = "kcv";
int be_verbose = 0;

void
usage()
{
    fatal("usage: %s [-v] <ack-kernel> <converted-ack-kernel>", prog);
}

void
main(argc, argv)
int	argc;
char	*argv[];
{

    struct outhead header;
    char *ack_kernel, *image_kernel;

    /* argument processing: */
    extern int getopt();
    extern int optind;
    extern char *optarg;
    int c;

    prog = argv[0];

    while ((c = getopt(argc, argv, "v")) != EOF) {
	switch (c) {
	case 'v':
	    be_verbose = 1;
	    break;
	default:
	    usage();
	    break;
	}
    }
	    
    if (argc - optind != 2) {
	usage();
    }
    
    ack_kernel = argv[optind];
    image_kernel = argv[optind + 1];

    if (!rd_open(ack_kernel)) {
	fatal("%s: cannot read %s", prog, ack_kernel);
    }

    rd_ohead(&header);
    if (BADMAGIC(header)) {
	fatal("%s: %s not an ACK object file", prog, ack_kernel);
    } else {
	convert(&header, ack_kernel, image_kernel);
    }

    exit(0);
}

#define COPYSIZ 8192

void
fcopy(ifp, off, siz, ofp)
FILE *ifp, *ofp;
long off, siz;
{
    char buf[COPYSIZ];

    if (fseek(ifp, off, SEEK_SET) < 0) {
        fatal("seek to %ld failed", off);
    }

    while (siz > 0) {
        int writesiz = (siz >= COPYSIZ) ? COPYSIZ : siz;

        if (fread(buf, writesiz, 1, ifp) != 1) {
            fatal("could not read %d bytes", writesiz);
	}
        if (fwrite(buf, writesiz, 1, ofp) != 1) {
            fatal("could not write %d bytes");
	}
        siz -= writesiz;
    }
}

/* buffer filled with '\0' chars, used for filling out spaces */
char null_buf[COPYSIZ];

int
write_zeros(ofp, siz)
FILE *ofp;
long siz;
{
    while (siz > 0) {
        int writesiz = (siz >= COPYSIZ) ? COPYSIZ : siz;

        if (fwrite(null_buf, writesiz, 1, ofp) != 1) {
            fatal("could not write %d bytes");
	}
        siz -= writesiz;
    }
}

#define MAX_SECT 6	/* should be enough */

void
convert(headp, in_name, out_name)
struct outhead	*headp;
char *in_name, *out_name;
{
    register unsigned short i;
    FILE *fp_in, *fp_out;
    long virtual_start = -1;
    long cur_pos = 0;	/* virtual "current position" in output file */
    struct outsect origsect[MAX_SECT];
    struct outsect newsect[MAX_SECT];
    struct outhead newhead;

    if (headp->oh_nsect > MAX_SECT) {
	fatal("too many sections (%d, max is %d)", headp->oh_nsect, MAX_SECT);
    }

    /* create header with possibly patched file offsets */
    if (!wr_open(out_name)) {
	fatal("couldn't open %s", out_name);
    }
    newhead = *headp;
    newhead.oh_nchar = 0;	/* as we don't copy that one */
    wr_ohead(&newhead);

    /*
     * read original sections
     */
    for (i = 0; i < headp->oh_nsect; i++) {
	rd_sect(&origsect[i], 1);
    }
    rd_close();

    /*
     * modify section offsets and sizes when needed
     */
    for (i = 0; i < headp->oh_nsect; i++) {
	verbose("Section %d:", i);

	/* new section is copy of the old one + some modifications: */
	newsect[i] = origsect[i];

	/* only sections that reside in the file need to be patched: */
	if (origsect[i].os_flen != 0) {
	    verbose("\tvirtual startaddress\t%lx", origsect[i].os_base);
	    verbose("\tstartaddress in file\t%ld", origsect[i].os_foff);
	    verbose("\tsection size in file\t%ld", origsect[i].os_flen);

	    if (virtual_start == -1) { /* first section */
		virtual_start = origsect[i].os_base;
		cur_pos = newsect[i].os_foff;
	    } else {
		long new_pos;

		new_pos = newsect[0].os_foff +
		    	    (origsect[i].os_base - virtual_start);
		if (new_pos != cur_pos) {
		    verbose("*** position %ld is patched to %ld",
			    cur_pos, new_pos);
		    cur_pos = new_pos;
		}
		newsect[i].os_foff = cur_pos;
	    }

	    /* round up size to next section in the file (if there is one) */
	    if ((i < headp->oh_nsect - 1) &&	/* there are more sections */
		(origsect[i+1].os_flen != 0)) 	/* and next one is in file*/
	    {
		newsect[i].os_size =
		    origsect[i+1].os_base - origsect[i].os_base;

		if (origsect[i].os_size != newsect[i].os_size) {
		    verbose("*** section size changed from %ld to %ld",
			    origsect[i].os_size, newsect[i].os_size);
		}
	    }

	    /* file size will be made equal to virtual size, so: */
	    newsect[i].os_flen = newsect[i].os_size;

	    /* update current position */
	    cur_pos += newsect[i].os_flen;
	}

	wr_sect(&newsect[i], 1);
    }
    wr_close();

    /*
     * Append all sections having nonzero file offset to out_name,
     * taking care to get the offsets right.
     */

    if ((fp_in = fopen(in_name, "r")) == NULL) {
	fatal("%s: cannot open `%s' for reading", prog, in_name);
    }
    if ((fp_out = fopen(out_name, "a")) == NULL) {
	fatal("%s: cannot open `%s' for writing", prog, out_name);
    }

    for (i = 0; i < headp->oh_nsect; i++) {
	if (origsect[i].os_flen != 0) {
	    long extra_bytes;

	    /* out_fp should already be at the correct position; check it */
	    if (ftell(fp_out) != newsect[i].os_foff) {
		fatal("unexpected position in %s: %ld instead of %ld)",
		      out_name, ftell(fp_out), newsect[i].os_foff);
	    }

	    /* copy the section */
	    fcopy(fp_in, origsect[i].os_foff, origsect[i].os_flen, fp_out);

	    verbose("copied section %d: %ld bytes from offset %ld to %ld",
		    i, origsect[i].os_flen,
		    origsect[i].os_foff, newsect[i].os_foff);

	    /* Add extra bytes if os_size was enlarged a bit */
	    extra_bytes = newsect[i].os_flen - origsect[i].os_flen;
	    if (extra_bytes > 0) {
		/* we cannot fseek() and write one 0 byte, because of
		 * ANSI C's peculiar fopen("a") semantics.
		 */
		write_zeros(fp_out, extra_bytes);
		verbose("*** added %ld extra bytes", extra_bytes);
	    }
	}
    }

    fclose(fp_out);
    fclose(fp_in);
}

rd_fatal() { fatal("Error in reading the object file"); }
wr_fatal() { fatal("Error in writing the object file"); }

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef __STDC__
void fatal(char *format, ...)
{
    va_list ap; va_start(ap, format);
#else
void fatal(format, va_alist) char *format; va_dcl
{
    va_list ap; va_start(ap);
#endif

    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

#ifdef __STDC__
void verbose(char *format, ...)
{
    va_list ap; va_start(ap, format);
#else
void verbose(format, va_alist) char *format; va_dcl
{
    va_list ap; va_start(ap);
#endif

    if (be_verbose) {
	vfprintf(stdout, format, ap);
	fprintf(stdout, "\n");
    }
    va_end(ap);
}

