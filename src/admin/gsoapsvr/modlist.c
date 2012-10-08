/*	@(#)modlist.c	1.1	96/02/27 10:02:48 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amoeba.h"
#include "proc.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "monitor.h"
#include "module/mutex.h"
#include "module/buffers.h"
#include "module/stdcmd.h"
#include "machdep.h"
#include "thread.h"
#include "disk.h"

#include "global.h"
#include "main.h"
#include "sp_print.h"
#include "filesvr.h"
#include "sp_impl.h"
#include "superfile.h"
#include "dirmarshall.h"
#include "dirfile.h"
#include "caching.h"
#include "seqno.h"
#include "misc.h"
#include "moddisk.h"
#include "modnvram.h"
#include "modlist.h"

/* from sp_impl.c: */
extern errstat sp_add_row();
extern errstat sp_del_row();
extern int     sp_search_row();
extern char   *sp_alloc();
extern char   *sp_str_copy();

extern sp_tab *_sp_table;
extern port    my_super_getport;

/* debugging */
#define DEBUG_LEVEL	1
#define dbmessage(lev, list) { if (lev <= DEBUG_LEVEL) { message list; } }

extern int Debugging;

struct ml_backend {
    char *     mbe_descr;
    char *     mbe_info;
    char *   (*mbe_get_buf)    _ARGS((void));
    errstat  (*mbe_writeblock) _ARGS((disk_addr block));
    errstat  (*mbe_readblock)  _ARGS((disk_addr block, char **bufp));
    errstat  (*mbe_init)       _ARGS((capability *svrcap, disk_addr firstblk,
				      disk_addr *nblocks, unsigned int *size));
    errstat  (*mbe_format)     _ARGS((capability *svrcap, disk_addr firstblk,
				      disk_addr nblocks));
};

static struct ml_backend ml_backends[] = {
    { "moddisk", "@",               md_get_buf,   md_writeblock,
       md_readblock,   md_init,     NULL        		},
    { "novram",  "in-core segment", novr_get_buf, novr_writeblock,
       novr_readblock, novr_init,   novr_format 		},
    { NULL,      NULL,	            NULL,         NULL,
      NULL,      NULL,		    NULL			}
};

static struct ml_backend *Backend = NULL;

/*
 * This module implements a store of directory modifications.
 * This avoids having to write entiry directories disk immediately.
 * Note that this module is only an interface to another module doing
 * the actual storage.  That module could just write modifications
 * atomically to disk, or (better yet) it could use non-volatile memory
 * if that is present.
 *
 * The modifications are stored in a double linked list.
 * The modifications to one object should be written to disk in one
 * atomic operation, because we use a sequence number per object. This
 * sequence number is used during recovery to check if the modifications
 * stored in the modifications list already have been done.
 */

#define MOD_STACKSIZE	(2*8192)
#define MOD_TIME	30
#define MODS_PER_BLOCK	10	/* estimation of #mods per block */

/* Structure to store modifications.
 * We cannot use rownum to identify a row, because the soap server
 * may move rows around.
 */
struct sp_mod {
    command	   mod_command;   /* The type of modification */
    char	  *mod_name;	  /* name of affected row */
    sp_row  	   mod_row;	  /* new row, if doing delete, replace */
    struct sp_mod *mod_next;	  /* Pointer to next in list */
    struct sp_mod *mod_prev;	  /* Pointer to previous in list */
};
typedef struct sp_mod *mod_p;

/* Structure combining modifications stored on the same block.
 * Note that the current sequence number for an object is stored per block!
 * That way we can determine the required order of modifications during
 * recovery.
 */
typedef struct mod_block *mod_block_p;
struct mod_block {
    disk_addr	   mb_blocknr;    /* block on which the mods are stored    */
    sp_seqno_t	   mb_seqno;      /* dir sequence number for this block    */
    int16	   mb_size;	  /* marshalled size of mods in this block */
    int16	   mb_nmods;	  /* number of mods in this block          */
    mod_p	   mb_mods;       /* double linked list of modifications   */
    mod_p	   mb_lastmod;
    mod_block_p    mb_older;	  /* older mod for same obj */
    mod_block_p    mb_newer;	  /* newer mod for same obj */
};

/*
 * Structure combining modifications for the same object.
 */
typedef struct mod_obj *mod_obj_p;
struct mod_obj {
    objnum	   om_objnum;     /* object number              */
    mod_block_p    om_firstblock; /* newest block for this object */
    mod_block_p    om_lastblock;  /* oldest block list for this object */
    mod_obj_p      om_next;	  /* next on object list        */
    mod_obj_p      om_prev;	  /* prev on object list        */
    int            om_safetoadd;  /* number of modifications we can make
				   * without having to check that the
				   * resulting directory fits in the
				   * (static) buffer of the dirfile module.
				   */
};

static mod_obj_p   Mod_list;	  /* the modifications list */

static mod_p       Mod_free_mod;   /* freelist */
static mod_block_p Mod_free_block; /* freelist */
static mod_obj_p   Mod_free_obj;   /* freelist */

static unsigned  Mod_bufsize;	/* size of mod list buffers   */
static disk_addr Mod_nblocks;	/* number of mod list buffers */

static int Mod_use;		/* use mod to store modifications	*/
static int Mod_nmods;		/* number of pending modifications	*/
static int Mod_changed;		/* mod changed in last Mod_time sec?	*/
static int Mod_bullet_down;	/* backing store down?			*/
static int Mod_time;		/* modlist flush time			*/

/* 
 * Entry points to get or print modlist status
 */

int ml_in_use()
{
    return Mod_use;
}

int ml_nmods()
{
    return Mod_nmods;
}

int ml_time()
{
    return Mod_time;
}

