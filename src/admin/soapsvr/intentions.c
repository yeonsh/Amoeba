/*	@(#)intentions.c	1.5	96/02/27 10:22:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "module/buffers.h"
#include "module/cap.h"

#include "caching.h"
#include "global.h"
#include "filesvr.h"
#include "sp_impl.h"
#include "misc.h"
#include "superfile.h"
#include "main.h"
#include "dirfile.h"
#include "intentions.h"
#include "dirmarshall.h"
#include "seqno.h"

/*
 * forward declarations
 */
void intent_rollforward();

/*
 * Number of first intention that has not been rolled forward:
 */
static int Intent_first = 0;

#ifndef SOAP_GROUP
/*
 * Set to 1 if the superblock contains (accepted) intentions received
 * from the other side.
 */
static int Received_intent = 0;
#endif

void
intent_make_permanent()
{
    /*
     * Make sure all intentions are done, switch to new versions of the changed
     * directories, and remove the intentions from block 0.
     */
    intent_rollforward();
    
    write_blocks_modified();

    if (super_nintents() > 0) {
	/* We have executed intentions, so now we have to update super block 0
	 * to ensure that the disk file knows that the intentions are gone.
	 */

	super_reset_intents(0);
	super_store_intents();
	MON_EVENT("cleared intentions");
    }

    Intent_first = 0;
}

void
intent_clear()
{
    /*
     * Invalidate the intentions from Intent_first onward
     */
    super_reset_intents(Intent_first);
}

int
intent_avail()
{
    return super_avail_intents();
}

void
intent_add(obj, filecaps)
objnum obj;
capability filecaps[NBULLETS];
{
    /* Add an intention (consisting of an array of filecaps containing
     * directory data) to the intensions list.
     * The caller should have checked (using intent_avail()) that there
     * is room for it, so we panic if super_add_intent() fails.
     */
    if (!super_add_intent(obj, filecaps)) {
	panic("cannot add intention for object %ld", obj);
    }
}

