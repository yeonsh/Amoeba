/*	@(#)dirfile.c	1.4	96/02/27 10:21:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <assert.h>
#define  _POSIX_SOURCE
#include <limits.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "bullet/bullet.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "module/buffers.h"
#include "module/stdcmd.h"
#include "module/mutex.h"
#include "module/ar.h"

#include "global.h"
#include "main.h"
#include "caching.h"	/* for make_room() */
#include "filesvr.h"
#include "misc.h"
#include "sp_impl.h"
#include "dirmarshall.h"
#include "dirfile.h"


#define BULLET_COMMIT_FLAGS	(BS_COMMIT | BS_SAFETY)

/* this one is static, but it seems to be used all over the place */
static capability NULLCAP;

static char IO_buf[MAX_DIR_BYTES + IOBUF_EXTRA];
/* A static buffer big enough to contain a flattened version of the biggest
 * directory allowed.  It should only be accessed by means of the following
 * two routines taking care of locking.
 */
static mutex buf_lock;

char *
sp_get_iobuf()
{
    mu_lock(&buf_lock);
    return IO_buf;
}

void
sp_put_iobuf(buf)
char *buf;
{
    assert(buf == IO_buf);
    mu_unlock(&buf_lock);
}
 
/*
 * Perform the specified Bullet operation, trying first the preferred Bullet
 * server, then the other.  If a Bullet server does not respond (RPC_NOTFOUND)
 * update the global record of available Bullet servers and indicate that the
 * bad one is not preferred, so that it will not be tried first.  On sucess,
 * exactly one of the newcaps has been filled in, and the other is cleared to
 * a NULLCAP.  ALso on success, the index of the one that got filled in is
 * returned, otherwise, -1 is returned.
 *
 * The arguments are the same as for b_modify/b_insert, except that the first
 * argument is a Bullet transaction command code (e.g. BS_CREATE), indicating
 * which bullet function to call, and the two cap arguments are pointers to
 * arrays of caps (size 2; one for each of the two Bullet servers), instead
 * of just pointers to a single cap.  The buf arg is ignored, of course, for
 * b_delete, as is the offset arg, for b_create.
 * 
 * Once a Bullet server has been marked unavailable, subsequent calls to
 * this function will no longer try to use it, to avoid repeated timeouts.
 * Only the watchdog function can set it back to available.
 */
static errstat
bullet_op(b_command, oldcaps, ncaps, offset, buf, size, commit, newcaps, index)
int	    b_command;	/* in: which Bullet function to call */
capability *oldcaps;	/* in: caps for existing file(s)     */
int	    ncaps;	/* in: number of them 		     */
b_fsize     offset;	/* in: where to make the change      */
char	   *buf;	/* in: new data to modify the file   */
b_fsize     size;	/* in: #bytes data in buf	     */
int         commit;	/* in: the commit and safety flags   */
capability *newcaps;	/* out: new file caps (one valid, others NULL) */
int        *index;	/* out: which was modified if STD_OK */
{
    errstat err;
    char *func_name;
    int   i, which;
    int   nc;
    int   order[NBULLETS];
    
    err = STD_SYSERR;
    fsvr_order_caps(oldcaps, ncaps, order);
    for (i = 0; i < ncaps && (which = order[i]) >= 0; i++) {
	assert(!NULLPORT(&oldcaps[which].cap_port));

	switch (b_command) {
	case BS_CREATE:
	    func_name = "b_create";
	    err = b_create(&oldcaps[which], buf, size, commit,
			   &newcaps[which]);
	    break;
	case BS_MODIFY:
	    func_name = "b_modify";
	    err = b_modify(&oldcaps[which], offset, buf, size, commit,
			   &newcaps[which]);
	    break;
	case BS_INSERT:
	    func_name = "b_insert";
	    err = b_insert(&oldcaps[which], offset, buf, size, commit,
			   &newcaps[which]);
	    break;
	case BS_DELETE:
	    func_name = "b_delete";
	    err = b_delete(&oldcaps[which], offset, size, commit,
			   &newcaps[which]);
	    break;
	}

	switch (err) {
	case STD_OK:
	    /* set others to NULLCAP */
	    for (nc = 0; nc < ncaps; nc++) {
		if (nc != which) {
		    newcaps[nc] = NULLCAP;
		}
	    }
	    *index = which;
	    return STD_OK;

	case RPC_NOTFOUND:
	    fsvr_set_unavail(&oldcaps[which].cap_port);
	    break;

	case STD_CAPBAD:
	    /* If invalid, it is gone forever, so: */
	    newcaps[which] = oldcaps[which] = NULLCAP;
	    /* fall through */
	default:
	    message("%s Bullet%d failed (%s)", func_name, which, err_why(err));
	    break;
	}
    }

    /* Unsuccessful */
    return err;
}