void ml_print()
{
    mod_obj_p om;

    message("=========== modification list ==========");
    for (om = Mod_list; om != NULL; om = om->om_next) {
	mod_block_p mb;

	message("directory %ld", om->om_objnum);
	for (mb = om->om_lastblock; mb != NULL; mb = mb->mb_newer) {
	    mod_p sm;
	
	    message("block %d; seqno (%ld, %ld)",
		    mb->mb_blocknr, hseq(&mb->mb_seqno), lseq(&mb->mb_seqno));

	    for (sm = mb->mb_mods; sm != NULL; sm = sm->mod_next) {
		message("%s %s", cmd_name(sm->mod_command), sm->mod_name); 
	    }
	}
	message("-----------");
    }
    message("========================================");
}

/*
 * Append and delete modifications from the modification list.
 */

static mod_p
mod_alloc()
{
    mod_p sm = NULL;

    /* try to pick one from the free list, if available */
    if (Mod_free_mod != NULL) {
	sm = Mod_free_mod;
	Mod_free_mod = Mod_free_mod->mod_next;
    } else {
	/* try to allocate a new one */
	sm = (mod_p) malloc(sizeof(struct sp_mod));
    }

    if (sm != NULL) {
	sm->mod_command = 0;
	sm->mod_next = NULL;
	sm->mod_prev = NULL;
	sm->mod_name = NULL;
    }

    return sm;
}

static void
mod_free(sm)
mod_p sm;
{
    assert(sm != NULL);
    
    if (sm->mod_name != NULL) {
	free((_VOIDSTAR) sm->mod_name);
    }

    switch (sm->mod_command) {
    case SP_APPEND:
    case SP_REPLACE:
	free_row(&sm->mod_row);
	break;
    case SP_DELETE:
    default:
	break;
    }

    sm->mod_next = Mod_free_mod;
    Mod_free_mod = sm;
}

/* The modifications types that are stored in the mod list.
 * SP_REPLACE also covers SP_PUTTYPE and SP_CHMOD.
 */
#define MOD_CMD(c) ((c) == SP_APPEND || (c) == SP_REPLACE || (c) == SP_DELETE)

static char *
buf_put_mod(buf, end, sm)
char *buf;
char *end;
mod_p sm;
{
    extern char *sp_buf_put_row();
    char *p;

    p = buf;
    p = buf_put_int16(p, end, (int16) sm->mod_command);
    if (sm->mod_command == SP_REPLACE || sm->mod_command == SP_APPEND) {
	/* buf_put_row also includes the row name */
	p = sp_buf_put_row(p, end, 1, &sm->mod_row, SP_MAXCOLUMNS);
    } else {
	assert(sm->mod_command == SP_DELETE);
	p = buf_put_string(p, end, sm->mod_name);
    }
    return p;
}


/*
 * Append and delete objects from the object list.
 */

static mod_obj_p
obj_alloc(obj)
objnum obj;
{
    mod_obj_p om = NULL;

    if (Mod_free_obj != NULL) {
	om = Mod_free_obj;
	Mod_free_obj = Mod_free_obj->om_next;

	om->om_objnum = obj;
	om->om_firstblock = om->om_lastblock = NULL;
	om->om_prev = om->om_next = NULL;
	om->om_safetoadd = 0;	/* will be re-initialised when needed */
    }

    return om;
}

static void
obj_free(om)
mod_obj_p om;
{
    om->om_next = Mod_free_obj;
    Mod_free_obj = om;
}

static mod_obj_p
obj_find(obj)
objnum obj;
{
    mod_obj_p om;

    for (om = Mod_list; om != NULL; om = om->om_next) {
	if (om->om_objnum == obj) {
	    return om;
	}
    }

    return NULL;
}

static void
obj_insert(om)
mod_obj_p om;
{
    om->om_prev = NULL;
    om->om_next = Mod_list;
    if (om->om_next != NULL) {
	om->om_next->om_prev = om;
    }
    Mod_list = om;
}

static void
obj_delete(om)
mod_obj_p om;
{
    if (om->om_next != NULL) {		/* not the last entry */
	om->om_next->om_prev = om->om_prev;
    }
    if (om->om_prev == NULL) {		/* at the front of the mod list */
	Mod_list = om->om_next;
    } else {
	om->om_prev->om_next = om->om_next;
    }

    obj_free(om);
}

static void
block_free(mb)
mod_block_p mb;
{
    mb->mb_older = Mod_free_block;
    Mod_free_block = mb;
}

static mod_block_p
block_alloc(block)
disk_addr block;
{
    mod_block_p mb = NULL;
    mod_block_p *prev = NULL;

    /* caller wants to allocate a certain block; let's see */
    for (prev = &Mod_free_block, mb = Mod_free_block;
	 mb != NULL;
	 prev = &mb->mb_older, mb = mb->mb_older)
    {
	if (mb->mb_blocknr == block) { /* found it */
	    /* remove it from the free list */
	    *prev = mb->mb_older;
	    break;
	}
    }

    if (mb != NULL) {
	zero_seq_no(&mb->mb_seqno);
	mb->mb_size = -1;
	mb->mb_nmods = 0;
	mb->mb_mods = mb->mb_lastmod = NULL;
	mb->mb_newer = mb->mb_older = NULL;
    } else {
	scream("block_alloc: block %ld not available", (long) block);
    }

    return mb;
}

static void
block_delete_mod(mb, sm)
mod_block_p mb;
mod_p  sm;
{
    /* Remove entry from the doubled linked modification list. */
    assert(mb != NULL);
    assert(sm != NULL);

    if (sm->mod_next == NULL) {	/* last entry */
	mb->mb_lastmod = sm->mod_prev;
    } else {
	sm->mod_next->mod_prev = sm->mod_prev;
    }

    if (sm->mod_prev == NULL) {	/* the front of the list */
	mb->mb_mods = sm->mod_next;
    } else {
	sm->mod_prev->mod_next = sm->mod_next;
    }

    mod_free(sm);

    mb->mb_nmods--;
    Mod_nmods--;
    mb->mb_size = -1; /* no longer valid */
}

