/*	@(#)dirmarshall.c	1.5	96/02/27 10:21:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "module/buffers.h"

#ifndef  MAKESUPER
#include "global.h"
#include "sp_impl.h"
#include "main.h"
#include "caching.h"
#endif
#include "dirmarshall.h"

/*
 * Flatten the row sr with ncols columns into p (bounded by e).
 */
char *
sp_buf_put_row(p, e, optim, sr, ncols)
char          *p;
char          *e;
int            optim; /* optimise writing the type cap */
struct sp_row *sr;
int            ncols;
{
    register int i;
    
    p = buf_put_string(p, e, sr->sr_name);
    if (optim) {
	if (NULLPORT(&sr->sr_typecap.cap_port)) {
	    /* add marker indicating a null type cap: */
	    p = buf_put_int16(p, e, (int16) 0);
	} else {
	    p = buf_put_int16(p, e, (int16) 1);	/* marker */
	    p = buf_put_cap(p, e, &sr->sr_typecap);
	}
    } else {
	p = buf_put_cap(p, e, &sr->sr_typecap);
    }
    p = buf_put_int32(p, e, (int32) sr->sr_time);
    p = buf_put_capset(p, e, &sr->sr_capset);
    for (i = 0; i < ncols; i++) {
	p = buf_put_right_bits(p, e, sr->sr_columns[i]);
    }
    return p;
}

char *
buf_put_row(p, e, sr, ncols)
char *p, *e;
struct sp_row *sr;
int ncols;
{
    return sp_buf_put_row(p, e, 0, sr, ncols);
}

#ifdef SOAP_DIR_SEQNR

char *
buf_put_seqno(buf, end, seqno)
char *buf;
char *end;
sp_seqno_t *seqno;
{
    char *p = buf;

    p = buf_put_uint32(p, end, seqno->seq[0]);
    p = buf_put_uint32(p, end, seqno->seq[1]);
    return p;
}

char *
buf_get_seqno(buf, end, seqno)
char *buf;
char *end;
sp_seqno_t *seqno;
{
    char *p = buf;

    p = buf_get_uint32(p, end, &seqno->seq[0]);
    p = buf_get_uint32(p, end, &seqno->seq[1]);
    return p;
}

#endif /* SOAP_DIR_SEQNR */

/*
 * Put the header and the first n rows of a directory into a char buffer,
 * and set *nrows_p to point to the place where the number of rows was stored:
 */
char *
buf_put_dir_n(p, e, sd, n, nrows_p)
char *p, *e;
struct sp_dir *sd;
int n;
char **nrows_p;
{
    register int i;
    
    if (n < 0 || n > sd->sd_nrows) {
	return 0;
    }
#ifdef SOAP_DIR_SEQNR
    /* The magic number is put twice in the buffer so that we always can
     * see later on whether we encountered an old or a new-style directory
     * file (the old version starts with the check field, which could
     * start with the same bytes as SP_DIR_MAGIC).
     */
    p = buf_put_int32(p, e, (int32) SP_DIR_MAGIC);
    p = buf_put_int32(p, e, (int32) SP_DIR_MAGIC);
#endif    
    p = buf_put_port(p, e, &sd->sd_random);
    p = buf_put_int16(p, e, (int16) sd->sd_ncolumns);
    *nrows_p = p;
    p = buf_put_int16(p, e, (int16) sd->sd_nrows);
#ifdef SOAP_DIR_SEQNR
    /* We put the seqno right after sd_nrows (i.s.o right after the magic
     * numbers) so that we can keep using the efficient bullet file editing
     * operations in dirfile.c, without too many changes.
     */
    p = buf_put_seqno(p, e, &sd->sd_seq_no);
#endif
    for (i = 0; i < sd->sd_ncolumns; i++) {
	p = buf_put_string(p, e, sd->sd_colnames[i]);
    }
    for (i = 0; i < n; i++) {
	p = buf_put_row(p, e, sd->sd_rows[i], sd->sd_ncolumns);
    }
    return p;
}