/* Save the directory sd at one of the file servers.  The capability of the
 * bullet file is returned in the corresponding entry in filecaps.  The other
 * ones are set to null.
 */
errstat
dirf_write(obj, nfilecaps, filecaps, fsvr_caps)
objnum     obj;
int 	   nfilecaps;
capability filecaps[];
capability fsvr_caps[];
{
    sp_tab *st;
    int     cap_index;
    char   *p, *iobuf;
    errstat err;

    MON_EVENT("create Bullet directory file");
    assert(!bad_objnum(obj));
    st = &_sp_table[obj];
    assert(st->st_dir != NULL);

    iobuf = sp_get_iobuf();
    if ((p = buf_put_dir(iobuf, iobuf + MAX_DIR_BYTES, st->st_dir)) == NULL) {
	/* directory too big ?? */
	MON_EVENT("dirf_write: buf_put_dir failed");
	message("dirf_write: buf_put_dir failed");
	err = STD_NOSPACE;
    } else {
	err = bullet_op(BS_CREATE,
			fsvr_caps, nfilecaps,
			(b_fsize) 0, iobuf, (b_fsize) (p - iobuf),
			BULLET_COMMIT_FLAGS, filecaps, &cap_index);
    }
    sp_put_iobuf(iobuf);

    return err;
}

/* Get file offset of row affected by pending modification and also offset of
 * where sd->sd_nrows is stored in the file.  Filecaps is array of 2
 * (replicated) capabilities for the existing file, which is soon to be
 * modified according to st->st_dir->sd_mods.
 */
static b_fsize
dirf_offset(st, filecaps, nfilecaps, nrows_offset_p)
sp_tab		*st;
capability	*filecaps;
int		 nfilecaps;
b_fsize		*nrows_offset_p;
{
    /* Don't forget to unlock the IO-buffer on failure: */
#   define Failure()	{ goto failure; }
    struct sp_save *ss;
    b_fsize offset = 0;
    char   *p, *nrows_p;
    char   *iobuf = NULL;

    assert(st->st_dir != NULL);
    ss = st->st_dir->sd_mods;    
    *nrows_offset_p = 0;

    iobuf = sp_get_iobuf();

    if (ss->ss_command != SP_APPEND) {
	p = buf_put_dir_n(iobuf, iobuf + MAX_DIR_BYTES, st->st_dir,
			  ss->ss_rownum, &nrows_p);
	if (p == NULL || nrows_p == 0) {
	    Failure();
	}
	offset = (b_fsize)(p - iobuf);
    } else {
	/* For SP_APPEND, the offset to the new row is
	 * just the size of the old file, so we use b_size:
	 */
	int     i, which;
	int     order[NBULLETS];
	errstat err = STD_NOTFOUND;
    
	fsvr_order_caps(filecaps, nfilecaps, order);
	for (i = 0; i < nfilecaps && (which = order[i]) >= 0; i++) {
	    assert(!NULLPORT(&filecaps[which].cap_port));

	    if ((err = b_size(&filecaps[which], &offset)) == STD_OK) {
		break;
	    }
	}

	if (err != STD_OK) {	/* none of the bsize()s worked */
	    Failure();
	}
	
	/* Get offset where sd->sd_nrows is stored: */
	p = buf_put_dir_n(iobuf, iobuf + MAX_DIR_BYTES, st->st_dir,
			  0, &nrows_p);
	if (p == NULL || nrows_p == NULL) {
	    Failure();
	}
    }

    sp_put_iobuf(iobuf);

    *nrows_offset_p = (b_fsize)(nrows_p - iobuf);
    return offset;

failure:
    if (iobuf != NULL) {
	sp_put_iobuf(iobuf);
    }
    return 0;
}