static void
block_del_modlist(mb)
mod_block_p mb;
{
    mod_p       sm, next_sm;

    next_sm = NULL;
    for (sm = mb->mb_mods; sm != NULL; sm = next_sm) {
	next_sm = sm->mod_next;

	block_delete_mod(mb, sm);
    }
}

static void
obj_delete_firstblock(om)
mod_obj_p   om;
{
    mod_block_p mb = om->om_firstblock;

    assert(mb != NULL);
    if (mb->mb_older != NULL) {
	mb->mb_older->mb_newer = NULL;
    } else {
	om->om_lastblock = NULL;
    }
    om->om_firstblock = mb->mb_older;
}

static void
obj_do_insert_block(om, mb, before)
mod_obj_p   om;
mod_block_p mb;
mod_block_p before;
{
    mb->mb_newer = (before != NULL) ? before->mb_newer : om->om_lastblock;
    mb->mb_older = before;
    if (mb->mb_newer == NULL) {
	om->om_firstblock = mb;
    } else {
	mb->mb_newer->mb_older = mb;
    }
    if (before != NULL) {
	before->mb_newer = mb;
    } else {
	om->om_lastblock = mb;
    }
}

static mod_block_p
obj_add_block(om)
mod_obj_p om;
{
    mod_block_p mb = NULL;

    if (Mod_free_block == NULL) {
	/* This shouldn't happen; the caller should have taken care
	 * there was an extra block available (by flushing if needed).
	 */
	scream("obj_add_block: all blocks in use");
    } else {
	/* any free block will do */
	mb = Mod_free_block;
	Mod_free_block = Mod_free_block->mb_older;

	mb->mb_nmods = 0;
	mb->mb_size = -1; /* unknown? */
	mb->mb_mods = mb->mb_lastmod = NULL;
	mb->mb_newer = mb->mb_older = NULL;

	obj_do_insert_block(om, mb, om->om_firstblock);
    }

    return mb;
}

static void
obj_insert_block(om, mb)
mod_obj_p   om;
mod_block_p mb;
{
    mod_block_p before;

    /* find block where to insert it before */
    for (before = om->om_firstblock;
	 before != NULL;
	 before = before->mb_older)
    {
	if (less_seq_no(&before->mb_seqno, &mb->mb_seqno)) {
	    break;
	}
    }

    if (before == NULL) {
	/* new last block */
	dbmessage(2, ("add block with seqno (%ld, %ld)",
		      hseq(&mb->mb_seqno), lseq(&mb->mb_seqno)));
    } else {
	/* put it before 'before' */
	dbmessage(2, ("put block with seqno (%ld, %ld) before (%ld, %ld)",
		      hseq(&mb->mb_seqno), lseq(&mb->mb_seqno),
		      hseq(&before->mb_seqno), lseq(&before->mb_seqno)));
    }

    obj_do_insert_block(om, mb, before);
}

static void
block_add_mod(mb, sm)
mod_block_p mb;
mod_p       sm;
{
    sm->mod_next = NULL;
    sm->mod_prev = mb->mb_lastmod;

    if (mb->mb_mods == NULL) {
	mb->mb_mods = sm;
    } else {
	assert(mb->mb_lastmod->mod_next == NULL);
	mb->mb_lastmod->mod_next = sm;
    }

    mb->mb_lastmod = sm;

    Mod_nmods++;
    mb->mb_nmods++;
    mb->mb_size = -1; /* no longer valid */
    Mod_changed = 1;
}


static int block_modsize();

static errstat
obj_append_mod(om, sm)
mod_obj_p om;
mod_p     sm;
{
    mod_block_p mb = NULL;

    /* Append entry to the double linked list. */
    assert(om != NULL);
    assert(sm != NULL);

    /* We must first determine whether we can add this mod to the last
     * block for this object, or if we need a new block.
     */
    if (om->om_firstblock == NULL) {
	/* no blocks yet */
	mb = obj_add_block(om);
    } else {
	/* Check whether there is room for this extra mod by marshalling
	 * it into a buffer.
	 */
	char *buf, *p;

	buf = (*Backend->mbe_get_buf)();
	p = buf + block_modsize(om, om->om_firstblock);
	p = buf_put_mod(p, buf + Mod_bufsize, sm);
	
	if (p != NULL) {
	    mb = om->om_firstblock;
	} else {
	    mb = obj_add_block(om);
	}
    }
	
    if (mb == NULL) {
	return STD_NOSPACE;
    }

    block_add_mod(mb, sm);

    dbmessage(2, ("%ld: %s %s stored on %ld (now contains %d mods)",
		  (long) om->om_objnum, cmd_name(sm->mod_command),
		  sm->mod_name,	(long) mb->mb_blocknr, mb->mb_nmods));

    return STD_OK;
}

/*
 * marshalling and unmarshalling of modlist blocks
 */

static char *
buf_put_modlist(buf, end, om, mb, hasmods)
char       *buf;
char       *end;
mod_obj_p   om;
mod_block_p mb;
int	    hasmods;
{
    mod_p sm;
    char *p;

    p = buf;

    /* Note: if 'hasmods' is 1, write the number of mods increased by 1
     * so that we can tell the difference between a cleared modlist block,
     * and one that just mentiones the new sequence number of the directory.
     * This arises when, e.g., two canceling modifications are applied.
     */
    if (hasmods) {
	p = buf_put_int16 (p, end, mb->mb_nmods + 1);
    } else {
	assert(mb->mb_nmods == 0);
	p = buf_put_int16 (p, end, 0);
    }
    p = buf_put_objnum(p, end, om->om_objnum);
    p = buf_put_seqno (p, end, &mb->mb_seqno);
    for (sm = mb->mb_mods; sm != NULL; sm = sm->mod_next) {
	p = buf_put_mod(p, end, sm);
    }
    return p;
}