/*
 * Put a directory into a char buffer:
 */
char *
buf_put_dir(p, e, sd)
char *p, *e;
struct sp_dir *sd;
{
    char *ignore;

    return buf_put_dir_n(p, e, sd, sd->sd_nrows, &ignore);
}

#ifndef MAKESUPER

/*
 * From sp_impl.c:
 */
extern char *sp_str_copy();
extern char *sp_buf_get_capset();

#define Failure(err)   { failure_err = err; goto failure; }

/*
 * Get a flattened row pointed to by p into sr.  ncols is the number of
 * columns that the row is supposed to have.
 */
char *
sp_buf_get_row(p, e, optim, sr, ncols, err_p)
char 	      *p;
char	      *e;
int	       optim;
struct sp_row *sr;
int 	       ncols;
errstat       *err_p;
{
    int     i;
    char   *name;
    int32   val32;
    errstat failure_err;
    
    if ((p = buf_get_string(p, e, &name)) == NULL) {
	Failure(STD_SYSERR);
    }
    if ((sr->sr_name = sp_str_copy(name)) == NULL) {
	Failure(STD_NOMEM);
    }

    if (optim) {
	int16 marker;
	
	p = buf_get_int16(p, e, &marker);
	if (p == NULL) {
	    Failure(STD_SYSERR);
	}

	if (marker == 0) {
	    static capability NULLCAP;

	    sr->sr_typecap = NULLCAP;
	} else {
	    p = buf_get_cap(p, e, &sr->sr_typecap);
	}
    } else {
	p = buf_get_cap(p, e, &sr->sr_typecap);
    }

    if ((p = buf_get_int32(p, e, &val32)) != NULL) {
	sr->sr_time = val32;
	if (sr->sr_time < (1988-1970) * (365*24*60*60)) {
	    /* Assume this directory entry was created by an old version
	     * of the server, which kept the time in minutes (rather than
	     * seconds) since 1970.
	     */
	    sr->sr_time *= 60;
	}
    }
    if ((p = sp_buf_get_capset(p, e, &sr->sr_capset)) == NULL) {
	/* It could be due to missing data or could be memory alloc
	 * failure.  It's safer to assume memory failure, so that the
	 * file will not be destroyed:
	 */
	Failure(STD_NOMEM);
    }
    for (i = 0; i < ncols; i++) {
	if ((p = buf_get_right_bits(p, e, &sr->sr_columns[i])) == NULL) {
	    Failure(STD_SYSERR);
	}
    }

    /* success: */
    *err_p = STD_OK;
    return p;

failure:
    *err_p = failure_err;
    return NULL;
}

char *
buf_get_row(p, e, sr, ncols, err_p)
char 	       *p;
char	       *e;
struct sp_row *sr;
int 	       ncols;
errstat       *err_p;
{
    return sp_buf_get_row(p, e, 0, sr, ncols, err_p);
}

/*
 * Construct a newly-malloc'd directory data structure using the data in the
 * buffer whose endpoints are p and e (p < e).  Returns pointer to rest of
 * buffer on success, NULL for fatal or non-fatal error.  Sets
 * *psd to point at the newly-malloc'd directory data structure, on
 * success or non-fatal error.  Sets it to NULL on fatal error.  A non-fatal
 * error implies that *psd points to a self-consistent directory,
 * although it may not have exactly the contents it should.
 */