static errstat
update_nrows_and_seqno(sd, nrows_offset, cap)
struct sp_dir *sd;
b_fsize        nrows_offset;
capability    *cap;
{
    /* Update the nrows and seqno (if any) stored in the file and commit */
    char buf[12]; /* enough for nrows and seqno */
    char *p, *buf_end;

    p = buf;
    buf_end = &buf[sizeof(buf)];

    p = buf_put_int16(p, buf_end, (int16) sd->sd_nrows);
#ifdef SOAP_DIR_SEQNR
    /* The sequence number is right after nrows: */
    p = buf_put_seqno(p, buf_end, &sd->sd_seq_no);
#endif
    assert(p != NULL);
    return b_modify(cap, nrows_offset, buf, (b_fsize) (p - buf),
		   BULLET_COMMIT_FLAGS, cap);
}
    
/*
 * Update the Bullet file associated with specified directory, using the
 * information stored in the sp_table "st_dir->sd_mods" entry.
 * Sd_mods describes what type of change was done to the internal,
 * malloc'd dir.
 * We do the same modification to the Bullet file, producing a new
 * committed file, without disturbing the original file.
 * For large directories, this is done with a combination of b_modify,
 * b_insert and b_delete, which allows us to avoid transmitting the
 * entire contents of the new file from the SOAP server to the Bullet server.
 */

