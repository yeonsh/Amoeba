/*	@(#)pd_read.c	1.6	96/02/27 11:03:32 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * process_d *pd_read(capability *filecap)
 *
 *	Read a process descriptor from a file, and convert it to host
 *	byte order.  (The pd was stored in network byte order by ainstall.)
 *	Returns pointer to allocated pd, or NULL if there was an error.
 *
 * errstat pd_read_multiple(capability *cap, process_d ***list, int *num)
 *
 *	More generic version of pd_read(): the capability can also be
 *	a directory containing several process descriptors, one per
 *	architecture.  The idea is that (with help from the run server)
 *	we run the binary for the architecture that is currently the
 *	most efficient.
 *	If successfull, the function returns STD_OK, and puts the list
 *	of pointers to allocated process descriptors and its length in the
 *	out parameters.  Otherwise it returns a standard error code
 *	telling what went wrong somewhere along the way.
 *
 * Note: pd_read_multiple() patches the segment descriptors of the binaries
 * to contain the corresponding file capabilities, while pd_read() does not.
 */

#include <stdlib.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/proc.h"
#include "bullet/bullet.h"
#include "class/checkpoint.h"
#include "capset.h"
#include "soap/soap.h"
#include "sp_dir.h"

/*
 * DEF_PDSIZE is enough for the process descriptor of normal binaries,
 * having one thread and and 4 segments.
 * Even if a buffer of size DEF_PDSIZE isn't big enough,
 * at least it gives us a starting point to iteratively determine
 * how big it *should* be.
 */
#define DEF_NSEG	4
#define DEF_NTHREAD	1
#define	DEF_PDSIZE	(sizeof(process_d) + \
			 DEF_NSEG * sizeof(segment_d) + \
			 DEF_NTHREAD * MAX_THREAD_SIZE)

static errstat
pd_do_read(cap, pd_ret)
capability  *cap;	/* In: capability of file holding pd */
process_d  **pd_ret;	/* Out: converted pd, when succesful */
{
    char       *buf = NULL,	/* for the byteorder independent version */
    	       *new_buf,
    	       *bufend;
    process_d  *pd = NULL,	/* pointer to a pd in our byteorder */
    	       *new_pd;
    size_t	cur_size = DEF_PDSIZE;
    errstat	err;
#   define 	Failure(e)	{ err = e; goto Exit; /* free allocated mem*/ }
    b_fsize	nread;		/* number of bytes actually read */

    /* allocate some initial size that will be enough for "normal" binaries */
    buf = (char *) malloc(cur_size);
    pd = (process_d *) malloc(cur_size);
    if (buf == NULL || pd == NULL) {
	Failure(STD_NOSPACE);
    }

    for (;;) {
	err = b_read(cap, (b_fsize) 0, buf, (b_fsize) cur_size, &nread);
	if (err != STD_OK) {
	    Failure(err);
	} else if (nread < sizeof(process_d)) {
	    Failure(STD_ARGBAD);
	}

	/* Let's see if we got enough of the file */
	if ((bufend = buf_get_pd(buf, buf + cur_size, pd)) != NULL) {
	    break;	    /* Succeeded! */
	}

	/* For valid pd's, the buffer should start with a known
	 * architecture identifier.  This filters out the common case
	 * where a shell script was presented instead of a binary.
	 */
	if (!pd_known_arch(buf)) {
	    Failure(STD_ARGBAD);
	}
	
	if (cur_size > MAX_PDSIZE) {
	    /* Process descriptor also not created with some checkpoint
	     * transaction, so it's unlikely to be a valid one.
	     */
	    Failure(STD_ARGBAD);
	}

	/* For next iteration double the size we read */
	cur_size *= 2;
	if ((new_buf = (char *)realloc(buf, cur_size)) != NULL) {
	    buf = new_buf;
	} else {
	    Failure(STD_NOSPACE);
	}
	if ((new_pd = (process_d *)realloc(pd, cur_size)) != NULL) {
	    pd = new_pd;
	} else {
	    Failure(STD_NOSPACE);
	}
    }

    /* Now we know the real size, and the pd is totally in the buffer.
     * Shrink it to the size needed.
     */
    cur_size = bufend - buf;
    if ((new_pd = (process_d *)realloc(pd, cur_size)) != NULL) {
	pd = new_pd;
    } else { /* should not occur, as we tried to shrink it */
	Failure(STD_NOSPACE);
    }
    
    free(buf);
    *pd_ret = pd;
    return STD_OK;

 Exit:
    if (buf != NULL) free(buf);
    if (pd != NULL) free(pd);
    return err;
}

process_d *
pd_read(cap)
capability *cap;
{
    process_d *pd;

    if (pd_do_read(cap, &pd) == STD_OK) {
	return pd;
    } else {
	return NULL;
    }
}

/*
 * pd_patch()
 *
 * Loop over the segment descriptors, patching those that
 * must point into the executable file.
 * They are recognizable by a nonzero offset and a zero port.
 */

static void
pd_patch(prog, pd)
capability *	prog;
process_d *	pd;
{
    uint16	i;
    segment_d *	sd;
	
    for (i = 0, sd = PD_SD(pd); i < pd->pd_nseg; i++, sd++)
	if (sd->sd_offset != 0 && NULLPORT(&sd->sd_cap.cap_port))
	    sd->sd_cap = *prog;
}

