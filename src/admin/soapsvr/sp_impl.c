/*	@(#)sp_impl.c	1.6	96/02/27 10:23:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "sp_dir.h"
#include "stdcom.h"
#include "soap/soapsvr.h"
#include "module/stdcmd.h"
#include "module/path.h"
#include "module/cap.h"
#include "module/prv.h"
#define  _POSIX_SOURCE
#include "posix/limits.h"
#include "posix/string.h"
#include <stdlib.h>

#ifdef KERNEL
/* The kernel currently lacks "badassertion" when it is compiled with
 * -DNDEBUG, so we cannot include "sys/assert.h" here.
 */
#ifdef NDEBUG
#define assert(s)	{}
#else
#define assert(s)	if (!(s)) { panic("soap assertion failed"); }
#endif
#else
#include <assert.h>
#endif

#include "monitor.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "module/buffers.h"

#ifdef SOAP_DIR_SEQNR
#include "seqno.h"
#endif

#ifdef SOAP_GROUP
/* Functions marked "STATIC" originally were "static", but the group-based
 * directory now also uses them in other modules.
 */
#define STATIC
#else
#define STATIC static
#endif

#ifdef NO_MEMMOVE
#define memmove(to, from, size)		bcopy(from, to, size)
#endif

/*
 * to be added to some soap include file:
 */
#define SP_NOMOREROWS ((uint16)(-1))
#define h_mask	h_offset

/*
 * external soap implementation routines that are server specific
 */
extern errstat sp_commit();
extern void sp_abort();
extern long sp_time();

#ifdef KERNEL
/* Kernel maintained objects don't need refreshment: */
#define sp_refresh(obj, command)	STD_OK
#else
extern errstat sp_refresh _ARGS((objnum obj, command cmd));
#endif

/*
 * external soap variables that are server specific
 */
extern sp_tab *_sp_table;
extern int _sp_entries;

extern capability _sp_generic;
extern port _sp_getport;

/*
 * administrate how many rows we have allocated currently:
 */
static int allocated_rows;

void
sp_update_nrows(nrows)
int nrows;	/* number of rows that were allocated or deleted */
{
    allocated_rows += nrows;
}

int
sp_allocated_rows()
{
    return allocated_rows;
}

static capability NULLCAP;

#define	alloc(s, n)	sp_alloc((unsigned)(s), (unsigned)(n))
#define Failure(err)	{ failure_err = err; goto failure; }

sp_dir *
alloc_dir()
{
    sp_dir *newdir = (sp_dir *) alloc(sizeof(sp_dir), 1);

    if (newdir != NULL) {
	newdir->sd_colnames = NULL;
	newdir->sd_rows = NULL;
	newdir->sd_mods = NULL;
	newdir->sd_update = NULL;
	newdir->sd_nrows = 0;
    }

    return newdir;
}

char *
sp_str_copy(s)
char *s;
/* A Soap specific version of str_copy() from the Amoeba library.
 * It uses sp_alloc() rather than malloc(), so that it also works
 * when the soap server is short on memory.
 */
{
    register char *new;

    if ((new = (char *) alloc((size_t) (strlen(s) + 1), 1)) != NULL) {
	register char *p = new;

	while ((*p++ = *s++) != '\0') {
	    /* do nothing */
	}
    }
    return new;
}

char *
sp_buf_get_capset(p, e, cs)
char   *p;
char   *e;
capset *cs;
/* Ditto: a Soap specific version of buf_get_capset() */
{
    return buf_get_capset(p, e, cs);
}

static int
sp_cs_copy(new, cs)
capset *new, *cs;
/* Ditto: a Soap specific version of cs_copy() */
{
    register suite *s, *new_s;
    register int i;

    new->cs_initial = cs->cs_initial;
    if ((new->cs_final = cs->cs_final) == 0) {
	new->cs_suite = (suite *) NULL;
	return 1;
    }

    new->cs_suite = (suite *) alloc(sizeof(suite), cs->cs_final);
    if (new->cs_suite == NULL) {
	return 0;
    }

    for (i = 0, s = &cs->cs_suite[0], new_s = &new->cs_suite[0];
	 i < cs->cs_final;
	 i++, s++, new_s++)
    {
	*new_s = *s;
    }

    return 1;
}

STATIC void
free_row(sr)
sp_row *sr;
{
    if (sr->sr_name != NULL) {
	free((_VOIDSTAR) sr->sr_name);
	sr->sr_name = NULL;
    }
    cs_free(&sr->sr_capset);
}

void
free_dir(sd)
sp_dir *sd;
{
    register i;

    if (sd->sd_colnames != NULL) {
	for (i = 0; i < sd->sd_ncolumns; i++) {
	    if (sd->sd_colnames[i] != NULL) {
		free((_VOIDSTAR) sd->sd_colnames[i]);
	    }
	}
	free((_VOIDSTAR) sd->sd_colnames);
    }

    if (sd->sd_rows != NULL) {
	for (i = 0; i < sd->sd_nrows; i++) {
	    if (sd->sd_rows[i] != NULL) {
		free_row(sd->sd_rows[i]);
		free((_VOIDSTAR) sd->sd_rows[i]);
	    }
	}
	free((_VOIDSTAR) sd->sd_rows);
	sp_update_nrows(- sd->sd_nrows);
    }

    free((_VOIDSTAR) sd);
}


STATIC int
copy_row(new, sr)
sp_row *new, *sr;
{
    register i;

    if ((new->sr_name = sp_str_copy(sr->sr_name)) == NULL) {
	return 0;
    }
    if (!sp_cs_copy(&new->sr_capset, &sr->sr_capset)) {
	free((_VOIDSTAR) new->sr_name);
	return 0;
    }
    new->sr_typecap = sr->sr_typecap;
    new->sr_time = sr->sr_time;
    for (i = 0; i < SP_MAXCOLUMNS; i++) {
	new->sr_columns[i] = sr->sr_columns[i];
    }
    return 1;
}

#define ROW_CHUNCK	8
/* The length of the array storing the pointers to the rows of a directory
 * is always a multiple of ROW_CHUNCK.
 * This reduces the need to (re)allocate and copy the row array,
 * when a directory is repeatedly modified.
 */

int
sp_rows_to_allocate(nrows)
int nrows;
{
    return ROW_CHUNCK * ((nrows + ROW_CHUNCK - 1) / ROW_CHUNCK);
}

/*
 * Add a new row to a directory (inserting it at the specified rownum).
 * Initialize it using specified row ptr.  WARNING: The dynamically allocated
 * fields of the row will not be reallocated and copied.
 *
 * Returns 1 or 0, for success or failure, respectively.  The only failure is
 * due to running out of memory in malloc.
 */
STATIC errstat
sp_add_row(st, row, rownum)
sp_tab *st;
sp_row *row;
short   rownum;
{
    sp_dir *sd;
    sp_row *sr_new = NULL;
    errstat failure_err;

    sd = st->st_dir;
    assert(sd != NULL);

    if (rownum < 0 || rownum > sd->sd_nrows) {
	Failure(STD_ARGBAD);
    }

    /* allocate a new row structure */
    if ((sr_new = (sp_row *) alloc(sizeof(sp_row), 1)) == NULL) {
	Failure(STD_NOMEM);
    }
    *sr_new = *row;

    if (sd->sd_nrows == 0) {
	sd->sd_rows = (sp_row **) alloc(sizeof(sp_row *),
					sp_rows_to_allocate(1));
	if (sd->sd_rows == NULL) {
	    Failure(STD_NOMEM);
	}
    } else {
	/* If all currently allocated rows are used, we need to extend
	 * the array of rows pointers. Otherwise there is still space left.
	 */
	if (sd->sd_nrows == sp_rows_to_allocate(sd->sd_nrows)) {
	    /* Avoid realloc(), because that will not uncache directories when
	     * we are out of memory (or when we have too much rows in cache).
	     */
	    sp_row **new_rows, **old_rows;

	    new_rows = (sp_row **)alloc(sizeof(sp_row *),
					sp_rows_to_allocate(sd->sd_nrows + 1));
	    if (new_rows == NULL) {
		Failure(STD_NOMEM);
	    }

	    /* switch over to the new array of row pointers */
	    old_rows = sd->sd_rows;
	    (void) memcpy((_VOIDSTAR) new_rows, (_VOIDSTAR) old_rows,
			  (size_t) sd->sd_nrows * sizeof(sp_row *));
	    sd->sd_rows = new_rows;
	    free((_VOIDSTAR) old_rows);
	}
    }

    if (rownum < sd->sd_nrows) {
	/* Move all row pointers above rownum to the next-higher index, to
	 * make room for the new row pointer:
	 */
	(void) memmove((_VOIDSTAR) &sd->sd_rows[rownum + 1],
		       (_VOIDSTAR) &sd->sd_rows[rownum],
		       (size_t) (sd->sd_nrows - rownum) * sizeof(sp_row *));
    }

    /* add the new row */
    sd->sd_rows[rownum] = sr_new;
    ++sd->sd_nrows;
    sp_update_nrows(1);
    return STD_OK;

failure:
    if (sr_new != NULL) {
	free((_VOIDSTAR) sr_new);
    }
    return failure_err;
}