errstat
dirf_modify(obj, ncaps, oldcaps, newcaps, fsvr_caps)
objnum obj;
int ncaps;
capability *oldcaps;
capability *newcaps;
capability *fsvr_caps;
{
    errstat err;
    sp_tab *st;
    struct sp_dir *sd;
    struct sp_save *ss;
    int cap_index = -1;
    b_fsize offset = 0, nrows_offset = 0, oldrow_size;
    short last_row;
    char *p;
    char *iobuf, *iobuf_end;
    int commit_flags;
    
    assert(!bad_objnum(obj));
    st = &_sp_table[obj];
    sd = st->st_dir;
    assert(sd != NULL);
    ss = sd->sd_mods;
    
    /* If the directory is small, or
     * if there are no modifications recorded, or more than one,
     * or no old file exists, switch to less efficient method,
     * namely, write out the entire directory with b_create:
     */
    if ((sd->sd_nrows < 100)			/* small dir */
#ifdef SOAP_DIR_SEQNR
    ||  sd->sd_old_format			/* current dir has old format */
#endif
    ||  (ss == NULL)				/* no mods */
    ||  (ss->ss_next != NULL)			/* more than 1 mod */
    ||  (caps_zero(oldcaps, ncaps) == ncaps))	/* no old file */
    {
	err = dirf_write(obj, ncaps, newcaps, fsvr_caps);
#ifdef SOAP_DIR_SEQNR
	if (err == STD_OK) {
	    /* Register the format change */
	    sd->sd_old_format = 0;
	}
#endif
	return err;
    }
    
    /* Get file offset of affected row and the offset where number of rows
     * is stored:
     */
    offset = dirf_offset(st, oldcaps, ncaps, &nrows_offset);
    if (offset < 1 || nrows_offset < 1) {
	message("dirf_modify: dirf_offset failed");
	return dirf_write(obj, ncaps, newcaps, fsvr_caps);
    }

    MON_EVENT("modify Bullet directory file");
    
    iobuf = sp_get_iobuf();
    iobuf_end = iobuf + MAX_DIR_BYTES;

    /* assume marshalling error by default */
    err = STD_SYSERR;

    switch (ss->ss_command) {
    case SP_APPEND:
	/* The last row of the directory is the new one.  Just append
	 * it to the existing bullet file:
	 */
	last_row = sd->sd_nrows - 1;
	if ((p = buf_put_row(iobuf, iobuf_end, sd->sd_rows[last_row],
			     sd->sd_ncolumns)) == NULL)	{
	    break;
	}

	err = bullet_op(BS_MODIFY, oldcaps, ncaps, offset, iobuf,
			(b_fsize)(p - iobuf), 0, newcaps, &cap_index);
	if (err == STD_OK) {
#ifdef SOAP_DIR_SEQNR /* for the moment */
	    err = update_nrows_and_seqno(sd, nrows_offset,
					 &newcaps[cap_index]);
#else
	    /* Now update the nrows stored in the file and commit: */
	    (void) buf_put_int16(iobuf, iobuf_end, (int16) sd->sd_nrows);
	    err = b_modify(&newcaps[cap_index], nrows_offset,
			   iobuf, (b_fsize) sizeof(int16),
			   BULLET_COMMIT_FLAGS, &newcaps[cap_index]);
#endif
	}
	break;
	
    case SP_DELETE:
	/* Calculate size of the row that is to be deleted: */
	if ((p = buf_put_row(iobuf, iobuf_end, &ss->ss_oldrow,
			     sd->sd_ncolumns)) == NULL) {
	    break;
	}
	
	/* Delete the specified row from the file: */
	err = bullet_op(BS_DELETE, oldcaps, ncaps, offset, iobuf,
			(b_fsize)(p - iobuf), 0, newcaps, &cap_index);
	if (err == STD_OK) {
#ifdef SOAP_DIR_SEQNR /* for the moment */
	    err = update_nrows_and_seqno(sd, nrows_offset,
					 &newcaps[cap_index]);
#else
	    /* Now update the nrows stored in the file and commit: */
	    (void) buf_put_int16(iobuf, iobuf_end,
				 (int16) sd->sd_nrows);
	    err = b_modify(&newcaps[cap_index], nrows_offset,
			   iobuf, (b_fsize) sizeof(int16),
			   BULLET_COMMIT_FLAGS, &newcaps[cap_index]);
#endif
	}
	break;
	
    case SP_REPLACE:
	/* Calculate size of the old row that is to be replaced: */
	if ((p = buf_put_row(iobuf, iobuf_end, &ss->ss_oldrow,
			     sd->sd_ncolumns)) == NULL) {
	    break;
	}
	oldrow_size = (b_fsize)(p - iobuf);
	
	/* Calculate size of the new row and prepare buffer to insert
	 * it in file:
	 */
	if ((p = buf_put_row(iobuf, iobuf_end, sd->sd_rows[ss->ss_rownum],
			     sd->sd_ncolumns)) == NULL) {
	    break;
	}
	
#ifdef SOAP_DIR_SEQNR
	/* The file will be committed when we update the seqno. */
	commit_flags = 0;
#else
	/* No seqno to be updated, so commit right away. */
	commit_flags = BULLET_COMMIT_FLAGS;
#endif
	/* Is new row the same size as old? */
	if ((b_fsize)(p - iobuf) == oldrow_size) {
	    /* File size unchanged; a single modify will do: */
	    err = bullet_op(BS_MODIFY, oldcaps, ncaps, offset, iobuf,
			    oldrow_size, commit_flags,
			    newcaps, &cap_index);
	} else {
	    capability tempcaps[NBULLETS];

	    /* Delete the old row from the file: */
	    err = bullet_op(BS_DELETE, oldcaps, ncaps, offset, iobuf,
			    oldrow_size, 0, tempcaps, &cap_index);
	    
	    if (err == STD_OK) {
		/* Insert the new row: */
		err = bullet_op(BS_INSERT, tempcaps, ncaps,
				offset, iobuf, (b_fsize)(p - iobuf),
				commit_flags, newcaps, &cap_index);
	    }
	}
#ifdef SOAP_DIR_SEQNR
	if (err == STD_OK) {
	    /* Although nrows is still the same, the seqno needs to be
	     * updated to register the directory modification.
	     */
	    err = update_nrows_and_seqno(sd, nrows_offset,
					 &newcaps[cap_index]);
	}
#endif
	break;

    default:
	/* No optimisation other commands; write it as whole below.
	 * Cannot do it here because we have the iobuf locked.
	 */
	err = STD_NOTNOW;
	break;
    }

    sp_put_iobuf(iobuf);

    if (err != STD_OK) {
	if (err != STD_NOTNOW) {
	    /* For some reason, a bullet file modify failed (or we got a
	     * marshalling error, but that should never happen).
	     */
	    scream("dirf_modify: rewrite bullet file; operation is %d (%s)",
		   (int) ss->ss_command, err_why(err));
	}

	/* Just try to write it out as a whole. */
	err = dirf_write(obj, ncaps, newcaps, fsvr_caps);
    }

    return err;
}

