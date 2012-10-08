/*	@(#)handle.c	1.4	96/02/27 13:07:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Dump a process descriptor.
   This is done by piping the process descriptor to the dump program,
   named pdump.
   The return value must be zero if the process is to be terminated. */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "exception.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "module/proc.h"
#include "ampolicy.h"

#define	_POSIX2_SOURCE
#define _XOPEN_SOURCE  /* For the SUNWspro compiler :-( */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define SIG_NODUMP	2
#define	DUMPCMD		"pdump -"

extern char *	progname;	/* From main program */
extern char *	dumpfile;	/* Ditto */


static void
dump_core(pdbuf, pdlen)
char *	pdbuf;	/* buffer containing network byteorder process descriptor */
int	pdlen;
{
    capability 	cap;
    capability	dump_cap;
    unsigned	dumpfile_length;
    errstat    	err;
    int		i;
    process_d *	pd;
    char * 	tmp_dump;

    /* get bullet svr cap */
    if ((err = name_lookup(DEF_BULLETSVR, &cap)) != STD_OK) {
	    fprintf(stderr, "%s: %s: not found (%s)\n",
		    progname, DEF_BULLETSVR, err_why(err));
	    return;
    }

    /* unpack the buffer here */
    if ((pd = pdmalloc(pdbuf, pdbuf + pdlen)) == 0) {
	fprintf(stderr, "%s: dump_core: pd_malloc failed\n", progname);
	return;
    }

    if (buf_get_pd(pdbuf, pdbuf + pdlen, pd) == 0)
    {
	fprintf(stderr, "%s: dump_core: bad process descriptor\n",
							    progname);
	return;
    }

    /* save core */
    if ((err = pd_preserve(&cap, pd, pdlen, &dump_cap)) != STD_OK) {
	    fprintf(stderr, "%s: %s: Cannot preserve dumpfile (%s)\n",
		    progname, DEF_BULLETSVR, err_why(err));
	    return;
    }

    if (name_append(dumpfile, &dump_cap) == STD_OK) {
	    fprintf(stderr, "%s: %s dumped\n", progname, dumpfile);
	    return;
    }	

    /* copy name, reserve space for ".1" extension */
    dumpfile_length = (unsigned) strlen(dumpfile);
    if ((tmp_dump = (char *) malloc(dumpfile_length + 3)) == 0) {
	    fprintf(stderr, "%s: Cannot allocate memory\n", 
		    progname);
	    return;
    }
    strcpy(tmp_dump, dumpfile);
    dumpfile = tmp_dump;
    dumpfile[dumpfile_length]     = '.';
    dumpfile[dumpfile_length + 2] = '\0';

    /* store at soap - try several names */
    for (i = 1; i != 10; i++) {
	    dumpfile[dumpfile_length + 1] = '0' + i; /* try other name */
	    if ((err = name_append(dumpfile, &dump_cap)) == STD_OK)
		    break;
    }

    if (err == STD_OK)
	    fprintf(stderr, "%s: %s dumped\n", progname, dumpfile);
    else {
	    fprintf(stderr, "%s: Cannot append file %s (%s)\n",
		    progname, dumpfile, err_why(err));
	    if ((err = std_destroy(&dump_cap)) != STD_OK)
		    fprintf(stderr, "%s: Cannot destroy dump file (%s)\n",
			    progname, err_why(err));
    }
    free(dumpfile);
    dumpfile = 0;
}


/*
** The process descriptor (pd) is in a network byteorder buffer and must be
** unpacked with buf_get_pd before use.  The reason it is in this form is that
** pdump expects it that way and that is the most probable fate for the pd.
*/

int
handle(cause, detail, pd, pdlen)
int	cause;
long	detail;
char *	pd;
int	pdlen;
{
    FILE *	fp;
    int		sts;
    
    switch (cause) {
    case TERM_NORMAL:
	fprintf(stderr, "%s: exit status %ld\n", progname, detail);
	break;
    case TERM_STUNNED:
	fprintf(stderr, "%s: stun code %ld\n", progname, detail);
	break;
    case TERM_EXCEPTION:
	fprintf(stderr, "%s: exception %ld: %s\n",
				progname, detail, exc_name((signum) detail));
	break;
    default:
	fprintf(stderr, "%s: termination cause %d, detail %ld\n",
						    progname, cause, detail);
	break;
    }
    if (pdlen == 0)
	return 0;

    if (dumpfile)
	dump_core(pd, pdlen);

    if ((fp = popen(DUMPCMD, "w")) == NULL)
	fprintf(stderr, "%s: can't open pipe to %s\n", progname, DUMPCMD);
    else {
	(void) fwrite(pd, sizeof (char), (unsigned) pdlen, fp);
	if ((sts = pclose(fp)) != 0)
	    fprintf(stderr, "%s: %s exit status %d\n",
						    progname, DUMPCMD, sts);
    }
    return 0;
}