/*
 * Delete the specified row from the directory st->st_dir, without
 * freeing the substructures of the row:
 */
STATIC errstat
sp_del_row(st, rownum)
sp_tab *st;
short   rownum;
{
    sp_dir *sd;

    sd = st->st_dir;
    assert(sd != NULL);

    if (rownum < 0 || rownum >= sd->sd_nrows) {
	return STD_ARGBAD;
    }

    free((_VOIDSTAR) sd->sd_rows[rownum]);
    sd->sd_rows[rownum] = NULL;
    --sd->sd_nrows;
    sp_update_nrows(-1);

    if (rownum < sd->sd_nrows) {
	/* Copy each of the following row pointers to next lower index,
	 * - to avoid holes in the row pointer array,
	 * - to avoid changing the order of entries
	 * - (most important) to allow the dir_file_modify function
	 *   to work properly in the SOAP server main program
	 */
	(void) memmove((_VOIDSTAR) &sd->sd_rows[rownum],
		       (_VOIDSTAR) &sd->sd_rows[rownum + 1],
		       (size_t) (sd->sd_nrows - rownum) * sizeof(sp_row *));
    }

    if (sd->sd_nrows == 0) {
	free((_VOIDSTAR) sd->sd_rows);
	sd->sd_rows = NULL;
    } else if (sd->sd_nrows == sp_rows_to_allocate(sd->sd_nrows)) {
	/* New number of rows is now exactly at the `roundoff boundary'
	 * so we can now shrink the row pointer array:
	 */
	sp_row **sr;

	sr = (sp_row **) realloc((_VOIDSTAR) sd->sd_rows,
				 (size_t) sd->sd_nrows * sizeof(sp_row *));
	/* The realloc() shouldn't have failed as we tried to make it smaller*/
	assert(sr != NULL);
	sd->sd_rows = sr;
    }

    return STD_OK;
}

static sp_tab *sp_update;
static sp_tab *sp_cur_update;

STATIC void
sp_add_update(st)
sp_tab *st;
{
    /* IF NOT ALREADY THERE, add the changed directory to the update list,
     * where sp_commit can find it later:
     */
    sp_tab *su;

    for (su = sp_update; su != NULL; su = su->st_dir->sd_update) {
	/* members of the update list *must* have sp_dir != NULL */
	assert(su->st_dir != NULL);

	if (su == st) {	/* already in update list */
	    return;
	}
    }

    /* not found; add to update list */
    st->st_dir->sd_update = sp_update;
    sp_update = st;
}

sp_tab *
sp_next_update()
{
    if (sp_cur_update != NULL) {
	assert(sp_cur_update->st_dir != NULL);
	sp_cur_update = sp_cur_update->st_dir->sd_update;
    }
    return sp_cur_update;
}

sp_tab *
sp_first_update()
{
    sp_cur_update = sp_update;
    return sp_cur_update;
}

void
sp_clear_updates()
{
    sp_update = NULL;
    sp_cur_update = NULL;
}

/* Each entry in the directory table (_sp_table[]) has a pointer to a directory
 * (st_dir) and a pointer to a linked list of information about uncommitted
 * changes to that directory (sd_mods).  The changes have already been made
 * in st_dir; the sd_mods field is merely to allow the changes to be undone
 * by sp_undo_mod_dir, in the case where committing the changes fails.  Most
 * of this is only of interest to the SOAP server program, not the kernel or
 * other implementations of SOAP directories.
 *
 * The following function modifies a directory as specified.
 * Before making the modification, it saves sufficient information to
 * undo the change later, if necessary (see sp_undo_mod_dir).  This involves
 * prefixing an entry to the sd_mods list.
 *
 * If hdr->h_command is SP_DELETE, the rownum indicates the row to be deleted.
 * If SP_APPEND, the row is the new row to be added (always at the end of the
 * dir).  (WARNING: The dynamically allocated fields of the row will not be
 * reallocated and copied.)  If SP_REPLACE, the rownum is the row to be
 * affected and row is its new value.  If STD_DESTROY, the st_dir field is
 * zeroed out, but the directory is not freed.  No other commands are allowed.
 *
 * Returns 1 or 0 for failure or success, respectively, and sets hdr->h_status
 * appropriately on failure:
 */
int
sp_save_and_mod_dir(st, hdr, rownum, row)
sp_tab *st;
header *hdr;
short   rownum;
sp_row *row;
{
    struct sp_save *ss;
    sp_dir *sd;

    sd = st->st_dir;
    assert(sd != NULL);

    /* Prefix new entry to beginning of sd_mods linked list for this dir: */
    if ((ss = (struct sp_save *) alloc(sizeof *ss, 1)) == NULL) {
	hdr->h_status = STD_NOMEM;
	return 0;
    }
    ss->ss_next = sd->sd_mods;
    sd->sd_mods = ss;
    st->st_flags |= SF_HAS_MODS;
    ss->ss_command = hdr->h_command;
    ss->ss_rownum = -1;
#ifdef SOAP_DIR_SEQNO
    ss->ss_old_seq = sd->sd_seq_no;
#endif

    switch (ss->ss_command) {
    case SP_APPEND:
	/* Nothing to remember; just do it: */
	if ((hdr->h_status = sp_add_row(st, row, sd->sd_nrows)) != STD_OK) {
	    return 0;
	}
	break;
    case SP_DELETE:
	/* Save old row for possible undo: */
	ss->ss_oldrow = *sd->sd_rows[rownum];
	ss->ss_rownum = rownum;
	if ((hdr->h_status = sp_del_row(st, rownum)) != STD_OK) {
	    return 0;
	}
	break;
    case SP_REPLACE:
    case SP_PUTTYPE:
    case SP_CHMOD:
	/* Save old row for possible undo: */
	ss->ss_command = SP_REPLACE;
	ss->ss_oldrow = *sd->sd_rows[rownum];
	ss->ss_rownum = rownum;

	*sd->sd_rows[rownum] = *row;
	break;
    case SP_CREATE:
	/* nothing to save additionally */
	assert(ss->ss_next == NULL);
	break;
    case STD_DESTROY:
	/* note: st->st_dir is not being touched yet */
	assert(ss->ss_next == NULL);
	st->st_flags |= SF_DESTROYING;
	break;
    default:
	hdr->h_status = STD_COMBAD;
	return 0;
    }

    sp_add_update(st);
    return 1;
}

void
sp_add_to_freelist(st)
sp_tab *st;
{
    st->st_dir = NULL;
    st->st_flags = SF_FREE;
}

/*
 * Deallocate the list of mods for a directory and all saved substructures:
 */
void
sp_free_mods(st)
sp_tab *st;
{
    assert(st->st_dir != NULL);
    assert(st->st_flags & SF_IN_CORE);

    if (st->st_flags & SF_DESTROYING) {
	/* if destroying, there should only be exactly one
	 * STD_DESTROY sp_save structure attached to it
	 */
	assert(st->st_dir->sd_mods != NULL);
	assert(st->st_dir->sd_mods->ss_command == STD_DESTROY);
	assert(st->st_dir->sd_mods->ss_next == NULL);
	
	if (st->st_dir->sd_mods != NULL) {
	    free((_VOIDSTAR)st->st_dir->sd_mods);
	}
	free_dir(st->st_dir);
	sp_add_to_freelist(st);
    } else {
	struct sp_save *ss, *ss_next;

	for (ss = st->st_dir->sd_mods; ss != NULL; ss = ss_next) {
	    ss_next = ss->ss_next;
	    if (ss->ss_rownum >= 0) {
		free_row(&ss->ss_oldrow);
	    }
	    free((_VOIDSTAR)ss);
	}
	st->st_dir->sd_mods = NULL;
    }
    st->st_flags &= ~SF_HAS_MODS;
}