void
intent_rollforward()
{
    /* Here the intentions, as stored safely in block 0, are executed. */
    int do_destroy;

#ifndef SOAP_GROUP
    /* Intentions are generated either by me or by the other server.  In the
     * second case I first have to read the new directories from the Bullet
     * server.
     */
    int from_other_side;

    from_other_side = Received_intent;
    if (Received_intent) {		/* got intention from other side */
	super_store_intents();		/* first save the intention */
	Received_intent = 0;
    }

    if (Intent_first < super_nintents()) {
	MON_EVENT("execute intentions");
    }

    /* Only destroy the original replica's when we are fully recovered
     * (if not, we're executing the intentions from the superblock,
     * and that might contain multiple intentions for a single directory).
     * In two-copy mode, let only one Soap server do the destroy.
     */
    do_destroy = fully_initialised() && (get_copymode()==1 || sp_whoami()==1);
#else
    do_destroy = 0;	/* temporarily */
#endif

    /* Perform intentions in the order in which they were created.
     * This is necessary because we try to optimise the number of disk
     * accesses by accumulating intentions before actually performing them
     * on disk.  As a result of that, multiple intentions for the same
     * directory may be present in the intentions list.
     */
    for (; Intent_first < super_nintents(); Intent_first++) {
	sp_tab *st;
	struct sp_new intention;
	capability    dir_caps[NBULLETS];
	int 	      same_version = 0;
	objnum 	      obj;
	int 	      j;

	super_get_intent(Intent_first, &intention);
	obj = intention.sn_object;
	get_dir_caps(obj, dir_caps);
	st = &_sp_table[obj];
	
	for (j = 0; j < NBULLETS; j++) {
	    if (cap_cmp(&dir_caps[j], &intention.sn_caps.sc_caps[j])) {
		if (!NULLPORT(&dir_caps[j].cap_port)) {
		    same_version = 1;
		}
	    } else {
		if (obj == 0) {
		    /* Object 0 is special: its cappair contains the
		     * capabilities for the file servers to use.
		     * Apparently someone wants us to switch over to a
		     * new bullet server.  TODO: do we allow NULLCAP here?
		     */
		    MON_EVENT("switching over to new bullet server");
		    fsvr_replace_svr(j, &intention.sn_caps.sc_caps[j]);
		} else if (do_destroy && !NULLPORT(&dir_caps[j].cap_port)) {
		    MON_EVENT("destroy previous dir");
		    destroy_file(&dir_caps[j]);
		}
		dir_caps[j] = intention.sn_caps.sc_caps[j];
	    }
	}

	/* Switch over to the new replica set */
	set_dir_caps(obj, dir_caps);
	
	/* If all ports are null now, the directory can be removed. */
	if ((st->st_flags & SF_DESTROYING) != 0) {
	    MON_EVENT("destroyed dir myself");
	    assert(caps_zero(dir_caps, NBULLETS) == NBULLETS);
	    cache_destroy(obj);
	} else if (caps_zero(dir_caps, NBULLETS) == NBULLETS) {
	    MON_EVENT("other side destroyed dir");
	    cache_destroy(obj);
	} else {
	    if (st->st_flags & SF_HAS_MODS) {
		/* The "undo" data can now be freed. */
		assert(st->st_dir != NULL);
		sp_free_mods(st);
	    } else {
		if (st->st_dir != NULL && st->st_dir->sd_mods != NULL) {
		    panic("directory %ld has pending mods", obj);
		}
	    }

#ifndef SOAP_GROUP
	    /* In case the intentions were generated by the other side,
	     * we have to uncache the directory if it was modified
	     * (i.e., when no non-NULL cap is the same).
	     */
	    if (from_other_side && !same_version) {
		cache_delete(obj);
		set_time_to_live(obj, SP_MAX_TIME2LIVE); /* Mark it as fresh */
	    }
#endif
	}
    }

#ifdef EXTENSIVE_CHECKING
    {
	/* After the rollforward, all accepted modification lists should have
	 * been removed from the directory structures.  We check this here.
	 */
	register objnum obj;

	for (obj = 1; obj < _sp_entries; obj++) {
	    if (sp_in_use(&_sp_table[obj]) && in_cache(obj)) {
		register struct sp_save *mod;

		if ((mod = _sp_table[obj].st_dir->sd_mods) != NULL) {
		    scream("directory %ld has pending modification %ld",
			   obj, mod->ss_command);
		}
	    }
	}
    }
#endif
}

void
intent_undo()
{
    /* When sp_abort()ing a Soap transaction, we also have to remove
     * the intentions created for it.
     */
    int int_nr;

    for (int_nr = super_nintents() - 1; int_nr >= Intent_first; int_nr--) {
	struct sp_new intention;
	capability    dir_caps[NBULLETS];
	int           j;

	super_get_intent(int_nr, &intention);
	get_dir_caps(intention.sn_object, dir_caps);

	for (j = 0; j < NBULLETS; j++) {
	    if (!cap_cmp(&dir_caps[j], &intention.sn_caps.sc_caps[j])) {
		destroy_file(&intention.sn_caps.sc_caps[j]);
		MON_EVENT("sp_abort: destroy unaccepted intention");
	    }
	}
    }

    intent_clear();
}


#ifndef SOAP_GROUP

/* Send an intention to the other server.  Do this carefully.  If the server
 * appears to be unavailable, we should actually make sure that the server
 * is unavailable, for example by resetting the processor and checking
 * that there are no processes running.  We might implement this later when
 * we have a good boot server available.  For now we are extremely careful
 * and cross our fingers, hoping that the unavailability is not due to three
 * communication errors in a row, as could happen as a result of network
 * partitioning.
 *
 * If we do assume the other side to have crashed, we switch to one-copy-mode
 * so that we can complete user requests without the consent of the other
 * server.
 */