#define Failure(err)	{ failure_err = err; goto failure; }

static char *
buf_get_mod(buf, end, smp)
char  *buf;
char  *end;
mod_p *smp;
{
    extern char *sp_buf_get_row();
    mod_p   sm = NULL;
    int16   cmd;
    char   *name, *alloc_name = NULL;
    char   *p;
    errstat err, failure_err;

    if ((sm = mod_alloc()) == NULL) {
	Failure(STD_NOSPACE);
    }

    p = buf;
    if ((p = buf_get_int16(p, end, &cmd)) == NULL) {
	Failure(STD_SYSERR);
    }
    if (!MOD_CMD(cmd)) {
	scream("bad command %d in mod list", cmd);
	Failure(STD_COMBAD);
    }
    sm->mod_command = cmd;

    if (cmd == SP_REPLACE || cmd == SP_APPEND) {
	p = sp_buf_get_row(p, end, 1, &sm->mod_row, SP_MAXCOLUMNS, &err);
	if (p == NULL) {
	    Failure(err);
	}
	name = sm->mod_row.sr_name;
    } else {
	assert(cmd == SP_DELETE);
	p = buf_get_string(p, end, &name);
	if (p == NULL) {
	    Failure(STD_SYSERR);
	}
    }

    if ((alloc_name = sp_str_copy(name)) == NULL) {
	Failure(STD_NOSPACE);
    }

    /* success */
    sm->mod_name = alloc_name;
    *smp = sm;
    return p;

failure:
    scream("buf_get_mod failed (%s)", err_why(failure_err));
    if (sm != NULL) {
	sm->mod_command = 0;
	mod_free(sm);
    }
    if (alloc_name != NULL) {
	free((_VOIDSTAR) alloc_name);
    }
    return NULL;
}

static char *
buf_get_modlist(buf, end, mb)
char       *buf;
char       *end;
mod_block_p mb;
{
    objnum     obj;
    mod_obj_p  om = NULL;
    char      *p;
    int16      nmods;        

    p = buf;
    p = buf_get_int16(p, end, &nmods);

    if (nmods > 0) {
	/* The nmods on disk is one higher than the actual number of mods: */
	nmods--;

	p = buf_get_objnum(p, end, &obj);
	p = buf_get_seqno (p, end, &mb->mb_seqno);

	if (p != NULL) {
	    dbmessage(2, ("block %d contains %d mods for dir %ld",
			  mb->mb_blocknr, (int) nmods, (long) obj));

	    if ((om = obj_find(obj)) == NULL) {
		if ((om = obj_alloc(obj)) != NULL) {
		    obj_insert(om);
		} else {
		    /* shouldn't happen; we've allocated enough */
		    panic("buf_get_modlist: cannot allocate obj");
		}
	    }

	    if (om != NULL) {
		mod_p sm;
		int   i;

		for (i = 0; i < nmods && p != NULL; i++) {
		    if ((p = buf_get_mod(p, end, &sm)) != NULL) {
			dbmessage(0, ("add mod %d: %s %s", i,
				     cmd_name(sm->mod_command), sm->mod_name));
			block_add_mod(mb, sm);
		    }
		}
	    }
	}
    }

    if (p == NULL) {
	scream("error unmarshalling modlist block %d", mb->mb_blocknr);
    }

    if (om != NULL) {
	obj_insert_block(om, mb);
    } else {
	block_free(mb);
    }

    return p;
}

static int
block_modsize(om, mb)
mod_obj_p   om;
mod_block_p mb;
{
    char *buf, *p;
    int size;

    /* Compute it by marshalling it into a buffer.
     * Optimisation: use cached mb_size field when valid.
     */
    if (mb->mb_size >= 0) {
	size = mb->mb_size;
    } else {
	MON_EVENT("block_modsize: compute size");

	buf = (*Backend->mbe_get_buf)();
	p = buf_put_modlist(buf, buf + Mod_bufsize, om, mb, 1);
	if (p == NULL) { /* didn't fit; shouldn't happen */
	    size = Mod_bufsize;
	} else {
	    size = p - buf;
	}
    }

    return size;
}


/* 
 * Routines to do the actually recovery from info stored in the modlist,
 * after a new server is started.
 */

static void
apply_mod(st, om, mb, sm)
sp_tab     *st;
mod_obj_p   om;
mod_block_p mb;
mod_p       sm;
{
    /* Perform modification to directory. */
    sp_row  oldrow, newrow;
    int     rownum;
    
    assert(om != NULL);
    assert(mb != NULL);
    assert(sm != NULL);
    assert(in_cache(om->om_objnum));

    dbmessage(2, ("apply `%s %s' to dir %ld seqno (%ld, %ld)",
		  cmd_name(sm->mod_command), sm->mod_name, (long)om->om_objnum,
		  hseq(&mb->mb_seqno), lseq(&mb->mb_seqno)));

    switch(sm->mod_command) {
    case SP_APPEND:
	MON_EVENT("apply_mod: sp_append");
	if (DEBUG_LEVEL >= 2) {
	    print_row(&sm->mod_row, st->st_dir->sd_ncolumns);
	}
	if (!copy_row(&newrow, &sm->mod_row)) {
	    panic("apply_mod: couldn't copy row");
	}
	if (sp_add_row(st, &newrow, st->st_dir->sd_nrows) != STD_OK) {
	    panic("apply_mod: couldn't add row");
	}
	break;

    case SP_DELETE:
	MON_EVENT("apply_mod: sp_delete");
	assert(st->st_dir->sd_rows != NULL);
	rownum = sp_search_row(st->st_dir, sm->mod_name);
	assert(rownum >= 0);

	if (sp_del_row(st, rownum) != STD_OK) {
	    panic("apply_mod: couldn't delete row");
	}
	break;

    case SP_REPLACE:
	MON_EVENT("apply_mod: sp_replace");
	if (DEBUG_LEVEL >= 2) {
	    print_row(&sm->mod_row, st->st_dir->sd_ncolumns);
	}
	if (!copy_row(&newrow, &sm->mod_row)) {
	    panic("apply_mod: couldn't copy row");
	}

	assert(st->st_dir->sd_rows != NULL);
	rownum = sp_search_row(st->st_dir, sm->mod_name);
	assert(rownum >= 0);

	oldrow = *st->st_dir->sd_rows[rownum];
	*st->st_dir->sd_rows[rownum] = newrow;
	free_row(&oldrow); 				/* free old row */
	break;

    default:
	panic("apply_mod: unknown command");
	break;
    }
}