/*
 * architectures (going to be) supported by Amoeba
 */
struct arch_info {
    char *arch_name;	/* name of the architecture */
    char *arch_pd_name;	/* name of a process descriptor for that architecture*/
};

#define NARCHS	6

static struct arch_info arch_list[NARCHS] = {
    { "i80386",  "pd.i80386" },
    { "mc68000", "pd.mc68000"},
    { "mipsel",  "pd.mipsel" },
    { "mipseb",  "pd.mipseb" },
    { "sparc",   "pd.sparc"  },
    { "vax",	 "pd.vax"    }
};

int
pd_known_arch(arch)
char *arch;
{
    int i;

    /* look if it's the identifier for a known architecture */
    for (i = 0; i < NARCHS; i++) {
        if (strcmp(arch, arch_list[i].arch_name) == 0) {
	    return 1;
        }
    }

    /* not found */
    return 0;
}

static errstat
add_to_list(pd_list, pd_num, pd, filecap)
process_d ***pd_list;	/* pointer to array of process descriptor pointers */
int 	    *pd_num;	/* number of process descriptors */
process_d   *pd;
capability  *filecap;
/*
 * Adds process descriptor `pd' to `pd_list', whose current size is `*pd_num'.
 * Furthermore, the segment descriptors of `pd' are patched to refer to
 * `filecap'.
 */
{
    size_t alloc_size = (*pd_num + 1) * sizeof(process_d *);
    register process_d **newlist;

    if (*pd_list == NULL) {
	newlist = (process_d **)malloc(alloc_size);
    } else {
	newlist = (process_d **)realloc(*pd_list, alloc_size);
    }
    if (newlist == NULL) {
	return STD_NOSPACE;
    }

    pd_patch(filecap, pd);
    newlist[*pd_num] = pd;
    *pd_list = newlist;
    *pd_num += 1;
    return STD_OK;
}


errstat
pd_read_multiple(cap, pd_array, pd_num)
capability  *cap;	/* IN:  cap of binary or directory with binaries */
process_d ***pd_array;	/* OUT: ptr to array of process descriptors ptrs */
int 	    *pd_num;	/* OUT: number of process descriptor ptrs in array */
/*
 * Used by exec_file() to read a single process descriptor from a file
 * or a set of process descriptors from a directory.
 */
{
    errstat	err, retval;
    process_d  *pd;
    int		num = 0;
    process_d **array = NULL;
    
    if ((err = pd_do_read(cap, &pd)) == STD_OK) {
	/* Single binary file */
	retval = add_to_list(&array, &num, pd, cap);
    } else if (err == STD_COMBAD) {
	/* Assume it is a directory of binaries; add valid pd's in it. */
	sp_entry    pd_entries[NARCHS];
	sp_result   pd_result[NARCHS];
	capset      cset;
	int	    arch;

	if (cs_singleton(&cset, cap) == 0) {
	    return STD_NOSPACE;
	}

	/* Fetch the binaries for all possible architectures in a single (!)
	 * Soap request.  Note that generally only a few will be present,
	 * but that information will be reported for each entry seperately.
	 */
	for (arch = 0; arch < NARCHS; arch++) {
	    pd_entries[arch].se_name = arch_list[arch].arch_pd_name;
	    pd_entries[arch].se_capset = cset;
	}

	if ((err = sp_setlookup(NARCHS, pd_entries, pd_result)) != STD_OK) {
	    cs_free(&cset);
	    return STD_ARGBAD; /* err? */
	}

	/* Failure value if we cannot find a valid process descriptor
	 * in this directory is STD_NOTFOUND.  If a path searching execXX()
	 * function is used, it should then proceed along $PATH.
	 */
	retval = STD_NOTFOUND;

	/* For each architecture that was present, try to add a procdesc */
	for (arch = 0; arch < NARCHS; arch++) {
	    err = ERR_CONVERT(pd_result[arch].sr_status);

	    if (err == STD_OK) {
		capset *file_cs = &pd_result[arch].sr_capset;
		int     suite_nr;

		/* Add the first available replica */
		for (suite_nr = 0; suite_nr < file_cs->cs_final; suite_nr++) {
		    suite *s = &file_cs->cs_suite[suite_nr];

		    if (!s->s_current) {
			continue;
		    }

		    err = pd_do_read(&s->s_object, &pd);
		    if (err == STD_OK) {
			/* Arch. of the binary should match its name */
			if (strcmp(arch_list[arch].arch_name, pd_arch(pd))) {
			    err = STD_SYSERR;
			}
		    }

		    if (err == STD_OK) {
			/* Add it to the list */
			err = add_to_list(&array, &num, pd, &s->s_object);
			if (err == STD_OK) {
			    retval = STD_OK;
			}

			/* We read a valid replica for this arch, so: */
			break;
		    }
		}

		cs_free(file_cs);
	    }
	}

	cs_free(&cset);
    } else {
	/* neither a binary nor a pd directory */
	retval = STD_ARGBAD;
    }

    if (retval == STD_OK) {
	*pd_array = array;
	*pd_num = num;
    }
    return retval;
}