#ifdef SOAP_DIR_SEQNR
static char Intent_buf[sizeof(sp_seqno_t) + MAXINTENT * sizeof(struct sp_new)];
#else
static char Intent_buf[MAXINTENT * sizeof(struct sp_new)];
#endif

static errstat
sp_sendintent()
{
    header  hdr;
    int     notfound;
    bufsize size;
    int     int_nr;
    char   *bufp, *end;
    errstat err;
    int	    tot_intents;
    
    MON_EVENT("send intention");

    /* Marshall the new intentions (with index Intent_first onward)
     * to Intent_buf
     */
    bufp = Intent_buf;
    end = &Intent_buf[sizeof Intent_buf];

#ifdef SOAP_DIR_SEQNR
    {
	/* Prefix the intentions by the global seqno. */
	sp_seqno_t seqno;

	get_global_seqno(&seqno);
	bufp = buf_put_seqno(bufp, end, &seqno);
    }
#endif
    tot_intents = super_nintents();
    for (int_nr = Intent_first; int_nr < tot_intents; int_nr++) {
	struct sp_new intention;
	int j;

	super_get_intent(int_nr, &intention);
	bufp = buf_put_objnum(bufp, end, intention.sn_object);
	for (j = 0; j < NBULLETS; j++) {
	    bufp = buf_put_cap(bufp, end, &intention.sn_caps.sc_caps[j]);
	}
    }
    if (bufp == NULL) {	/* shouldn't happen; it's (exactly) big enough */
	panic("Intention RPC buffer overflow");
    }

    /* Try to get the intention across.  Don't give up until RPC_NOTFOUND
     * is received three times in a row.  Currently this amounts to a
     * total of half a minute (each try is 10 seconds).
     */
    (void) timeout((interval) 10000);
    notfound = 0;
    while (notfound < 3) {
	hdr.h_command = SP_INTENTIONS;
	hdr.h_seq = get_seq_nr();
	hdr.h_size = tot_intents - Intent_first;

	err = to_other_side(1, &hdr, Intent_buf, (bufsize)(bufp - Intent_buf), 
			    &hdr, NILBUF, 0, &size);

	if (err == RPC_NOTFOUND) {
	    notfound++;
	} else if (err == RPC_FAILURE) {
	    notfound = 0;	/* start counting again */
	} else {
	    break;
	}
    }
    (void) timeout(DEFAULT_TIMEOUT);
    
    if (err != STD_OK) {
	switch (err) {
	case STD_SYSERR:   /* we didn't know it crashed; it'll be back soon */
	case RPC_NOTFOUND:
	    MON_EVENT("other side seems to have crashed");
	    message("other side seems to have crashed");
	    set_copymode(1);
	    flush_copymode();

	    /* We don't know what values the other server had in the
	     * time_to_live attributes of its directories.  Assume the worst,
	     * so we can continue aging and expiring directories safely.
	     */
	    refresh_all();
	    err = STD_OK;
	    break;
	case STD_ARGBAD:
	    panic("bad intentions format/sequence number");
	    break;
	case STD_COMBAD:
	    panic("protocol bug: other side did not expect intention");
	    break;
	case STD_NOTNOW:
	    /* other side was busy when it received the intention */
	    MON_EVENT("intention not accepted");
	    break;
	default:
	    scream("unexpected error sending intention (%s)", err_why(err));
	    break;
	}
    }

    return err;
}

#define Failure(err)	{ errval = err; goto failure; }

/* An intention has been received by the other server.  If I receive an
 * intention during recovery, this means that the other server hadn't even
 * noticed my unavailability yet.  In any case, carefully save the intention,
 * unless I'm busy myself.  In that case for_me() will fail.
 */