/* 
 * Routines to store modifications in the mod list.
 */

static void
check_and_remove_mods(om)
mod_obj_p om;
{
    /* Check the the modification list to see if any operation is canceled
     * by the last appended modification.
     */
    mod_p sm;
    mod_p prev;
    mod_p new;

    assert(om != NULL);
    assert(om->om_firstblock != NULL);
    assert(om->om_firstblock->mb_lastmod != NULL); /* the one just added */
    
    new = om->om_firstblock->mb_lastmod;

    switch (new->mod_command) {
    case SP_DELETE:
	/* Remove previous SP_APPEND and SP_REPLACEs for the same row name.
	 * In case of SP_APPEND, the SP_DELETE must be removed as well.
	 */
	prev = NULL;
	for (sm = new->mod_prev; sm != NULL; sm = prev) {
	    command mod_cmd = sm->mod_command;

	    prev = sm->mod_prev;
	    if (strcmp(sm->mod_name, new->mod_name) == 0) {
		/* It is not possible to have two SP_DELETEs following
		 * each other, so:
		 */
		assert(mod_cmd == SP_APPEND || mod_cmd == SP_REPLACE);

		MON_EVENT("check_and_remove_mod: cancel row operation");
		block_delete_mod(om->om_firstblock, sm);

		if (mod_cmd == SP_APPEND) {
		    /* remove the SP_DELETE modification */
		    MON_EVENT("check_and_remove_mod: cancel sp_delete");
		    block_delete_mod(om->om_firstblock, new);
		}

		break; /* Don't go further along the list! */
	    }
	}
	break;
    case SP_REPLACE:
	/* In this case we can remove corresponding previous SP_REPLACEs 
	 * and we can modify a corresponding previous SP_APPEND.
	 */
	prev = NULL;
	for (sm = new->mod_prev; sm != NULL; sm = prev) {
	    command mod_cmd = sm->mod_command;

	    prev = sm->mod_prev;
	    if (strcmp(sm->mod_name, new->mod_name) == 0) {
		if (mod_cmd == SP_REPLACE) {
		    MON_EVENT("check_and_remove_mod: cancel sp_replace");
		    block_delete_mod(om->om_firstblock, sm);
		} else {
		    sp_row temp_row;

		    assert(mod_cmd == SP_APPEND);
		    MON_EVENT("check_and_remove_mod: modify sp_append");

		    temp_row = new->mod_row;
		    new->mod_row = sm->mod_row;
		    sm->mod_row = temp_row;

		    block_delete_mod(om->om_firstblock, new);
		}

		break; /* Don't go further along the list! */
	    }
	}
	break;
    case SP_APPEND:
	/* We could try to remove a corresponding SP_DELETE,
	 * and change the new command to be SP_REPLACE.
	 */
	break;
    default:
	panic("check_and_remove_mods: unknown command");
    }
}

static errstat
mod_write(om, mb, hasmods)
mod_obj_p   om;
mod_block_p mb;
int         hasmods;
{
    /* Write out a modifications block */
    char     *buf, *end, *p;
    disk_addr blocknr = mb->mb_blocknr;
    errstat   err;

    buf = (*Backend->mbe_get_buf)();
    end = buf + Mod_bufsize;

    p = buf_put_modlist(buf, end, om, mb, hasmods);
    if (p == NULL) {
	/* Append_mod() must have made sure the mod can be added to the
	 * block, so when we get a marshalling error here, it's a bug.
	 */
	panic("ml_store: modlist marshalling error");
    }

    /* Save the size so that we don't have to recompute it when
     * we want to add another modification to this block.
     */
    mb->mb_size = (p - buf);

    if ((err = (*Backend->mbe_writeblock)(blocknr)) != STD_OK) {
	scream("cannot write modlist block %d (%s)", blocknr, err_why(err));

	/* But maybe the bullet server is still up.
	 * In that case it is nonfatal;  we should just refrain from
	 * using the mod list until the server becomes available again.
	 */
    }

    return err;
}

/* 
 * The main entry points for this module:
 *	ml_init:    initialise this module
 *	ml_recover: recovers modifications stored in mod;
 * 	ml_use:     re-initializes the mod list; 
 *	ml_remove:  remove modifications from the mod list
 *	ml_flush:   flush all modifications to disk;
 *	ml_store:   store modification in the mod list.
 */

int
ml_has_mods(obj)
objnum obj;
/*
 * Allows the caching module to find out whether it may cache out a
 * certain directory.
 */
{
    return obj_find(obj) != NULL;
}