/* Undo all changes to a directory, using the information stored in the sd_mods
 * linked list.  These are in order from newest change to oldest, so they can
 * be undone in the order found in the list:
 */
int
sp_undo_mod_dir(st)
sp_tab *st;
{
    int rtn = 1;
    sp_dir *sd = st->st_dir;
    struct sp_save *ss;
    sp_row temp_row;

    assert(sd != NULL);
    for (ss = sd->sd_mods; ss != NULL; ss = ss->ss_next) {
	/* ss->ss_command indicates what type of change it was: */
	switch (ss->ss_command) {
	case SP_APPEND:
	    /* Save a copy of the row, for freeing: */
	    temp_row = *sd->sd_rows[sd->sd_nrows - 1];

	    /* Delete the row from the sd_rows array: */
	    rtn = (sp_del_row(st, sd->sd_nrows - 1) == STD_OK);
#ifdef SOAP_DIR_SEQNO
	    sd->sd_seqno = ss->ss_old_seqno;
#endif

	    /* Free the substructures of the deleted row: */
	    free_row(&temp_row);

	    /* Safety -- should be unnecessary: */
	    ss->ss_rownum = -1;
	    break;
	case SP_DELETE:
	    rtn = (sp_add_row(st, &ss->ss_oldrow, ss->ss_rownum) == STD_OK);
#ifdef SOAP_DIR_SEQNO
	    sd->sd_seqno = ss->ss_old_seqno;
#endif

	    /* Prevent sp_free_mods from freeing the substructures
	     * of this row:
	     */
	    ss->ss_rownum = -1;
	    break;
	case SP_REPLACE:
	    temp_row = *sd->sd_rows[ss->ss_rownum];
	    *sd->sd_rows[ss->ss_rownum] = ss->ss_oldrow;
#ifdef SOAP_DIR_SEQNO
	    sd->sd_seqno = ss->ss_old_seqno;
#endif

	    /* Cause sp_free_mods to free the new version of the
	     * row instead of the old version:
	     */
	    ss->ss_oldrow = temp_row;
	    break;
	case SP_CREATE:
	    /* let sp_free_mods() remove this new directory: */
	    st->st_flags |= SF_DESTROYING;
	    ss->ss_command = STD_DESTROY;
	    break;
	case STD_DESTROY:
	    assert(st->st_flags & SF_DESTROYING);
	    /* Prevent sp_free_mods from freeing this dir: */
	    st->st_flags &= ~SF_DESTROYING;
#ifdef SOAP_DIR_SEQNO
	    sd->sd_seqno = ss->ss_old_seqno;
#endif
	    break;
	default:
	    rtn = 0;
	}
    }
    sp_free_mods(st);
    return rtn;
}


/* Gets the table entry corresponding to the object specified by the header,
 * but only if the private part of the header is valid.  On entry,
 * hdr->h_command should be set to the operation that is to be performed.
 * Sets the hdr status on error, but leaves it alone on success, to avoid
 * wiping out the h_command field (which is the same field -- aarghh!)
 */
static sp_tab *
sp_getobj(hdr)
header *hdr;
{
    register sp_tab *st;
    objnum      obj;
    rights_bits rights;
    errstat     failure_err;
    errstat     err;

    obj = prv_number(&hdr->h_priv);
    if (obj <= 0 || obj > _sp_entries) {
	MON_EVENT("sp_getobj: obj out of range");
	Failure(STD_CAPBAD);
    }
    st = &_sp_table[obj];

    if (!sp_in_use(st)) {
	MON_EVENT("sp_getobj: object not in use");
	Failure(SP_UNREACH);	/* was: STD_CAPBAD */
    }

    /* There is interest in this object, so give the server program a
     * chance to update its time-to-live field, read it into cache, or
     * whatever, using its own private version of sp_refresh() (the
     * version in the library is a no-op).  Also, let the server know
     * what the command is:
     */
    if ((err = sp_refresh(obj, hdr->h_command)) != STD_OK) {
	Failure(err);
    }

    /* now it should be in core */
    assert((st->st_flags & SF_STATE) == SF_IN_CORE);
    assert(st->st_dir != NULL);

    if (prv_decode(&hdr->h_priv, &rights, &st->st_dir->sd_random) < 0) {
	MON_EVENT("sp_getobj: prv_decode failed");
	Failure(STD_CAPBAD);
    }

    /* success */
    return st;

failure:
    hdr->h_status = failure_err;
    return NULL;
}

/* Get the table entry corresponding to the capability set cs.  Also return
 * the rights mask.  On entry, hdr->h_command should be set to the operation
 * that is to be performed.  On error, sets hdr->h_status and returns 0; on
 * success does not alter anything.
 */
static sp_tab *
sp_getst(cs, mask, hdr)
capset *cs;
long   *mask;
header *hdr;
{
    register int i;
    register suite *s;
    errstat err = STD_CAPBAD;	/* error if none of the entries is for me */

    for (i = 0, s = &cs->cs_suite[0]; i < cs->cs_final; i++, s++) {
	register capability *cap;

	if (!s->s_current) {	/* skip unused suites */
	    continue;
	}
	    
	cap = &s->s_object;

	if (PORTCMP(&cap->cap_port, &_sp_generic.cap_port)) {
	    /* a capability for me; check it */
	    register sp_tab *st;
	    header new_header;

	    /* sp_getobj only uses the h_command and h_priv fields: */
	    new_header.h_command = hdr->h_command;
	    new_header.h_priv = cap->cap_priv;	/* try this object in capset */

	    if ((st = sp_getobj(&new_header)) != NULL) {
		*mask = cap->cap_priv.prv_rights & PRV_ALL_RIGHTS;
		return st;
	    }
	    err = new_header.h_status;
	}
    }

    hdr->h_status = err;
    return NULL;
}

static int
sp_restrict(cap, mask, new)
capability *cap, *new;
long mask;
{
    sp_tab *st;
    header hdr;

    if (!PORTCMP(&cap->cap_port, &_sp_generic.cap_port)) {
	MON_EVENT("sp_restrict");
	return std_restrict(cap, mask, new) != STD_OK;
    }

    MON_EVENT("generate capability locally");
    hdr.h_priv = cap->cap_priv;	/* For sp_getobj */
    hdr.h_command = STD_RESTRICT;
    if ((st = sp_getobj(&hdr)) == NULL) {
	return 1;
    }
    new->cap_port = cap->cap_port;

    assert(st->st_dir != NULL);
    return prv_encode(&new->cap_priv, (objnum) (st - _sp_table),
	      (rights_bits)(mask & PRV_ALL_RIGHTS), &st->st_dir->sd_random) < 0;
}

static char *
buf_gen_capset(buf, end, cs, mask, err_p)
char            *buf;
register char   *end;
register capset *cs;
rights_bits      mask;
errstat		*err_p;
{
    /* Generate a restricted version of the capset cs according to mask,
     * and put it in the buffer.
     */
    register char  *p;
    register suite *s;
    register int    i;
    errstat         err;

    p = buf;
    p = buf_put_int16(p, end, (int16) cs->cs_initial);
    p = buf_put_int16(p, end, (int16) cs->cs_final);
    err = (cs->cs_final == 0) ? STD_OK : SP_UNREACH;
    for (i = 0, s = &cs->cs_suite[0]; i < cs->cs_final; i++, s++) {
	capability newcap;

	if (s->s_current && cc_restrict(&s->s_object, mask,
					&newcap, sp_restrict) == 0) {
	    err = STD_OK;
	} else {
	    /* Not current or rights restrict failed.  Generate a fake
	     * capability with the correct port.
	     */
	    newcap = NULLCAP;
	    newcap.cap_port = s->s_object.cap_port;
	}

	p = buf_put_cap(p, end, &newcap);
	p = buf_put_int16(p, end, (int16) s->s_current);
    }

    *err_p = err;
    return p;
}

typedef void (*fail_func)();

/* A user request arrived.  Enter the critical section.  Check the capability.
 * If correct, check whether the required rights are present or not.
 */