int
sp_gotintent(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    errstat errval;
    int     intents_added = 0;
    int     new_intents;
    char   *bufp, *end;
    sp_seqno_t new_global_seq;

    if (!for_me(hdr)) {
	return 0;
    }
    
    /* If we get an intention while we are not yet initialised completely
     * then the other side apparently doesn't know about our previous crash.
     * We will tell the other side about this by means of the error
     * code STD_SYSERR (I don't know a really appropriate error code).
     */
    if (!fully_initialised()) {
	scream("other side did not know we had crashed");
	Failure(STD_SYSERR);
    }

    /* Given the current protocol, it should be impossible to receive an
     * intention in one-copy-mode.  However, this isn't theory, and it never
     * harms to check.  The reply causes the other side to panic.
     */
    if (get_copymode() == 1) {
	scream("received intention in 1-copy mode");
	Failure(STD_COMBAD);
    }
    
    if (hdr->h_seq != get_seq_nr() + 1) {
	/* In the current protocol, sequence numbers can only rise by 1 per
	 * round.  My complaint will typically cause the other side to panic.
	 */
	scream("got intention with bogus seq nr %ld (my seq nr is %ld)",
	       hdr->h_seq, get_seq_nr());
	Failure(STD_ARGBAD);
    }

    MON_EVENT("intention accepted");

    new_intents = hdr->h_size;
    if (new_intents < 0 || new_intents > MAXINTENT) {
	scream("bad number of intentions (%ld)", new_intents);
	Failure(STD_ARGBAD);
    }

    /* If we don't have room for the new ones,
     * We first have to make the already accepted ones permanent.
     */
    if (intent_avail() < new_intents) {
	intent_make_permanent();
    }
    assert(intent_avail() >= new_intents);

    bufp = buf;
    end = &buf[size];

#ifdef SOAP_DIR_SEQNR
    /* Intentions are prefixed with the new global seqno. */
    bufp = buf_get_seqno(bufp, end, &new_global_seq);
    if (bufp == NULL) {
	scream("gotintent: global seqno missing");
	Failure(STD_ARGBAD);
    }
#endif

    /* Unmarshall the intentions contained in the buffer,
     * and add them to my own intention list.
     */
    {
	objnum obj;
	capability newcaps[NBULLETS];
	int i;

	for (i = 0; i < new_intents; i++) {
	    int j;

	    bufp = buf_get_objnum(bufp, end, &obj);
	    for (j = 0; j < NBULLETS; j++) {
		bufp = buf_get_cap(bufp, end, &newcaps[j]);
	    }

	    /* sanity checks */
	    if (bufp == NULL) {
		scream("error while unmarshalling intentions");
		Failure(STD_ARGBAD);
	    }
	    if (bad_objnum(obj) && obj != 0) {
		scream("got intention for invalid object %ld", obj);
		Failure(STD_ARGBAD);
	    }

	    intents_added++;
	    intent_add(obj, newcaps);
	}

	set_seq_nr(hdr->h_seq);

	/* set flag to put it on disk before executing any other command. */
	Received_intent = 1;
    }

#ifdef SOAP_DIR_SEQNR
    /* everything ok; switch to new seqno used for directories */
    set_global_seqno(&new_global_seq);
    /* message("gotintent: new global seq [%ld, %ld]",
	    hseq(&new_global_seq), lseq(&new_global_seq)); */
#endif

    /* Trick: in this case we first return the reply, and immediately
     * roll forward if we have got nothing else to do.
     * This avoids letting the other side for us, writing the currently
     * accepted intention to disk, when it almost immediately sends
     * yet another intention.
     */
    hdr->h_status = STD_OK;
    putrep(hdr, NILBUF, 0);
    sp_end();

    /* when not busy: execute this intention */
    if (sp_try()) {
	sp_end();
    }
    return -1;	/* suppress putrep--done already */

failure:
    if (intents_added > 0) {
	intent_clear();
    }
    hdr->h_status = errval;
    sp_end();
    return 0;
}

/* Abort any outstanding operations by restoring the directories.  The
 * information necessary to do this was stored in the sd_mods list for the
 * directory by sp_save_and_mod_dir().  If Bullet files have been created,
 * delete them.
 */