/*
 * Reads a directory using one of the capabilities in filecaps;
 * it doesn't matter which.  Returns err status.
 * Returns in dir_ret the newly-malloc'd directory data structure,
 * on success or non-fatal error.  Sets it to NULL on fatal error.
 * A non-fatal error implies that dir_ret points to a self-consistent
 * directory, although it may not have exactly the contents it should.
 */
errstat
dirf_read(obj, filecaps, nfilecaps, dir_ret)
objnum		obj;
capability	filecaps[];	/* caps may be zeroed if dir has bad format! */
int		nfilecaps;
struct sp_dir **dir_ret;
{
    b_fsize size;
    int     i, which;
    int     order[NBULLETS];
    int     bad_format[NBULLETS];
    int     nbad_format = 0;
    char   *p, *iobuf;
    errstat err = STD_NOTFOUND; /* when bulletsvrs storing it are down */
    
    MON_EVENT("read Bullet directory file");
    
    *dir_ret = NULL;

    fsvr_order_caps(filecaps, nfilecaps, order);
    if (order[0] != fsvr_preferred()) {
	MON_EVENT("dirf_read: preferred replica not yet available");
    }

    iobuf = sp_get_iobuf();

    for (i = 0; i < nfilecaps && (which = order[i]) >= 0; i++) {
	assert(!NULLPORT(&filecaps[which].cap_port));

	err = b_read(&filecaps[which], 0L, iobuf,
		     (b_fsize) MAX_DIR_BYTES, &size);
	if (err == STD_OK) {
	    struct sp_dir *newdir = NULL;

	    p = buf_get_dir(iobuf, &iobuf[size], &newdir, &err);
		
	    if (newdir != NULL) {
		/* Switch over to replica just read, deleting previous: */
		if (*dir_ret != NULL) {
		    free_dir(*dir_ret);
		}
		*dir_ret = newdir;
	    }

	    if (err == STD_NOMEM) {
		scream("out of memory loading directory %d from file", obj);
		break;	/* no sense in trying other replicas in this case */
	    } else {
		if (p != &iobuf[size]) {
		    message("dir %d: replica #%d has bad format", obj, which);
		    bad_format[nbad_format++] = which;
		} else {
		    /* Destroy other files that had bad format: */
		    int i;

		    for (i = 0; i < nbad_format; i++) {
			MON_EVENT("dirf_read: destroy bad format dir");
			message("dir %d -- destroyed bad file #%d",
				obj, bad_format[i]);
			destroy_file(&filecaps[bad_format[i]]);
			filecaps[bad_format[i]] = NULLCAP;
		    }

		    /* Success: */
		    break;
		}
	    }
	} else if (err == RPC_NOTFOUND) {
	    message("dirf_read: file server %d unreachable", which);
	    fsvr_set_unavail(&filecaps[which].cap_port);
	} else {
	    message("directory %d -- can't read replica #%d (%s)",
		    obj, which, err_why(err));
		
	    /* If invalid, it is gone forever, so: */
	    if (err == STD_CAPBAD) {
		filecaps[which] = NULLCAP;
		message("dir %d -- cleared file #%d cap", obj, which);
	    }
	}
    }