errstat
ml_store(obj)
objnum obj;
{
    /* Add the modifications for st to the modlist. */
    struct sp_save *undo;
    sp_tab   *st;
    mod_p     sm = NULL;
    mod_obj_p om = NULL;
    short     rownum;
    int       om_allocated = 0;
    errstat   failure_err;

    st = &_sp_table[obj];
    undo = st->st_dir->sd_mods;

    if (!Mod_use || undo == NULL ||
	!MOD_CMD(undo->ss_command) || (undo->ss_next != NULL))
    {
	/* not applicable, or a list of multiple updates (SP_INSTALL) */
	return STD_COMBAD;
    }

    /* Make sure we have a spare extra block, in case the modification
     * cannot be added to an existing block.
     * We do it here, because that's the safest thing to do.
     */
    if (Mod_free_block == NULL) {
	dbmessage(1, ("ml_store: flush to get free block"));
	ml_flush();
    }

    om = obj_find(obj);
    if (om == NULL) {
	if ((om = obj_alloc(obj)) == NULL) {
	    scream("ml_store: cannot allocate obj structure");
	    Failure(STD_NOMEM);
	} else {
	    om_allocated = 1;
	}
    }

    /* Now see if the operation doesn't make the directory too big!
     * Eventually we will create a bullet directory file for it, and they
     * have a maximum size currently.
     */
    if (undo->ss_command != SP_DELETE) {
	/* Marshalling a very big directory is dreadfully slow,
	 * so must we try to avoid that whenever possible.
	 */
	if (om->om_safetoadd == 0) {
	    om->om_safetoadd = dirf_safe_rows(obj);
	}

	if (om->om_safetoadd == 0) {
	    scream("directory %ld too big to allow update", (long) obj);
	    Failure(STD_NOSPACE); /* note: different from STD_NOMEM */
	}

	/* now subtract one for the modification we're going to add */
	om->om_safetoadd--;
    }

    if ((sm = mod_alloc()) == NULL) {
	scream("ml_store: cannot allocate mod structure");
	Failure(STD_NOMEM);
    }
    sm->mod_name = NULL;
    sm->mod_command = undo->ss_command;

    switch (sm->mod_command) {
    case SP_APPEND:
	MON_EVENT("ml_store: sp_append");
	rownum = st->st_dir->sd_nrows - 1;
	sm->mod_name = sp_str_copy(st->st_dir->sd_rows[rownum]->sr_name);
	if (sm->mod_name == NULL) {
	    Failure(STD_NOMEM);
	}
	if (!copy_row(&sm->mod_row, st->st_dir->sd_rows[rownum])) { 
	    Failure(STD_NOMEM);
	}
	break;

    case SP_DELETE:
	MON_EVENT("ml_store: sp_delete");
	sm->mod_name = sp_str_copy(undo->ss_oldrow.sr_name);
	if (sm->mod_name == NULL) {
	    Failure(STD_NOMEM);
	}
	break;

    case SP_REPLACE:	/* covers also SP_PUTTYPE and SP_CHMOD */
	MON_EVENT("ml_store: sp_replace");
	rownum = undo->ss_rownum;
	sm->mod_name = sp_str_copy(st->st_dir->sd_rows[rownum]->sr_name);
	if (sm->mod_name == NULL) {
	    Failure(STD_NOMEM);
	}
	if (!copy_row(&sm->mod_row, st->st_dir->sd_rows[rownum])) {
	    Failure(STD_NOMEM);
	}
	break;

    default:
	panic("ml_store: unknown command");
	break;
    }

    if (obj_append_mod(om, sm) != STD_OK) {
	scream("ml_store: cannot append mod to obj structure");
	Failure(STD_NOMEM);
    }

    /* Update the sequence number of the modification block */
    om->om_firstblock->mb_seqno = st->st_dir->sd_seq_no;

    if (om_allocated) {
	obj_insert(om);
    } else {
	/* maybe this modification cancels a previous one: */
	check_and_remove_mods(om);
    }

    /* And finally, write out the modifications block which we have changed */
    return mod_write(om, om->om_firstblock, 1);

failure:
    scream("ml_store: no room to store modification");
    if (om != NULL && om_allocated) {
	obj_free(om);
    }
    if (sm != NULL) {
	mod_free(sm);
    }
    return failure_err;
}

static void
ml_do_remove(obj, writeit)
objnum obj;
int writeit;
{
    /* The object specified has gone to disk; remove pending modifications. */

    mod_obj_p   om;
    mod_block_p mb;

    if ((om = obj_find(obj)) == NULL) {
	/* no modifications stored for this dir */
	return;
    }
    
    dbmessage(2, ("ml_do_remove(%ld, %d)", (long) obj, writeit));
    for (mb = om->om_firstblock; mb != NULL; mb = om->om_firstblock) {
	MON_EVENT("ml_remove: remove modifications block");
	obj_delete_firstblock(om);
	block_del_modlist(mb);

	if (writeit) {
	    /* The cleared modifications block is no longer needed (the
	     * modifications it contained are already incorporated into
	     * the directory file).  But to avoid unnecessary disk_write()s,
	     * only do it when explicitly told so.
	     */
	    dbmessage(2, ("CLEAR modlist block %ld", (long) mb->mb_blocknr));
	    (void) mod_write(om, mb, 0);
	}

	block_free(mb);
    }

    obj_delete(om);
}

void
ml_remove(obj)
objnum obj;
{
    ml_do_remove(obj, 0);
}

