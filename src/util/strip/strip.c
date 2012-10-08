/*	@(#)strip.c	1.6	96/03/01 16:55:59 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* strip - remove symbols.		Author: Dick van Veen */
/* Heavily mangled to let it work under Amoeba by versto@cs.vu.nl */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "amoeba.h"
#include "stderr.h"
#include "module/proc.h"
#include "module/name.h"

/* Strip [file] ...
 *
 *	-	when no file is present, a.out is assumed.
 *
 */

#define A_OUT		"a.out"
#define NAME_LENGTH	128	/* max file path name */

char new_file[NAME_LENGTH];	/* contains name of temporary */

int status = 0;			/* exit status */

main(argc, argv)
int argc;
char **argv;
{
    argv++;
    if (*argv == NULL) {
	if (strip(A_OUT) != 0) {
		status++;
	}
    } else {
	while (*argv != NULL) {
		if (strip(*argv) != 0) {
			status++;
		}
		argv++;
	}
    }
    exit(status);
}

int
strip(file)
char *file;
{
    int fd, new_fd;

    if ((fd = read_header(file)) < 0) {
	return(1);
    }

    new_fd = make_tmp(new_file);
    if (new_fd == -1) {
	fprintf(stderr, "can't create temporary file\n");
	close(fd);
	return(1);
    }
    if (write_header(new_fd)) {
	fprintf(stderr, "%s: can't write temporary file\n");
	unlink(new_file);
	close(fd);
	close(new_fd);
	return(1);
    }
    if (copy_file(fd, new_fd)) {
	fprintf(stderr, "can't copy %s\n", file);
	unlink(new_file);
	close(fd);
	close(new_fd);
	return(1);
    }

    close(fd);
    close(new_fd);
    if (unlink(file) == -1) {
	fprintf(stderr, "can't unlink %s\n", file);
	unlink(new_file);
	return(1);
    }
    link(new_file, file);
    unlink(new_file);
    return(0);
}

static int convert_proc_desc();

read_header(file)
char *file;
{
    int fd;

    fd = open(file, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "can't open %s\n", file);
	return(-1);
    }
    if (convert_proc_desc(file) != 0) {
	close(fd);
	return(-1);
    }
    return(fd);
}


#define NSEG_NO_SYM	4	/* text + daya + bss + stack */
#define NSEG_SYM	5       /* text + data + bss + stack + symtab */

#define NTHREAD 1
#define PDSIZE(nseg)	(sizeof(process_d) + (nseg)*sizeof(segment_d) + \
			 NTHREAD*(sizeof(thread_d) + sizeof(thread_idle)))

/* The buffer that will contain process descriptor of stripped binary: */
static union proc_info {
    char        pdbuf[PDSIZE(NSEG_NO_SYM)];
    process_d   pd;
    char        padding[1024];	/* do we really need this? */
} new_pd;