void
sp_abort()
{
    sp_tab *st, *st_next;
    
    MON_EVENT("abort");
    for (st = sp_first_update(); st != NULL; st = st_next) {
	st_next = sp_next_update();

	(void) sp_undo_mod_dir(st);
    }
    sp_clear_updates();

    intent_undo();

    sp_end();
}

/* atomic commit:
 *
 *	create new bullet files for all updated directories.
 *		if this fails, abort.
 *	send the intentions to the other side.
 *		if not accepted, abort.
 *	atomically overwrite block #0, containing intentions.
 */
errstat
sp_commit()
{
    sp_tab *st;
    int nintents = 0;
#ifdef SOAP_DIR_SEQNR
    sp_seqno_t old_global_seq, new_global_seq;

    get_global_seqno(&new_global_seq);
#endif
    
    /*
     * first make sure we have room for the new intentions
     */
    for (st = sp_first_update(); st != NULL; st = sp_next_update()) {
	nintents++;
    }
    if (intent_avail() < nintents) {
	intent_make_permanent();
    }
    if (intent_avail() < nintents) {
	message("too many intentions: %d (max %d)", nintents, intent_avail());
	MON_EVENT("too many intentions");
	sp_abort();
	return STD_NOSPACE;
    }

    /*
     * now try to create new Bullet directory files
     */
    for (st = sp_first_update(); st != NULL; st = sp_next_update()) {
	capability newcaps[NBULLETS];
	objnum obj = st - _sp_table;

#ifdef SOAP_DIR_SEQNR
	incr_seq_no(&new_global_seq);
#endif
	if (st->st_flags & SF_DESTROYING) {
	    MON_EVENT("destroying dir");
	    clear_caps(newcaps, NBULLETS);
	} else {
	    capability curcaps[NBULLETS];
	    capability filesvrs[NBULLETS];
	    errstat err;

	    get_dir_caps(obj, curcaps);
	    fsvr_get_svrs(filesvrs, NBULLETS);
#ifdef SOAP_DIR_SEQNR
	    assert(st->st_dir != 0);
	    st->st_dir->sd_seq_no = new_global_seq;
#endif
	    err = dirf_modify(obj, NBULLETS, curcaps, newcaps, filesvrs);
	    if (err != STD_OK) {
		scream("sp_commit: dirf_modify failed for dir %ld (%s)",
		       (long) obj, err_why(err));
		MON_EVENT("aborted commit");
		sp_abort();
		return SP_UNAVAIL;
	    }
	}
	intent_add(obj, newcaps);
    }

    if ((super_nintents() - Intent_first) > 0) {
	/* All Bullet files are created.  Now update the commit block.
	 * If in 1-copy mode, we can just do so.  Otherwise we first
	 * have to send the intention to the other server.
	 */
	long old_seq;
	int status;

	old_seq = get_seq_nr();
	set_seq_nr(old_seq + 1);
#ifdef SOAP_DIR_SEQNR
	/* intentions accepted, so switch to incremented global seqno */
	get_global_seqno(&old_global_seq);
	set_global_seqno(&new_global_seq);
	/* message("sp_commit: new global seq [%ld, %ld]",
		hseq(&new_global_seq), lseq(&new_global_seq)); */
#endif
	if (get_copymode() == 2) {
	    if ((status = sp_sendintent()) != STD_OK) {
		set_seq_nr(old_seq);	/* restore sequence number */
#ifdef SOAP_DIR_SEQNR
		set_global_seqno(&old_global_seq);
#endif
		sp_abort();
		return status == STD_NOTNOW ? SP_REPEAT : status;
	    }
	}

	super_store_intents();
    }

    sp_clear_updates();
    sp_end();

    return STD_OK;
}

void
intent_check(forced)
int forced;
{
    if (fully_initialised()) {
	if (forced || (intent_avail() < 2)) {
	    intent_make_permanent();
	} else {
	    intent_rollforward();
	}
    }
}

#endif

void
intent_init()
{
}