errstat
write_new_dir(obj, modify)
objnum obj;
int    modify;
{
    capability curcaps[NBULLETS];
    capability newcaps[NBULLETS];
    capability filesvrs[NBULLETS];
    errstat err;

    /* all objects for which we have modifications should be in memory: */
    assert(in_cache(obj));
    
    get_dir_caps(obj, curcaps);
    fsvr_get_svrs(filesvrs, NBULLETS);

    if (modify) {
	err = dirf_modify(obj, NBULLETS, curcaps, newcaps, filesvrs);
    } else {
	err = dirf_write(obj, NBULLETS, newcaps, filesvrs);
    }
	
    if (err == STD_OK) {
	int i;

	set_dir_caps(obj, newcaps);

	/* Commit right away; there could be several changed directories. */
	MON_EVENT("wrote new dir");
	write_blocks_modified();

	/* We can only remove the originals when we are sure we didn't
	 * inherit an old-style directory partition with shared caps.
	 * Fortunately, we can detect this: both capabilities will be present
	 * in that case.
	 */
	if (!NULLPORT(&curcaps[0].cap_port) &&
	    !NULLPORT(&curcaps[1].cap_port))
	{
	    dbmessage(1, ("don't destroy caps for directory %ld", (long) obj));
	} 
	else
	{
	    /* Throw the orginals away, in case they are different */
	    for (i = 0; i < NBULLETS; i++) {
		if (!NULLPORT(&curcaps[i].cap_port) &&
		    !cap_cmp(&curcaps[i], &newcaps[i]))
		{
		    destroy_file(&curcaps[i]);
		}
	    }
	}
    } else {
	scream("write_new_dir: dirf_%s failed for dir %ld (%s)",
	       (modify ? "modify" : "write"), (long) obj, err_why(err));
    }

    Mod_bullet_down = (err != STD_OK);
    return err;
}

errstat
ml_write_dir(obj)
objnum obj;
{
    errstat err;

    if ((err = write_new_dir(obj, 0)) == STD_OK) {
	/* update mod list */
	ml_remove(obj);
    }

    return err;
}

errstat
ml_flush()
{
    /* Flush modifications. */
    mod_obj_p om, om_next;
    errstat err;
    
    if (!Mod_use) {
	return STD_NOTNOW;
    }
    
    MON_EVENT("ml_flush");
    
    om_next = NULL;
    for (om = Mod_list; om != NULL; om = om_next) {
	om_next = om->om_next;

	if ((err = write_new_dir(om->om_objnum, 0)) != STD_OK) {
	    /* bullet server down temporarily? */
	    return err;
	}

	ml_do_remove(om->om_objnum, 1);
    }

    return STD_OK;
}

static void
cleaner_thread(param, paramsize)
char *param;
int paramsize;
/*ARGSUSED*/
{
    /* Check every Mod_time if mod has been used.
     * If not, write the modified directories to disk. 
     * If the backstore is not accessible, check if it is back again.
     */
    for (;;) {
	Mod_changed = 0;
	sleep((unsigned) Mod_time);

	if (Mod_bullet_down) {
	    fsvr_check_svrs();
	    Mod_bullet_down = (fsvr_nr_avail() < 1);
	}

	if (!Mod_bullet_down && !Mod_changed) {
	    MON_EVENT("modlist cleaner: ml_flush");
	    sp_begin();
	    ml_flush();
	    sp_end();
	}
    }
}


static errstat
mod_read_mods(seqno)
sp_seqno_t *seqno;
{
    /*
     * For recovery, we must fetch the modlist and apply the modifications
     * that weren't performed yet.  This can be determined by means of the
     * sequence numbers kept for each directory.
     */
    disk_addr block;
    errstat   err, retval = STD_OK;

    zero_seq_no(seqno);
    for (block = 1; block <= Mod_nblocks; block++) {
	mod_block_p mb;
	char *buf, *end;

	if ((mb = block_alloc(block)) == NULL) {
	    panic("could not allocate block %d", block);
	}

	if ((err = (*Backend->mbe_readblock)(block, &buf)) != STD_OK) {
	    scream("cannot read block %d", block);
	    retval = err;
	    continue;
	}

	end = buf + Mod_bufsize;

	if (buf_get_modlist(buf, end, mb) == NULL) {
	    scream("error parsing modlist block %d", block);
	    retval = STD_SYSERR;
	}

	if (less_seq_no(seqno, &mb->mb_seqno)) {
	    *seqno = mb->mb_seqno;
	    dbmessage(2, ("block %d has higher seqno (%ld, %ld)",
			  block, hseq(seqno), lseq(seqno)));
	}
    }

    return retval;
}

void
ml_recover(seqno)
sp_seqno_t *seqno;
{
    /*
     * Check for outstanding modifications and perform them.
     */
    mod_obj_p om, next_om;
    errstat   err;

    assert(!Mod_use);

    if ((err = mod_read_mods(seqno)) != STD_OK) {
	scream("could not recover all mods");
    }

    message("ml_recover: check mod list for modifications");

    next_om = NULL;
    for (om = Mod_list; om != NULL; om = next_om) {
	sp_tab     *st;
	int         applied_mods;
	mod_block_p mb;
	mod_p       sm;

	next_om = om->om_next;
	if (bad_objnum(om->om_objnum)) {
	    panic("ml_recover: bad object number %ld", om->om_objnum);
	}

	st = &_sp_table[om->om_objnum];
	if (!sp_in_use(st)) {
	    /* In this case we encountered modlist entries for
	     * a directory that has been deleted later on.
	     */
	    message("ml_recover: ignoring updates for deleted dir %ld",
		    (long) om->om_objnum);
	    ml_do_remove(om->om_objnum, 1);
	    continue;
	}

	message("ml_recover: recover dir %ld", (long) om->om_objnum);
	if ((err = sp_refresh(om->om_objnum, SP_LOOKUP)) != STD_OK) {
	    /* Maybe the bullet server is down, or we lost a directory file.
	     * Currently just do the safest thing: panic().
	     * TODO: look for specific error codes and see if we can do
	     * something else in a few cases.
	     */
	    panic("ml_recover: refresh of %ld failed (%s)",
		  (long) om->om_objnum, err_why(err));
	} 

	assert(in_cache(om->om_objnum));

	/* Apply the mods (from lowest to highest) that are in a block
	 * with a higher sequence number the directory on disk.
	 */
	applied_mods = 0;
	for (mb = om->om_lastblock; mb != NULL; mb = mb->mb_newer) {
	    if (less_seq_no(&st->st_dir->sd_seq_no, &mb->mb_seqno)) {
		for (sm = mb->mb_mods; sm != NULL; sm = sm->mod_next) {
		    apply_mod(st, om, mb, sm);
		}

		/* Update sequence number */
		st->st_dir->sd_seq_no = mb->mb_seqno;
		applied_mods = 1;
	    } else {
		message("ml_recover: mod seq (%ld, %ld) <= dir seq (%ld, %ld)",
			hseq(&mb->mb_seqno), lseq(&mb->mb_seqno), 
			hseq(&st->st_dir->sd_seq_no),
			lseq(&st->st_dir->sd_seq_no));
	    }
	}

	if (applied_mods) {
	    /* Write the modified directory to bullet server */
	    if (write_new_dir(om->om_objnum, 0) != STD_OK) {
		panic("apply_mod: cannot write dir %ld", (long) om->om_objnum);
	    }
	}

	ml_do_remove(om->om_objnum, 1);
    }

    message("ml_recover: finished");
}