static int
convert_proc_desc(file)
char *file;
/* 
 * If the process descriptor in file contains a symbol table segment,
 * convert it to one without it and return 0.
 * If somethings goes wrong, return 1.
 */
{
    capability	filecap;
    errstat	status;
    process_d   *proc_desc = NULL;
    int		seg, n_sym_tab;
    segment_d	*sd;
    thread_d	*td_src, *td_dest;
    thread_idle *tdi_src, *tdi_dest;

    /* when something goes wrong, call failure in order to clean up
     * and return properly */
#   define Failure()	{ goto failure; }

    if ((status = name_lookup(file, &filecap)) != STD_OK) {
        fprintf(stderr, "cannot lookup %s (%s)\n", file, err_why(status));
	Failure();
    }
    
    if ((proc_desc = pd_read(&filecap)) == NULL) {
	fprintf(stderr, "%s: no process descriptor\n", file);
	Failure();
    }

    /* Amoeba binaries (created by either ainstall or acv) currently have
     * their symbol table in the 5th and last segment (if it exists).
     * Moreover, the text of this symtab is at the end of an Amoeba binary.
     *
     * This assumption makes stripping the symbol table off a binary
     * relatively easy, because we can just omit the last segment,
     * without having to change the offsets of the other segments
     * in the process descriptor.
     */

    if (proc_desc->pd_nseg != NSEG_SYM) {
	fprintf(stderr, "%s: has no symbol table\n", file);
	Failure();
    }

    if (pd_size(proc_desc) != PDSIZE(NSEG_SYM)) { /* yet another sanity check*/
	fprintf(stderr, "%s: unexpected pd size (%d instead of %d)\n",
		file, pd_size(proc_desc), PDSIZE(NSEG_SYM));
	Failure();
    }

#if defined(SYSV) || defined(AMOEBA)
    memset(new_pd.pdbuf, 0, sizeof new_pd.pdbuf);
#else
    bzero(new_pd.pdbuf, sizeof new_pd.pdbuf); /* Zero process buffer */
#endif

    /* convert process descriptor to one without symbol table segment */
    (void) strncpy(new_pd.pd.pd_magic, proc_desc->pd_magic, ARCHSIZE);
    new_pd.pd.pd_nseg = NSEG_NO_SYM;
    new_pd.pd.pd_nthread = NTHREAD;
  
    n_sym_tab = 0;
    sd = &PD_SD(&new_pd.pd)[0];
    for (seg = 0; seg < (int) proc_desc->pd_nseg; seg++) {
	if (((&PD_SD(proc_desc)[seg])->sd_type & MAP_SYMTAB) == 0) {
	    /* non symbol table segment: copy it */
	    *sd++ = PD_SD(proc_desc)[seg];
	} else {
	    n_sym_tab++;
	}
    }

    if (n_sym_tab != 1) {
	if (n_sym_tab == 0) {
	    fprintf(stderr, "%s doesn't have a symbol table\n", file);
	} else {
	    fprintf(stderr, "%s has %d(?!) symbol tables\n", file, n_sym_tab);
	}
	Failure();
    }

    /* copy thread descriptor */
    td_src = PD_TD(proc_desc);
    td_dest = PD_TD(&new_pd.pd);
    *td_dest = *td_src;

    tdi_src = (thread_idle *)(td_src + 1);
    tdi_dest = (thread_idle *)(td_dest + 1);
    *tdi_dest = *tdi_src;

    free((char *)proc_desc);
    return(0);

failure:
    if (proc_desc != NULL) free((char *)proc_desc);
    return(1);
}

#define MAX_PD_SIZE	1024

write_header(fd)
int fd;
/* write the Amoeba process descriptor followed by the a.out header */
{
    char proc_buf[MAX_PD_SIZE];
    char *bufp;

    (void) lseek(fd, 0L, SEEK_SET);
    if ((bufp = buf_put_pd(proc_buf, &proc_buf[MAX_PD_SIZE], &new_pd.pd))==0){
	return(1);
    }
    if (write(fd, proc_buf, bufp - proc_buf) != bufp - proc_buf) {
	return(1);
    }
    return(0);
}

int
fcopy(ifd, off, siz, ofd)
int ifd, ofd;
long off, siz;
{
#define COPYSIZ 8192
    char buf[COPYSIZ];
    int writesiz;

    if (lseek(ifd, off, 0) != off || lseek(ofd, off, 0) != off) {
	fprintf(stderr, "seek failed\n");
	return(1);
    }

    while (siz > 0) {
	writesiz = (siz >= COPYSIZ) ? COPYSIZ : siz;

	if (read(ifd, buf, writesiz) != writesiz ||
	    write(ofd, buf, writesiz) != writesiz) {
	    fprintf(stderr, "I/O error\n");
	    return(1);
	}
	siz -= writesiz;
    }

    return(0);
}

int
copy_file(fdin, fdout)
int fdin, fdout;
/* Copy segments present in file (i.e. with offset > 0) from fdin to fdout. */
{
    segment_d *sd;
    int seg;

    for (seg = 0; seg < (int) new_pd.pd.pd_nseg; seg++) {
	sd = &PD_SD(&new_pd.pd)[seg];
	if (sd->sd_offset > 0) {
	    if (fcopy(fdin, sd->sd_offset, sd->sd_len, fdout) != 0) {
		return(1);
	    }
	}
    }
    return(0);
}

int
make_tmp(new_name)
char *new_name;
{
    char *nameptr, *tempnam();

    if ((nameptr = tempnam("/tmp", "strip")) == 0) {
	return(-1);
    } else {
	(void) strcpy(new_name, nameptr);
	free(nameptr);
    }
    return(creat(new_name, 0777));
}