char *
buf_get_dir(buf, e, psd, err_p)
char 	       *buf;
char	       *e;
struct sp_dir **psd;
errstat        *err_p;
{
    extern struct sp_dir *alloc_dir();
    struct sp_dir *sd = NULL;
    int16   nrows, ncolumns;
    char   *p;
    int     col, row;
    errstat failure_err;
    
    if ((sd = alloc_dir()) == NULL) {
	Failure(STD_NOMEM);
    }
    p = buf;
#ifdef SOAP_DIR_SEQNR
    /* also support old-style directory files */
    {
	int32 magic1, magic2;

	p = buf_get_int32(p, e, &magic1);
	p = buf_get_int32(p, e, &magic2);
	if (p != NULL && magic1 == SP_DIR_MAGIC && magic2 == SP_DIR_MAGIC) {
	    sd->sd_old_format = 0;
	} else {
	    /* message("recover from old-fashioned directory format"); */
	    sd->sd_old_format = 1; /* needs rewrite in new format */
	    p = buf; /* start unmarshalling at the beginning again */
	}
    }
#endif
    p = buf_get_port(p, e, &sd->sd_random);
    p = buf_get_int16(p, e, &ncolumns);
    p = buf_get_int16(p, e, &nrows);
#ifdef SOAP_DIR_SEQNR
    if (sd->sd_old_format) {
	/* fake a new-style directory with sequence number [0, 1] */
	sd->sd_seq_no.seq[0] = 0;
	sd->sd_seq_no.seq[1] = 1;
    } else {
	p = buf_get_seqno(p, e, &sd->sd_seq_no);
    }
#endif
    
    /* NOTE: 5000 is not a real limitation, just a sanity check: */
    if (p == NULL ||
	(ncolumns <= 0 || ncolumns > SP_MAXCOLUMNS) ||
	(nrows < 0 || nrows > 5000))
    {
	Failure(STD_SYSERR);
    }

    sd->sd_ncolumns = ncolumns;
    sd->sd_nrows = nrows;
    
    if (sd->sd_nrows > 0) {
        /* Uncache enough directories to make room for the rows in this one.
         * Note that due to malloc() fragmentation, this is not really
         * guaranteed to be enough, but it is a start, at least.
         */
        make_room(sd->sd_nrows);

	sd->sd_rows = (struct sp_row **) alloc(sizeof(struct sp_row *),
					    sp_rows_to_allocate(sd->sd_nrows));
	if (sd->sd_rows == NULL) {
	    Failure(STD_NOMEM);
	}
    }

    sd->sd_colnames = (char **)alloc(sizeof(char *), sd->sd_ncolumns);
    if (sd->sd_colnames == NULL) {
	Failure(STD_NOMEM);
    }
    for (col = 0; col < sd->sd_ncolumns; col++) {
	char *name;

	if ((p = buf_get_string(p, e, &name)) == NULL) {
	    Failure(STD_SYSERR);
	}
	if ((sd->sd_colnames[col] = sp_str_copy(name)) == NULL) {
	    Failure(STD_NOMEM);
	}
    }
    
    /* At this point, we are committed to returning a self-consistent
     * directory, regardless of problems getting the rows:
     */
    *err_p = STD_OK;
    for (row = 0; row < sd->sd_nrows; row++) {
	if ((sd->sd_rows[row] = (sp_row *) alloc(sizeof(sp_row), 1)) == NULL) {
	    sd->sd_nrows = row;
	    break;
	}

	p = buf_get_row(p, e, sd->sd_rows[row], sd->sd_ncolumns, err_p);
	if (p == NULL) {
	    /* Not enough rows in file, or not enough memory to
	     * read them all; return partial directory:
	     */
	    free((_VOIDSTAR) sd->sd_rows[row]);
	    sd->sd_rows[row] = NULL;
	    sd->sd_nrows = row;
	    break;
	}
    }
    *psd = sd;
    sp_update_nrows(sd->sd_nrows);
    return p;

failure:
    if (sd != NULL) {
	if (sd->sd_rows != NULL) {
	    /* First adminstrate that we have allocated rows.
	     * (free_dir() undoes this again..)
	     */
	    sp_update_nrows(sd->sd_nrows);
	}
	free_dir(sd);
    }
    *psd = NULL;
    *err_p = failure_err;
    return NULL;
}

#endif /* !MAKESUPER */