    sp_put_iobuf(iobuf);
    return err;
}

errstat
dirf_touch(obj, filecaps, good_file)
objnum	   obj;
capability filecaps[NBULLETS];
int	   good_file[NBULLETS];
{
    /*
     * Touch existing files, whether they have a correct port or not.
     * Also initialises the good_file[] array telling which bullet files
     * we could try to use as basis for replicas with a correct port.
     */
    int     j;
    errstat retval;

    retval = SP_UNREACH;
    
    for (j = 0; j < NBULLETS; j++) {
	good_file[j] = 0;	/* only set to 1 in case std_touch succeeds */

	if (fsvr_avail(&filecaps[j].cap_port)) {
	    errstat err;

	    err = std_touch(&filecaps[j]);

	    switch (err) {
	    case STD_OK:
		MON_EVENT("dirf_touch: touched a Bullet directory replica");
		retval = STD_OK;
		good_file[j] = 1;
		break;
	    case STD_CAPBAD:
		MON_EVENT("dirf_touch: zeroed invalid cap");
		message("dirf_touch: dir %ld: zeroed invalid cap", obj);
		filecaps[j] = NULLCAP;
		break;
	    case RPC_NOTFOUND:
		MON_EVENT("dirf_touch: server not found");
		fsvr_set_unavail(&filecaps[j].cap_port);
		break;
	    default:
		MON_EVENT("dirf_touch: std_touch failed");
		message("dirf_touch: dir %ld: std_touch failed (%s; cap=%s)",
			obj, err_why(err), ar_cap(&filecaps[j]));
		if (retval != STD_OK) {
		    retval = err;
		}
		break;
	    }
	}
    }

    return retval;
}

errstat
dirf_check(obj, filecaps)
objnum     obj;
capability filecaps[];
{
    /* Checks if the directory files for this object have sensible and
     * consistent sizes.
     */
    b_fsize size[NBULLETS];
    int     num_good = 0;
    int     j;
    errstat retval = STD_OK;
    errstat err;

    for (j = 0; j < NBULLETS; j++) {
	if (fsvr_avail(&filecaps[j].cap_port)) {
	    /* get the size of this replica */
	    err = b_size(&filecaps[j], &size[j]);

	    if (err == STD_OK) {
		if (size[j] < (PORTSIZE + 2 * sizeof(short))) {	
		    message("directory %ld: replica %d too small",
			    obj, j);
		} else {
		    num_good++;
		}
	    } else {
		retval = err;
		if (err == RPC_NOTFOUND) {
		    fsvr_set_unavail(&filecaps[j].cap_port);
		} else {
		    message("directory %ld: bsize replica %d failed (%s)",
			    obj, j, err_why(err));
		}
	    }
	}
    }

    if (num_good == NBULLETS) {
	/* check that all replicas have the same size */
	for (j = 1; j < NBULLETS; j++) {
	    if (size[j] != size[0]) {
		message("different Bullet file sizes, dir %ld", obj);
		retval = STD_SYSERR;
	    }
	}
    }

    return retval;
}

#ifdef SOAP_GROUP

#define MAX_BYTES_PER_ROW	(NAME_MAX + 2 * (sizeof(suite)+sizeof(int32)))

int
dirf_safe_rows(obj)
objnum obj;
{
    /* Return the number of rows that can be added without risking
     * overflow when finally writing it.
     */
    sp_tab *st;
    char *p, *iobuf;

    st = &_sp_table[obj];
    assert(st->st_dir != NULL);

    iobuf = sp_get_iobuf();
    p = buf_put_dir(iobuf, iobuf + MAX_DIR_BYTES, st->st_dir);
    sp_put_iobuf(iobuf);

    if (p != NULL) {
	return (MAX_DIR_BYTES - (p - iobuf)) / MAX_BYTES_PER_ROW;
    } else {
	return 0;
    }
}

#endif