static sp_tab *
sp_acquire(hdr, required, onfailure)
header     *hdr;
rights_bits required;
fail_func  *onfailure;
{
    register sp_tab *st;
    rights_bits rights, column_rights;
    errstat     failure_err;

    sp_begin();
    if ((st = sp_getobj(hdr)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    rights = hdr->h_priv.prv_rights & PRV_ALL_RIGHTS;

    /* When checking the presence of column rights, only take the *actual*
     * columns present into acount (i.e., do not use SP_COLMASK here)
     */ 
    column_rights = (1L << st->st_dir->sd_ncolumns) - 1;

    if ((rights & column_rights) == 0 || (rights & required) != required) {
	Failure(STD_DENIED);
    }
    *onfailure = sp_end;	/* don't forget to call this func */
    return st;

failure:
    sp_end();
    hdr->h_status = failure_err;
    return NULL;
}

/* Check whether the name is ok, i.e., contains only printable characters,
 * no spaces, and no slashes.  Its length should be between 1 and NAME_MAX.
 */
static int
bad_name(name)
char *name;
{
    register char *s;
    register int len;

    for (s = name; *s != '\0'; s++) {
	if (*s < '!' || *s > '~' || *s == '/') {
	    return 1;
	}
    }

    len = s - name;
    return (len == 0 || len > NAME_MAX);
}


static sp_tab *
find_free_slot()
/*
 * Try to find a free slot in _sp_table, and if succesful return it.
 * Otherwise return NULL.
 * Current implementation just searches the whole table from start to
 * finish.  TODO: a "free list" implementation would be much faster.
 */
{
    register sp_tab *st, *end;

    end = &_sp_table[_sp_entries];
    for (st = &_sp_table[1]; st < end; st++) {
	if (!sp_in_use(st)) {
	    st->st_flags = 0;
	    return st;
	}
    }
    return NULL;
}

static void
sp_do_nothing()
{
    /* dummy version of cleanup function;
     * used in case neither sp_end() nor sp_abort() should be called
     */
}

errstat
sp_create_rootdir(colnames, ncolumns, checkfield, rootcs)
char   *colnames[];
int     ncolumns;
port   *checkfield;
capset *rootcs;
{
    /*
     * Used by servers that export a directory interface.
     * It returns in rootcs a capset that can be used to
     * modify the directory graph using the normal directory stubs.
     */
    static int rootdir_created;
    int        col;
    sp_tab    *st = NULL;
    sp_dir    *sd = NULL;
    capability rootcap;
    errstat    failure_err;

    MON_EVENT("sp_create_rootdir");
    if (rootdir_created) {
	/* this function should only be called once */
	Failure(STD_NOTNOW);
    }

    if ((st = find_free_slot()) == NULL) {
	Failure(STD_NOSPACE);
    }
    if ((sd = alloc_dir()) == NULL) {
	Failure(STD_NOMEM);
    }
#ifdef SOAP_DIR_SEQNR
    zero_seq_no(&sd->sd_seq_no);
    sd->sd_old_format = 0;
#endif

    /* use the checkfield provided */
    sd->sd_random = *checkfield;
    rootcap.cap_port = _sp_generic.cap_port;
    if (prv_encode(&rootcap.cap_priv, (objnum) (st - _sp_table),
		   PRV_ALL_RIGHTS, &sd->sd_random) < 0)
    {
	Failure(STD_SYSERR);
    }
    if (!cs_singleton(rootcs, &rootcap)) {
	Failure(STD_NOMEM);
    }

    /* initialise columns of the directory */
    sd->sd_ncolumns = ncolumns;
    sd->sd_colnames = (char **) alloc(sizeof(char *), ncolumns);
    if (sd->sd_colnames == NULL) {
	Failure(STD_NOMEM);
    }
    for (col = 0; col < ncolumns; col++) {
	if ((sd->sd_colnames[col] = sp_str_copy(colnames[col])) == NULL) {
	    Failure(STD_NOMEM);
	}
    }

    /* new directory has no rows yet */
    sd->sd_nrows = 0;
    sd->sd_rows = NULL;

    /* point _sp_table entry to this new directory */
    st->st_dir = sd;
    st->st_flags = SF_IN_CORE;

    rootdir_created = 1;
    return STD_OK;

failure:
    if (sd != NULL) free_dir(sd);
    if (st != NULL) sp_add_to_freelist(st);

    return failure_err;
}

/* Create a directory.  The request contains the names of the columns.  By
 * counting the names, we know the number of columns.
 */
static int
sp_docreate(hdr, buf, size)
header *hdr;
char *buf;
{
    sp_tab   *st = NULL;
    sp_dir   *sd = NULL;
    char     *colnames[SP_MAXCOLUMNS + 1];
    int       ncolumns, col;
    port      checkfield;
    char     *p, *e, *name;
    errstat   failure_err;
    fail_func on_failure = sp_do_nothing;

    MON_EVENT("sp_create");
    ncolumns = 0;
#ifdef KERNEL
    {
	/* we don't want outsiders to create new kernel directories: */
	rights_bits needed = SP_COLMASK | SP_DELRGT | SP_MODRGT;

	hdr->h_command = SP_LOOKUP;	/* temporarily */
	if (sp_acquire(hdr, needed, &on_failure) == NULL) {
	    Failure(ERR_CONVERT(hdr->h_status));
	}
	hdr->h_command = SP_CREATE;	/* restore */
    }
#else
    /* Although the object mentioned in the capability is not really used,
     * the capability itself must be valid.  
     * Careful now!  As sp_refresh() will be called implicitly to make sure
     * it is in cache (so that it can be checked), we must temporarily set
     * the command field to something other than SP_CREATE, because the
     * latter has different semantics for sp_refresh().
     */
    hdr->h_command = SP_LOOKUP;	/* temporarily */
    if (sp_acquire(hdr, 0L, &on_failure) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    hdr->h_command = SP_CREATE;	/* restore */
#endif

    p = buf;
    e = &buf[size];

#ifdef SOAP_GROUP
    /* In this case, the original request was prefixed with the check field
     * for the new directory.
     */
    p = buf_get_port(p, e, &checkfield);
    if (p == NULL) {
	MON_EVENT("sp_docreate: no check field");
	return STD_ARGBAD;
    }
#endif

    /* Get the column names */
    colnames[0] = NULL;
    for (ncolumns = 0; ncolumns < SP_MAXCOLUMNS; ncolumns++) {
	if (p == e) {
	    break;
	}
	if ((p = buf_get_string(p, e, &name)) == NULL) {
	    break;
	}
	if (bad_name(name)) {
	    MON_EVENT("sp_docreate: bad column name");
	    Failure(STD_ARGBAD);
	}
	if ((colnames[ncolumns] = sp_str_copy(name)) == NULL) {
	    Failure(STD_NOMEM);
	}
	colnames[ncolumns + 1] = NULL;
    }
    if (ncolumns == 0 || p != e) {
	MON_EVENT("sp_docreate: column names don't fill the buffer");
	Failure(STD_ARGBAD);
    }

    /* Note: in case of the Group-based Soapsvr we can only guarantee
     * that each server picks the same object number for the new directory
     * when all servers have the same superfile size.
     */
    if ((st = find_free_slot()) == NULL) {
	MON_EVENT("sp_docreate: no free directory slot");
	Failure(STD_NOSPACE);
    }

    if ((sd = alloc_dir()) == NULL) {
	MON_EVENT("sp_docreate: cannot allocate directory");
	Failure(STD_NOMEM);
    }

#ifdef KERNEL
    /* The root directory is created by a call to sp_create_rootdir,
     * so the directory server in the kernel may use its own policy
     * concerning the checkfield of the root.
     * For other directories we can just use new random port.
     */
    uniqport(&checkfield);
#else
#ifndef SOAP_GROUP
    uniqport(&checkfield);
#endif
#endif
    sd->sd_random = checkfield;
    if (prv_encode(&hdr->h_priv, (objnum) (st - _sp_table),
		   PRV_ALL_RIGHTS, &sd->sd_random) < 0)
    {
	MON_EVENT("sp_docreate: prv_encode failure");
	Failure(STD_SYSERR);
    }

    /* initialise columns of the directory */
    sd->sd_ncolumns = ncolumns;
    sd->sd_colnames = (char **) alloc(sizeof(char *), ncolumns);
    if (sd->sd_colnames == NULL) {
	MON_EVENT("sp_docreate: cannot allocate column names");
	Failure(STD_NOMEM);
    }
    for (col = 0; col < ncolumns; col++) {
	sd->sd_colnames[col] = colnames[col];
	colnames[col] = NULL;
    }

    /* new directory has no rows yet */
    sd->sd_nrows = 0;
    sd->sd_rows = NULL;

    /* point _sp_table entry to this new directory */
    st->st_dir = sd;
    st->st_flags = SF_IN_CORE;

#ifdef SOAP_DIR_SEQNR
    sp_init_dirseqno(&sd->sd_seq_no);
    sd->sd_old_format = 0;
#endif

    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, 0, (sp_row *) NULL)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    /* Give server program a chance to initialize it: */
    hdr->h_status = sp_commit();

    if (hdr->h_status == STD_OK) {
	/* Now that we know that the creation is accepted we give the server
	 * the chance to update its cache, if needed.
	 */
	if (sp_refresh((objnum)(st - _sp_table), SP_CREATE) != STD_OK) {
	    /* don't return failure; the creation has been accepted already */
	    MON_EVENT("sp_docreate: refresh failed");
	}
    }

    return 0;

failure:
    /*
     * The local variables provide enough information to be able to see what
     * has to be undone.
     */
    if (sd != NULL) free_dir(sd);
    if (st != NULL) sp_add_to_freelist(st);
    (*on_failure)();

    for (col = 0; col < ncolumns; col++) {
	if (colnames[col] != NULL) {
	    free((_VOIDSTAR) colnames[col]);
	}
    }
    hdr->h_status = failure_err;
    return 0;
}

/*
 * Destroy a directory.  Simple.  Only allow this if the directory is empty.
 */
static int
sp_dodestroy(hdr)
header *hdr;
{
    sp_tab   *st;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_destroy");

    hdr->h_command = STD_DESTROY;	/* Get rid of synonomous SP_DISCARD */
    if ((st = sp_acquire(hdr, SP_DELRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    if (st->st_dir->sd_nrows != 0) {
	Failure(SP_NOTEMPTY);
    }

    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, 0, (sp_row *) NULL)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    /* success */
    hdr->h_status = sp_commit();
    return 0;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* List a directory.  Returns a flattened representation of the number of
 * columns, the number of rows, the names of the columns, the names of the
 * rows and the right masks.  hdr->h_extra indicates the desired starting
 * row, on input, and indicates the first row not returned (due to lack of
 * space), on output.  If all rows were returned, hdr->h_extra is set to
 * a special value to tell the client that another transaction is not
 * necessary.
 */
static int
sp_dolist(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    sp_tab   *st;
    sp_dir   *sd;
    int	      col, row;
    char     *p, *e;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_list");
    if (hdr->h_size > (unsigned) size) {
	Failure(STD_ARGBAD);
    }
    if ((st = sp_acquire(hdr, 0L, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    e = &buf[hdr->h_size];
    sd = st->st_dir;

    /* put global dir info in buffer */
    p = buf_put_int16(buf, e, (int16) sd->sd_ncolumns);
    p = buf_put_int16(p, e, (int16) sd->sd_nrows);
    for (col = 0; col < sd->sd_ncolumns; col++) {
	p = buf_put_string(p, e, sd->sd_colnames[col]);
    }

    /* put rows in buffer; h_extra specifies rownum where we start */
    for (row = hdr->h_extra; row < sd->sd_nrows; row++) {
	sp_row   *sr;

	hdr->h_extra = row;	/* The lowest row not yet stored */
	hdr->h_size = p - buf;	/* Size of buf up to last full row */
	sr = sd->sd_rows[row];
	p = buf_put_string(p, e, sr->sr_name);
	for (col = 0; col < sd->sd_ncolumns; col++) {
	    p = buf_put_right_bits(p, e, sr->sr_columns[col]);
	}
	if (p == NULL) {
	    break;
	}
    }

    /* success; possibly partial */
    sp_end();
    if (p != NULL) {
	hdr->h_size = p - buf;
	hdr->h_extra = SP_NOMOREROWS;	/* There are no more rows */
    }
    hdr->h_status = STD_OK;
    return hdr->h_size;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

STATIC int
sp_search_row(sd, name)
sp_dir *sd;
char *name;
/*
 * return pointer to row with with name ``name'' or NULL
 */
{
    register struct sp_row **srp, **sr_end;
    register int firstchar;

    firstchar = *name;
    sr_end = &sd->sd_rows[sd->sd_nrows];

    for (srp = sd->sd_rows; srp < sr_end; srp++) {
	register struct sp_row *sr = *srp;

	if ((*sr->sr_name == firstchar) && (strcmp(sr->sr_name, name) == 0)) {
	    return srp - sd->sd_rows;
	}
    }

    /* not found */
    return -1;
}

#ifdef PROFILING
/* to see how much the commits for the commands costs on average */
static errstat sp_app_commit() { return sp_commit(); }
static errstat sp_rep_commit() { return sp_commit(); }
static errstat sp_del_commit() { return sp_commit(); }
#else
#define sp_app_commit	sp_commit
#define sp_rep_commit	sp_commit
#define sp_del_commit	sp_commit
#endif

/* Append a row to a directory.  The name, right masks, and initial
 * capability set is specified.  A lot of work, but rather simple.
 */
static int
sp_doappend(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    sp_tab     *st;
    sp_dir     *sd;
    sp_row      newrow, *srn = NULL;
    capset      cs, *csp = NULL;
    int         col;
    char       *p, *e, *name;
    rights_bits new_cols[SP_MAXCOLUMNS];
    fail_func   on_failure = sp_do_nothing;
    errstat     failure_err;

    MON_EVENT("sp_append");
    e = &buf[size];

    /* get and check rowname arg */
    if ((p = buf_get_string(buf, e, &name)) == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    /* get capset arg */
    if ((p = sp_buf_get_capset(p, e, &cs)) == NULL) {
	Failure(STD_ARGBAD);
    }
    csp = &cs;	/* deallocate on error */

    /* Allow 1 up to SP_MAXCOLUMNS new masks to be present.
     * The caller may not know (or care) how many columns this directory has.
     */
    for (col = 0; col < SP_MAXCOLUMNS; col++) {
	if ((p = buf_get_right_bits(p, e, &new_cols[col])) == NULL) {
	    if (col < 1) {
		/* fail: at least one column mask required */
		Failure(STD_ARGBAD);
	    } else {
		/* less columns provided than SP_MAXCOLUMNS, assume 0 */
		new_cols[col] = 0;
	    }
	}
    }
    if (p != NULL && p != e) {	/* more than SP_MAXCOLUMNS masks? */
	Failure(STD_ARGBAD);
    }

    if ((st = sp_acquire(hdr, SP_MODRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;

    /* check that the entry doesn't exists already */
    if (sp_search_row(sd, name) >= 0) {
	Failure(STD_EXISTS);
    }

    /* Prepare the new row: */
    srn = &newrow;
    srn->sr_name = NULL;
    srn->sr_typecap = NULLCAP;
    srn->sr_time = sp_time();
    srn->sr_capset = cs;
    csp = NULL;	/* free_row() takes care of freeing the capset */

    if ((srn->sr_name = sp_str_copy(name)) == NULL) {
	Failure(STD_NOMEM);
    }

    /* Take as much masks from new_cols as we need for this dir */
    for (col = 0; col < sd->sd_ncolumns; col++) {
	srn->sr_columns[col] = new_cols[col];
    }

    /* do the update */
    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, 0, srn)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    hdr->h_status = sp_app_commit();
    return 0;

failure:
    if (csp != NULL) cs_free(csp);
    if (srn != NULL) free_row(srn);
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* Replace a capability set.  The name and new capability set is specified.
 */
static int
sp_doreplace(hdr, buf, size)
header *hdr;
char *buf;
{
    sp_tab *st;
    sp_dir   *sd;
    sp_row   *sr;
    capset    cs, *csp = NULL;
    sp_row    newrow, *srn = NULL;
    int       rownum;
    char     *p, *e, *name;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_replace");
    e = &buf[size];

    /* get and check rowname arg */
    if ((p = buf_get_string(buf, e, &name)) == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    /* get capset arg */
    if ((p = sp_buf_get_capset(p, e, &cs)) == NULL) {
	Failure(STD_ARGBAD);
    }
    csp = &cs;	/* deallocate on error */

    if ((st = sp_acquire(hdr, SP_MODRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;

    /* check if entry exists */
    if ((rownum = sp_search_row(sd, name)) < 0) {
	Failure(STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    /* Prepare the new row: */
    if (!copy_row(&newrow, sr)) {
	Failure(STD_NOMEM);
    }
    srn = &newrow;
    srn->sr_time = sp_time();
    (void)cs_free(&srn->sr_capset);
    srn->sr_capset = cs;
    csp = NULL;	/* cs is cs_free()d by free_row() */

    /* do the update */
    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, rownum, srn)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
	    
    hdr->h_status = sp_rep_commit();
    return 0;

failure:
    if (csp != NULL) cs_free(csp);
    if (srn != NULL) free_row(srn);
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* Change the rights masks in a row.
 */
static int
sp_dochmod(hdr, buf, size)
header *hdr;
char *buf;
{
    sp_tab     *st;
    sp_dir     *sd;
    sp_row     *sr;
    int         col, rownum;
    sp_row      newrow, *srn = NULL;
    char       *p, *e, *name;
    rights_bits new_cols[SP_MAXCOLUMNS];
    fail_func   on_failure = sp_do_nothing;
    errstat     failure_err;

    MON_EVENT("sp_chmod");
    e = &buf[size];
    if ((p = buf_get_string(buf, e, &name)) == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    /* Allow 1 up to SP_MAXCOLUMNS new masks to be present.
     * The caller may not know (or care) how many columns this directory has.
     */
    for (col = 0; col < SP_MAXCOLUMNS; col++) {
	if ((p = buf_get_right_bits(p, e, &new_cols[col])) == NULL) {
	    if (col < 1) {
		/* fail: at least one column mask required */
		Failure(STD_ARGBAD);
	    } else {
		/* less columns provided than SP_MAXCOLUMNS, assume 0 */
		new_cols[col] = 0;
	    }
	}
    }
    if (p != NULL && p != e) {	/* more than SP_MAXCOLUMNS masks? */
	Failure(STD_ARGBAD);
    }

    if ((st = sp_acquire(hdr, SP_MODRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;

    if ((rownum = sp_search_row(sd, name)) < 0) {	/* not found */
	Failure(STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    /* Prepare the new row: */
    if (!copy_row(&newrow, sr)) {
	Failure(STD_NOMEM);
    }
    srn = &newrow;
    srn->sr_time = sp_time();

    /* Take as much masks from new_cols as we need for this dir */
    for (col = 0; col < sd->sd_ncolumns; col++) {
	srn->sr_columns[col] = new_cols[col];
    }

    /* do the update */
    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, rownum, srn)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    hdr->h_status = sp_commit();
    return 0;

failure:
    if (srn != NULL) free_row(srn);
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* Return the rights masks in a row.
 */
static int
sp_dogetmasks(hdr, inbuf, insize, outbuf, outsize)
header *hdr;
char   *inbuf;
int     insize;
char   *outbuf;
int	outsize;
{
    sp_tab *st;
    sp_dir   *sd;
    sp_row   *sr;
    int       col, rownum;
    char     *p, *e, *name;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_getmasks");
    e = &inbuf[insize];

    if ((st = sp_acquire(hdr, 0L, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;

    if ((p = buf_get_string(inbuf, e, &name)) == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    if ((rownum = sp_search_row(sd, name)) < 0) { /* not found */
	Failure(STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    on_failure = sp_abort;
    p = outbuf;
    e = &outbuf[outsize];
    for (col = 0; col < sd->sd_ncolumns; col++) {
	p = buf_put_right_bits(p, e, sr->sr_columns[col]);
    }
    if (p == NULL) {
	Failure(STD_OVERFLOW);
    }

    sp_end();
    hdr->h_size = p - outbuf;
    hdr->h_status = STD_OK;
    return hdr->h_size;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

#ifdef SOAP_DIR_SEQNR

extern char *buf_put_seqno();

/* Return the seqno of the directory. */
static int
sp_dogetseqno(hdr, outbuf, outsize)
header *hdr;
char   *outbuf;
int	outsize;
{
    sp_tab   *st;
    char     *p;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_getseqno");
    if ((st = sp_acquire(hdr, 0L, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    p = buf_put_seqno(outbuf, &outbuf[outsize], &st->st_dir->sd_seq_no);
    if (p == NULL) {
	Failure(STD_OVERFLOW);
    }

    sp_end();
    hdr->h_size = p - outbuf;
    hdr->h_status = STD_OK;
    return hdr->h_size;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

#endif /* SOAP_DIR_SEQNR */

/* Delete a row.
 */
static int
sp_dodelete(hdr, buf, size)
header *hdr;
char *buf;
{
    sp_tab   *st;
    sp_dir   *sd;
    char     *name;
    int       rownum;
    fail_func on_failure = sp_do_nothing;
    errstat   failure_err;

    MON_EVENT("sp_delete");
    if (buf_get_string(buf, &buf[size], &name) == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    if ((st = sp_acquire(hdr, SP_MODRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;

    if ((rownum = sp_search_row(sd, name)) < 0) { /* not found */
	Failure(STD_NOTFOUND);
    }

    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, rownum, (sp_row *) NULL)){
	Failure(ERR_CONVERT(hdr->h_status));
    }

    hdr->h_status = sp_del_commit();
    return 0;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* Traverse a path as far as possible, and return the resulting capability
 * set and the rest of the path.
 */
static int
sp_dolookup(hdr, inbuf, insize, outbuf, outsize)
header *hdr;
char   *inbuf;
int     insize;
char   *outbuf;
int	outsize;
{
    register char *slash;
    sp_tab        *st;
    rights_bits    have, colmask, column_rights;
    int	           rownum, noperm;
    capset        *dir;
    char          *name;
    char          *p, *e;
    fail_func      on_failure = sp_do_nothing;

    MON_EVENT("sp_lookup");
    e = &inbuf[insize];
    if ((p = buf_get_string(inbuf, e, &name)) == 0) {
	hdr->h_status = STD_ARGBAD;
	return 0;
    }

    if ((st = sp_acquire(hdr, 0L, &on_failure)) == 0) {
	return 0;
    }

    colmask = hdr->h_priv.prv_rights & PRV_ALL_RIGHTS;
    have = ~0;
    noperm = 0;
    do {
	register sp_dir *sd;
	register char *next;

	while (*name == '/') {	/* skip multiple slashes */
	    name++;
	}
	if (*name == 0) {	/* we're finished */
	    break;
	}
	next = name;

	/* get next component, terminated by slash or '\0' */
	slash = NULL;
	while (*next != 0) {
	    if (*next == '/') {
		*(slash = next++) = 0;	/* terminate it */
		break;
	    } else {
		next++;
	    }
	}

	/* When checking the presence of column rights, only take the *actual*
	 * columns present into acount, so do not use SP_COLMASK here.
	 */ 
	sd = st->st_dir;
	column_rights = (1L << sd->sd_ncolumns) - 1;
	colmask &= (have & column_rights);

	/* if we have no permission to read the directory, return */
	if (colmask == 0) {
	    dir = NULL;
	    noperm = 1;
	    break;
	}

	/* TODO: if (bad_name(name)) return STD_ARGBAD; */

	/* look for the entry */
	if ((rownum = sp_search_row(sd, name)) >= 0) {
	    /* calculate the rights mask */
	    register int col;
	    sp_row   *sr;

	    sr = sd->sd_rows[rownum];
	    for (have = 0, col = 0; col < sd->sd_ncolumns; col++) {
		if ((colmask >> col) & 1) {
		    have |= sr->sr_columns[col];
		}
	    }
	    dir = &sr->sr_capset;
	    name = next;
	}

	if (slash != NULL) {	/* restore slash */
	    *slash = '/';
	}

	/* if the entry is not in the directory, return */
	if (rownum < 0) {
	    dir = NULL;
	    break;
	}
    } while (slash != NULL && (st = sp_getst(dir, &colmask, hdr)) != NULL);

    sp_end();

    hdr->h_size = 0;
    if (noperm) {
	hdr->h_status = STD_DENIED;
    } else if (dir == NULL) {
	hdr->h_status = STD_NOTFOUND;
    } else {
	errstat err;

	e = &outbuf[outsize];
	p = buf_put_string(outbuf, e, name);	/* unparsed piece of path */
	p = buf_gen_capset(p, e, dir, have, &err);
	if (p != NULL) {
	    hdr->h_size = (p - outbuf);
	    hdr->h_status = err;
	} else {
	    hdr->h_status = STD_OVERFLOW;
	}
    }

    return hdr->h_size;
}

static char *
sp_find(p, e, dir, name)
char *p, *e;
capset *dir;
char *name;
{
    sp_tab     *st;
    sp_dir     *sd;
    sp_row     *sr;
    rights_bits have, colmask;
    int         col, rownum;
    header      hdr;
    char       *save_p;
    errstat     status;

    hdr.h_command = SP_SETLOOKUP;
    if ((st = sp_getst(dir, &colmask, &hdr)) == NULL) {
	return buf_put_int16(p, e, (int16) hdr.h_status);
    }
    sd = st->st_dir;

    if (bad_name(name)) {
	return buf_put_int16(p, e, (int16) STD_ARGBAD);
    }

    if ((rownum = sp_search_row(sd, name)) < 0) {    /* not found */
	return buf_put_int16(p, e, (int16) STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    /* calculate the rights mask */
    have = 0;
    for (col = 0; col < sd->sd_ncolumns; col++) {
	if ((colmask >> col) & 1) {
	    have |= sr->sr_columns[col];
	}
    }

    status = STD_OK;
    save_p = p;
    p = buf_put_int16 (p, e, (int16) status);
    p = buf_put_cap   (p, e, &sr->sr_typecap);
    p = buf_put_int32 (p, e, (int32) sr->sr_time);
    p = buf_gen_capset(p, e, &sr->sr_capset, have, &status);

    if (status != STD_OK) {
	/* oops, not OK after all */
	p = buf_put_int16(save_p, e, (int16) status);
    }

    return p;
}

static int
sp_dosetlookup(hdr, inbuf, insize, outbuf, outsize)
header *hdr;
char   *inbuf;
int     insize;
char   *outbuf;
int     outsize;
{
    char *inp, *ine, *name;
    char *outp, *oute;
    capset dir, *dirp = NULL;
    errstat failure_err;

    MON_EVENT("sp_setlookup");

    inp = inbuf;
    ine = &inbuf[insize];
    outp = outbuf;
    oute = &outbuf[outsize];

    /* the entries are listed in the buffer */
    sp_begin();
    do {
	if ((inp = sp_buf_get_capset(inp, ine, &dir)) == NULL) {
	    Failure(STD_ARGBAD);
	}
	dirp = &dir;
	if ((inp = buf_get_string(inp, ine, &name)) == NULL) {
	    Failure(STD_ARGBAD);
	}
	outp = sp_find(outp, oute, &dir, name);
	cs_free(&dir);
	dirp = NULL;
    } while (inp != ine && outp != NULL);

    if (outp == NULL) {
	Failure(STD_OVERFLOW);
    }

    /* success */
    sp_end();
    hdr->h_size = outp - outbuf;
    hdr->h_status = STD_OK;
    return hdr->h_size;

failure:
    sp_end();
    if (dirp != NULL) cs_free(dirp);
    hdr->h_status = failure_err;
    return 0;
}

/* This routine replaces the capability set belonging to name in the
 * directory pointed to by st.  oldcap, if not null, must be in the current
 * capability set for the update to succeed.
 */
static int
sp_tryreplace(st, name, cs, oldcap)
sp_tab *st;
char *name;
capset *cs;
capability *oldcap;
{
    sp_dir  *sd = st->st_dir;
    sp_row  *sr;
    int      rownum;
    header   hdr;
    sp_row   newrow, *srn = NULL;
    capset   cs_old;
    errstat  failure_err;

    if ((rownum = sp_search_row(sd, name)) < 0) {    /* not found */
	Failure(STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    if (NULLPORT(&oldcap->cap_port)) {
	if (sr->sr_capset.cs_final != 0) {
	    MON_EVENT("sp_tryreplace: cs_final != 0");
	    Failure(SP_CLASH);
	}
    } else {
	if (!cs_member(&sr->sr_capset, oldcap)) {
	    /* But we must allow the object manager to install
	     * new objects when it doesn't have an owner cap for the original.
	     * To make sure it really is the original object, we have to
	     * use std_restrict() (which may need some help from the server).
	     */
	    capset *csp = &sr->sr_capset;
	    capability newcap;
	    int i;

	    for (i = 0; i < csp->cs_final; i++) {
		rights_bits rights, stored_rights;
		suite *s = &csp->cs_suite[i];

		rights = oldcap->cap_priv.prv_rights;
		stored_rights = s->s_object.cap_priv.prv_rights;
		if (s->s_current &&
		    PORTCMP(&oldcap->cap_port, &s->s_object.cap_port) &&
		    ((stored_rights & rights) == rights) &&
		    (std_restrict(&s->s_object, rights, &newcap) == STD_OK))
		{
		    if (cap_cmp(&newcap, oldcap)) {
			MON_EVENT("sp_tryreplace: got restricted original");
			break;
		    } else {
			MON_EVENT("sp_tryreplace: wrong restricted original");
		    }
		}

	    }

	    if (i >= csp->cs_final) {
		Failure(SP_CLASH);
	    }
	}
    }

    /* Prepare the new row: */
    if (!copy_row(&newrow, sr)) {
	Failure(STD_NOMEM);
    }
    srn = &newrow;
    srn->sr_time = sp_time();

    /* Modify the new row's capset and put it in the dir */
    cs_old = srn->sr_capset;
    if (!sp_cs_copy(&srn->sr_capset, cs)) {
	srn->sr_capset = cs_old;	/* first restore */
	Failure(STD_NOMEM);
    }
    cs_free(&cs_old);

    hdr.h_command = SP_REPLACE;
    if (!sp_save_and_mod_dir(st, &hdr, rownum, srn)) {
	Failure(ERR_CONVERT(hdr.h_status));
    }

    /* success */
    return STD_OK;

failure:
    if (srn != NULL) free_row(srn);
    return failure_err;
}

/* Update a set of directory entries.  All entries have to be at this
 * directory service or it won't work.  Specified are capability sets for
 * directories, and names within those directories (simple names, no
 * pathnames).  Moreover, an old capability can be specified which has to
 * be in the current capability set for the update to succeed.
 */
static int
sp_doinstall(hdr, buf, size)
header *hdr;
char *buf;
{
    sp_tab     *st;
    rights_bits mask, column_rights;
    capset      dir, cs;
    capset     *dirp = NULL, *csp = NULL;
    capability  oldcap;
    char       *p, *e, *name;
    fail_func   on_failure = sp_do_nothing;
    errstat     err, failure_err;

    MON_EVENT("sp_install");

    p = buf;
    e = &buf[size];

    sp_begin();
    on_failure = sp_end;

    /* the entries are listed in the buffer */
    do {
	/* get params (capset;string;capset;cap) from buffer */
	p = sp_buf_get_capset(p, e, &dir);
	if (p != NULL) {
	    dirp = &dir;	/* deallocate on error */
	}
	p = buf_get_string(p, e, &name);
	p = sp_buf_get_capset(p, e, &cs);
	if (p != NULL) {
	    csp = &cs;		/* deallocate on error */
	}
	p = buf_get_cap(p, e, &oldcap);

	if (p == NULL || bad_name(name)) { /* could not fetch all 4 params */
	    Failure(STD_ARGBAD);
	}

	/* check directory */
	if ((st = sp_getst(&dir, &mask, hdr)) == NULL) {
	    Failure(ERR_CONVERT(hdr->h_status));
	}

	/* When checking the presence of column rights, only take the *actual*
	 * columns present into acount, so do not use SP_COLMASK here.
	 */ 
	column_rights = (1L << st->st_dir->sd_ncolumns) - 1;
	if ((mask & SP_MODRGT) == 0 || (mask & column_rights) == 0) {
	    /* directory must be modifiable, and we need access to a column */
	    Failure(STD_DENIED);
	}

	/* from now on, undo changes made by sp_tryplace() calls: */
	on_failure = sp_abort;
	if ((err = sp_tryreplace(st, name, &cs, &oldcap)) != STD_OK) {
	    Failure(err);
	}

	/* free capsets fetched from request buffer */
	cs_free(dirp);
	cs_free(csp);
	csp = dirp = NULL;
    } while (p != e);

    /* success */
    hdr->h_status = sp_commit();
    return 0;

failure:
    (*on_failure)();
    if (csp != NULL) cs_free(csp);
    if (dirp != NULL) cs_free(dirp);
    hdr->h_status = failure_err;
    return 0;
}

/* This routine is probably called by Guido.  Hi Guido.  Guido wants to
 * specify the type of the object by means of a capability.  We have reserved
 * a field in each directory entry for this.
 */
static int
sp_doputtype(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    sp_tab    *st;
    sp_dir    *sd;
    sp_row    *sr, newrow, *srn = NULL;
    capability typecap;
    int        rownum;
    char      *p, *e, *name;
    fail_func  on_failure = sp_do_nothing;
    errstat    failure_err;

    MON_EVENT("sp_puttype");
    e = &buf[size];
    p = buf_get_string(buf, e, &name);
    p = buf_get_cap(p, e, &typecap);
    if (p == NULL || bad_name(name)) {
	Failure(STD_ARGBAD);
    }

    if ((st = sp_acquire(hdr, SP_MODRGT, &on_failure)) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }
    sd = st->st_dir;
    
    if ((rownum = sp_search_row(sd, name)) < 0) {	/* not found */
	Failure(STD_NOTFOUND);
    }
    sr = sd->sd_rows[rownum];

    /* Prepare the new row: */
    if (!copy_row(&newrow, sr)) {
	Failure(STD_NOMEM);
    }
    srn = &newrow;
    srn->sr_time = sp_time();
    srn->sr_typecap = typecap;

    /* do the update */
    on_failure = sp_abort;
    if (!sp_save_and_mod_dir(st, hdr, rownum, srn)) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    /* success */
    hdr->h_status = sp_commit();
    return 0;

failure:
    if (srn != NULL) free_row(srn);
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

/* The only effect is to update the time-to-live attribute of the object.
 * sp_getobj will do this for us:
 */
static int
sp_dotouch(hdr)
header *hdr;
{
    MON_EVENT("std_touch");
    if (prv_number(&hdr->h_priv) == 0) {
	/* touch on super cap */
	hdr->h_status = STD_OK;
    } else {
	sp_begin();
	if (sp_getobj(hdr) != NULL) {
	    hdr->h_status = STD_OK;
	}
	/* else hdr->h_status was set to error code by sp_getobj */
	sp_end();
    }
    return 0;
}

/* This is the same as sp_dotouch, only it handles a set of directories
 * to be touched (specified by their private parts) in a single request.
 * Returns in the header the number of directories touched and the
 * last error that occured, if any.
 */
errstat
sp_dontouch(hdr, buf, size)
header	*hdr;
char	*buf;	/* buffer containing private parts of caps to touch */
int	 size;
{
    bufsize count;
    errstat ret;

    MON_EVENT("std_ntouch");
    ret = STD_OK; /* return STD_OK unless a touch fails */
    count = 0;

    if (hdr->h_size * sizeof(private) != size) {
	ret = STD_ARGBAD;
    } else {
	header  touch_hdr;	/* to contain the STD_TOUCH requests */
	char   *p, *end;
	int     n;

	p = buf;
	end = buf + size;
	touch_hdr.h_port = hdr->h_port;
	for (n = hdr->h_size; n > 0; n--) {
	    errstat err;

	    /* reinitialize h_command; it may be overwritten */
	    touch_hdr.h_command = STD_TOUCH;
	    if ((p = buf_get_priv(p, end, &touch_hdr.h_priv)) == NULL) {
		/* should not happen; we've checked the size */
		ret = STD_ARGBAD;
		break;
	    }

	    err = STD_OK;
	    if (prv_number(&touch_hdr.h_priv) != 0) {
		/* a touch on the super cap (object 0) has no effect */
		sp_begin();
		if (sp_getobj(&touch_hdr) == NULL) {
		    err = ERR_CONVERT(touch_hdr.h_status);
		}
		sp_end();
	    }

	    if (err != STD_OK) {
		ret = err;
	    } else {
		count++;
	    }
	}
    }

    hdr->h_extra = count; /* number directories sucessfully touched */
    hdr->h_status = ret;
    return 0;
}


/* Somebody wants to know what the heck this capability is for.
 * Return a small descriptive string.
 */
static int
sp_doinfo(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    MON_EVENT("std_info");

    if (size < 30) {	/* sanity check */
	hdr->h_status = STD_SYSERR;
	return 0;
    }

    if (prv_number(&hdr->h_priv) == 0) {
	(void) strcpy(buf, "soap generic capability");
	hdr->h_status = STD_OK;
    } else {
	sp_begin();
	if (sp_getobj(hdr) == NULL) {
	    /* hdr->h_status set by sp_getobj */
	    *buf = '\0';
	} else {
	    rights_bits rights = hdr->h_priv.prv_rights & PRV_ALL_RIGHTS;
	    char *p = buf;

#ifdef KERNEL
	    *p++ = '%';
#else
	    *p++ = '/';
#endif
	    *p++ = rights & SP_DELRGT ? 'd' : '-';
	    *p++ = rights & SP_MODRGT ? 'w' : '-';
	    *p++ = rights & 0x1       ? '1' : '-';
	    *p++ = rights & 0x2       ? '2' : '-';
	    *p++ = rights & 0x4       ? '3' : '-';
	    *p++ = rights & 0x8       ? '4' : '-';
	    *p = '\0';
	    hdr->h_status = STD_OK;
	}
	sp_end();
    }

    hdr->h_size = strlen(buf);
    return hdr->h_size;
}

/* Restrict the rights in a capability.
 */
static int
sp_dorestrict(hdr)
header *hdr;
{
    capability cap, new;
    fail_func  on_failure = sp_do_nothing;
    errstat    failure_err;

    MON_EVENT("sp_restrict");
    if (sp_acquire(hdr, 0L, &on_failure) == NULL) {
	Failure(ERR_CONVERT(hdr->h_status));
    }

    cap.cap_port = hdr->h_port;
    cap.cap_priv = hdr->h_priv;
    if (cc_restrict(&cap, hdr->h_mask, &new, sp_restrict) != 0) {
	Failure(STD_CAPBAD);
    }

    /* success */
    sp_end();
    hdr->h_priv = new.cap_priv;	/* return private part of restricted cap */
    hdr->h_status = STD_OK;
    return 0;

failure:
    (*on_failure)();
    hdr->h_status = failure_err;
    return 0;
}

int
sp_trans(hdr, buf, size, outbuf, outsize)
header *hdr;
char   *buf;
int     size;
char   *outbuf;
int     outsize;
{
    if (!PORTCMP(&hdr->h_port, &_sp_generic.cap_port) &&
	!PORTCMP(&hdr->h_port, &_sp_getport))
    {
	/* This is the case when the server implementing a directory
	 * service tries to perform a soap operation on a remote object.
	 */
	bufsize bs;

	bs = trans(hdr, buf, (bufsize)size, hdr, outbuf, (bufsize)outsize);
	if (ERR_STATUS(bs)) {
	    hdr->h_status = bs;
	    return 0;
	}
	return bs;
    }

    switch (hdr->h_command) {
    case SP_LOOKUP:    return sp_dolookup    (hdr, buf, size, outbuf, outsize);
    case SP_SETLOOKUP: return sp_dosetlookup (hdr, buf, size, outbuf, outsize);
    case SP_GETMASKS:  return sp_dogetmasks  (hdr, buf, size, outbuf, outsize);
    case STD_INFO:     return sp_doinfo      (hdr, outbuf, outsize);
    case STD_TOUCH:    return sp_dotouch     (hdr);
    case STD_NTOUCH:   return sp_dontouch    (hdr, buf, size);
    case STD_RESTRICT: return sp_dorestrict  (hdr);
    case STD_DESTROY:
    case SP_DISCARD:   return sp_dodestroy   (hdr);
    case SP_LIST:      return sp_dolist      (hdr, outbuf, outsize);
    case SP_CREATE:    return sp_docreate    (hdr, buf, size);
    case SP_APPEND:    return sp_doappend    (hdr, buf, size);
    case SP_REPLACE:   return sp_doreplace   (hdr, buf, size);
    case SP_CHMOD:     return sp_dochmod     (hdr, buf, size);
    case SP_DELETE:    return sp_dodelete    (hdr, buf, size);
    case SP_INSTALL:   return sp_doinstall   (hdr, buf, size);
    case SP_PUTTYPE:   return sp_doputtype   (hdr, buf, size);
#ifdef SOAP_DIR_SEQNR
    case SP_GETSEQNR:  return sp_dogetseqno  (hdr, outbuf, outsize);
#endif
    default:	       MON_EVENT("bad command");
		       hdr->h_status = STD_COMBAD;
		       return 0;
    }
}


#ifdef KERNEL

void
sp_change_chkfield(obj, chkfield)
objnum	obj;
port *	chkfield;
{
    sp_tab *	st;

    st = &_sp_table[obj];	/* Get pointer to root directory */
    if (!sp_in_use(st))
	panic("sp_change_chkfield: kernel root directory is missing");
    st->st_dir->sd_random = *chkfield;
}

#endif