static int Mod_inited = 0;

void
ml_use()
{
    assert(!Mod_use);

    if (Mod_inited) {
	if (!thread_newthread(cleaner_thread, MOD_STACKSIZE, (char *)NULL, 0)){
	    scream("ml_use: thread_newthread failed");
	} else {
	    message("ml_use: cleaner started");
	    Mod_use = 1;
	}
    }
}

#define MOD_SERVER	"MOD_SERVER"
#define MOD_BLOCK	"MOD_BLOCK"

extern capability *getcap();

errstat
ml_init(autoformat, forcedformat)
int autoformat;
int forcedformat;
{
    capability *mod_server;
    char       *mod_block;
    disk_addr   firstblock;
    char        info[80];
    int         i, size;
    errstat     err;

    /* The modlist server capability and the starting block are temporarily
     * fetched from the environment.  Eventually, this info must be retrieved
     * from the superblock.
     */
    if ((mod_server = getcap(MOD_SERVER)) == NULL) {
	message("no capability `%s'; assuming no mod list", MOD_SERVER);
	return STD_SYSERR;
    }

    if ((mod_block = getenv(MOD_BLOCK)) == NULL) {
        message("no environment var `%s'; assuming no mod list", MOD_BLOCK);
        return STD_SYSERR;
    }

    firstblock = atoi(mod_block);
    message("assuming first modlist block at %ld", (long) firstblock);

    /* determine which backend to use by means of the result of std_info */
    err = std_info(mod_server, info, (int) sizeof(info), &size);
    if (err != STD_OK && err != STD_OVERFLOW) {
	scream("std_info on modlist cap failed (%s)", err_why(err));
	return err;
    }

    if (err == STD_OK && size < sizeof(info)) {
	info[size] = '\0';
    } else {
	info[sizeof(info) - 1] = '\0';
    }
    for (Backend = &ml_backends[0]; Backend->mbe_descr != NULL; Backend++) {
	message("trying backend `%s', with info `%s'", 
		Backend->mbe_descr, Backend->mbe_info);
	if (strncmp(Backend->mbe_info, info, strlen(Backend->mbe_info)) == 0) {
	    message("matches with `%s'", info);
	    break;
	}
    }

    if (Backend->mbe_descr == NULL) {
	scream("no modlist backend for server with info string `%s'", info);
	return err;
    }

    if (forcedformat) {
	if (Backend->mbe_format != NULL) {
	    /* Let the module itself think up a suitable size for the modlist*/
	    message("formatting modlist");
	    err = (*Backend->mbe_format)(mod_server, firstblock, (disk_addr)0);
	    if (err != STD_OK) {	
		scream("modlist format failed (%s)", err_why(err));
		return err;
	    }
	} else {
	    scream("no format entrypoint for this kind of modlist");
	    return STD_NOTFOUND;
	}
    }

    /* initialise the backend */
    err = (*Backend->mbe_init)(mod_server, firstblock,
			       &Mod_nblocks, &Mod_bufsize);

    /* maybe we have to auto-format it: */
    if (err == STD_SYSERR && autoformat && Backend->mbe_format != NULL) {
	message("auto formatting modlist");

	/* Let the module itself think up a suitable size for the modlist */
	err = (*Backend->mbe_format)(mod_server, firstblock, (disk_addr) 0);

	if (err == STD_OK) {	/* re-initialise the backend */
	    err = (*Backend->mbe_init)(mod_server, firstblock,
				       &Mod_nblocks, &Mod_bufsize);
	}
    }

    if (err != STD_OK) {
	message("ml_init: md_init failed (%s)", err_why(err));
	return err;
    }

    message("ml_init: use %d blocks of size %d", Mod_nblocks, Mod_bufsize);

    /*
     * Preallocate the structures, so that we don't run into problems
     * later on.
     */
    Mod_free_obj = NULL;
    for (i = 0; i < Mod_nblocks; i++) {
	mod_obj_p   om;

	if ((om = (mod_obj_p) malloc(sizeof(struct mod_obj))) == NULL) {
	    panic("cannot allocate mod_obj structure");
	}

	/* add it to the free list: */
	obj_free(om);
    }

    Mod_free_block = NULL; 
    for (i = 1; i <= Mod_nblocks; i++) {
	mod_block_p mb;

	if ((mb = (mod_block_p) malloc(sizeof(struct mod_block))) == NULL) {
	    panic("cannot allocate mod_block structure");
	}
	mb->mb_blocknr = i;

	/* add it to the free list: */
	block_free(mb);
    }

    Mod_free_mod = NULL;
    for (i = 0; i < Mod_nblocks * MODS_PER_BLOCK; i++) {
	mod_p sm;

	if ((sm = (mod_p) malloc(sizeof(struct sp_mod))) == NULL) {
	    panic("cannot allocate sp_mod structure");
	}
	sm->mod_command = 0;
	sm->mod_name = NULL;

	/* add it to the free list: */
	mod_free(sm);
    }

    Mod_time = MOD_TIME;
    Mod_inited = 1;

    return STD_OK;
}

