/*	@(#)expire.c	1.3	96/02/27 10:21:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "module/buffers.h"
#include "module/mutex.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"

#include "global.h"
#include "main.h"
#include "caching.h"
#include "intentions.h"
#include "expire.h"
#include "sp_impl.h"
#include "superfile.h"

#define Failure(e)	{ errval = e; goto failure; }

static errstat
destroy_dir(obj)
objnum obj;
{
    /* Note that we cannot use sp_dodestroy() for this, because that only
     * works when the directory is empty.
     */
    header  hdr;
    struct  sp_table *st;
    errstat errval, err;

    if (!sp_try()) {
	/* only destroy directories when not busy */
	return STD_NOTNOW;
    }

    if (bad_objnum(obj)) {
	scream("expire: bad directory %ld", obj);
	Failure(STD_ARGBAD);
    }

    st = &_sp_table[obj];

    /* Check that the directory is still in use, and that its time-to-live
     * attribute is still zero.  Though unlikely, the object could have been
     * touched/used just after we expired it.
     */
    if (!sp_in_use(st) || get_time_to_live(obj) > 0) {
	scream("expire: dir %ld should not be expired after all", obj);
	Failure(STD_OK); /* don't try this one again */
    }

    /* Ensure that it is in the cache. */
    err = sp_refresh(obj, STD_DESTROY);
    set_time_to_live(obj, 0);    /* because it was reset by sp_refresh */

    if (err != STD_OK) {
	/* Bad directory file format?  Probably requires a manual sp_fix.
	 * Otherwise the watchdog will clean it up eventually.
	 */
	scream("expire: could not refresh dir %ld (%s)", obj, err_why(err));
	Failure(err);
    }

    hdr.h_command = STD_DESTROY;
    if (!sp_save_and_mod_dir(st, &hdr, 0, (struct sp_row *)NULL)) {
	Failure(STD_NOSPACE);	/* only possible reason */
    }

    if ((err = sp_commit()) == SP_REPEAT) {	/* other side busy */
	MON_EVENT("expire: could not commit deletion of a directory");
	err = STD_NOTNOW;
    }

    if (err == STD_OK) {
	MON_EVENT("expire: deleted expired directory");
    }
    return err;

failure:
    sp_end();
    MON_EVENT("expire: directory deletion failed");
    return errval;
}

#define MAXEXPIRED	100	/* not dependent on MAXINTENT anymore */
#define OBJ_UNUSED	0    /* indicates unused entry in Expired_dirs */

static objnum   Expired_dirs[MAXEXPIRED];
static int    n_Expired = 0;
static mutex mu_Expired;

void
expire_delete_dirs()
{
    int     i;
    errstat err;

    /* We come here with the global mutex locked; first unlock it.
     * We will try to relock it for every entry seperately.
     */
    sp_end();

    mu_lock(&mu_Expired);
    
    for (i = 0; i < n_Expired; i++) {
	if (Expired_dirs[i] != OBJ_UNUSED) {
	    err = destroy_dir(Expired_dirs[i]);
	    if (err != STD_NOTNOW && err != STD_NOSPACE) {
		/* i.e., STD_OK or some unexpected error */
		Expired_dirs[i] = OBJ_UNUSED;
	    }
	}
    }
    
    mu_unlock(&mu_Expired);
}

static errstat
add_to_expire_list(obj)
objnum obj;
{
    /*
     * Directory obj has expired, and all available Soap servers agree with
     * this.  We cannot destroy the directory at once, because we have the
     * global mutex locked.  What we do is: we now save the object number in
     * an array, and we will try to destroy it, the next time
     * expire_delete_dirs() is called.
     * In one copy mode, it is called just after the watchdog.
     * In two copy mode, soap 0 calls the watchdog() every minute,
     * and soap 1 calls expire_delete_dirs().
     */
    int	    present, unused;
    int     i;
    errstat err = STD_OK;

    mu_lock(&mu_Expired);

    /* First check if it is not already present in Expired.
     * Also look for an unsed entry.
     */
    unused = -1;
    present = -1;
    for (i = 0; i < n_Expired; i++) {
	if (Expired_dirs[i] == obj) {
	    present = i;
	    break;
	} else if (unused < 0 && Expired_dirs[i] == OBJ_UNUSED) {
	    unused = i;
	}
    }

    if (present < 0) {	/* not yet present; add it */
	if (unused < 0 && n_Expired < MAXEXPIRED) {
	    /* add a new entry to Expired_dirs */
	    MON_EVENT("expire: extend Expired");
	    unused = n_Expired;
	    Expired_dirs[unused] = OBJ_UNUSED;
	    n_Expired++;
	}

	if (unused < 0) {
	    err = STD_NOSPACE;
	} else {
	    Expired_dirs[unused] = obj;
	}
    } else {
	MON_EVENT("expire: dir already in Expired list");
    }

    mu_unlock(&mu_Expired);
    return err;
}

/*
 * We've received a list of expired directories from the other server.  We
 * construct an intention list to destroy the ones that we agree are expired
 * and send the rest back in our reply, together with their non-zero
 * time2live fields.  This tells the other server to update its own time2live
 * fields for these directories, and prevents it from trying to expire
 * these directories in the next STD_AGE operation.
 */
int
expire_dirs(hdr, inbuf, insize, outbuf, outsize)
header *hdr;
char   *inbuf;
int     insize;
char   *outbuf;
int     outsize;
{
    int32   num_expired;
    int32   num_refused = 0;
    objnum  refused[MAXEXPIRED];
    int     i;
    char   *bufp, *end;
    errstat errval;
    
    if (!for_me(hdr)) {
	return 0;
    }
    if (!fully_initialised()) {
	Failure(STD_NOTNOW);
    }
    MON_EVENT("expire: got expiration list");
    
    bufp = inbuf;
    end = &inbuf[insize];
    bufp = buf_get_int32(bufp, end, &num_expired);
    if (bufp == NULL || num_expired < 0 || num_expired > MAXEXPIRED) {
	Failure(STD_ARGBAD);
    }

    for (i = 0; i < num_expired; i++) {
	objnum olddir;

	bufp = buf_get_objnum(bufp, end, &olddir);
	if (bufp == NULL || bad_objnum(olddir)) {
	    Failure(STD_ARGBAD);
	}
	
	if (get_time_to_live(olddir) > 0 ||
	    add_to_expire_list(olddir) != STD_OK)
	{
	    refused[num_refused++] = olddir;
	}
    }
    
    /* Form a reply containing all those directories that have
     * non-zero time2live, or which we could not add to the expiration list.
     */
    bufp = outbuf;
    end = &outbuf[outsize];
    bufp = buf_put_int32(bufp, end, num_refused);
    for (i = 0; i < num_refused; i++) {
	bufp = buf_put_objnum(bufp, end, refused[i]);
	bufp = buf_put_int16(bufp, end, (int16) get_time_to_live(refused[i]));
    }
    if (bufp == NULL) {
	/* Oops, overflowed buffer. */
	bufp = end;
    }
    
    sp_end();
    hdr->h_status = STD_OK;
    return (bufp - outbuf);

failure:
    sp_end();
    hdr->h_status = errval;
    return 0;
}


/*
 * Send list of expired dirs (i.e., time2live==0) dirs to the other server.
 * It will destroy only those that it agrees are expired (have not been
 * accessed recently enough in the other server either):
 */
static errstat
send_expired(num_expired, expired)
long num_expired;
objnum expired[];
{
    /*
     * Although these the following buffers are static (to avoid stack problems
     * in case MAXEXPIRED is increased someday), they need not be protected by
     * seperate mutexes, because they are only used while the global sp_mutex
     * is locked.
     */
    static char expired_buf[sizeof(long) + MAXEXPIRED * sizeof(objnum)];
    static char refused_buf[sizeof(long) +
			    MAXEXPIRED * (sizeof(objnum) + sizeof(short))];
    header  hdr;
    bufsize n;
    int     i;
    int32   num_refused;
    char   *bufp, *end;
    errstat err;
    
    assert(num_expired <= MAXEXPIRED);
    MON_EVENT("expire: send expiration list");

    /* Fill buffer with dirs that we think should be expired: */
    bufp = expired_buf;
    end = &expired_buf[sizeof(expired_buf)];
    bufp = buf_put_int32(bufp, end, (int32) num_expired);
    for (i = 0; i < num_expired; i++) {
	bufp = buf_put_objnum(bufp, end, expired[i]);
    }
    if (bufp == NULL) {
	panic("send_expired: transaction buffer too small");
    }
    
    hdr.h_command = SP_EXPIRE;
    err = to_other_side(3, &hdr, expired_buf, (bufsize) (bufp - expired_buf),
			&hdr, refused_buf, (bufsize) sizeof(refused_buf), &n);

    if (err != STD_OK) {
	message("send_expired: expired dirs not accepted (%s)",	err_why(err));
	return err;
    }

    /*
     * The reply consists of an array of (directory-number, time2live)
     * pairs.  The other server is telling us which directories it refused
     * to expire because they have non-zero time2live fields.  We now
     * update our time2live fields to reflect this:
     */
    bufp = refused_buf;
    end = &refused_buf[n];

    bufp = buf_get_int32(bufp, end, &num_refused);
    if (bufp == NULL || num_refused < 0 || num_refused > num_expired) {
	return STD_SYSERR;
    }

    err = STD_OK;
    for (i = 0; i < num_refused; i++) {
	objnum refused_dir;
	int16  correct_time2live;

	bufp = buf_get_objnum(bufp, end, &refused_dir);
	bufp = buf_get_int16(bufp, end, &correct_time2live);
	if (bufp == NULL) {
	    MON_EVENT("send_expired: short reply");
	    err = STD_SYSERR;
	    break;
	}

	/* sanity checks */
	if (bad_objnum(refused_dir) ||
	    (correct_time2live < 0 || correct_time2live > SP_MAX_TIME2LIVE))
	{
	    MON_EVENT("send_expired: bad reply");
	    err = STD_SYSERR;
	    break;	/* don't trust the rest either */
	}

	if (correct_time2live == 0) {
	    /* we'll try this one again next time */
	    MON_EVENT("send_expired: other side could not destroy dir");
	} else if (get_time_to_live(refused_dir) < correct_time2live) {
	    /* other side thinks this dir should live a bit longer */
	    MON_EVENT("send_expired: correct time2live");
	    set_time_to_live(refused_dir, (short) correct_time2live);
	}
    }
    
    return err;
}


/*
 * Decrement the time-to-live for each directory.  If in 1-copy mode, destroy
 * each one that has expired (time-to-live == 0).  If in 2-copy mode, and I
 * am server 0, just tell server 1 which ones have expired and let server 1 do
 * the destroys (for those that it also agrees have expired).  If in 2-copy
 * mode and I am server 1, do nothing but decrement time-to-live.  The separate
 * SP_EXPIRE command from the other server will cause us to actually destroy
 * dirs.
 */
static errstat
expire_do_age()
{
    objnum  obj;
    objnum  expired[MAXEXPIRED];
    long    num_expired = 0;	/* number of entries added to `expired' */
    errstat err = STD_OK;

    MON_EVENT("expire: age");

    for (obj = 1; obj < _sp_entries; obj++) {
	int ttl;

	if (!sp_in_use(&_sp_table[obj])) {
	    continue;
	}

	if ((ttl = get_time_to_live(obj)) > 0) {
	    /* decrement time-to-live attribute for this dir */
	    set_time_to_live(obj, ttl - 1);
	} else {
	    /* dir is candidate for removal */
	    if (get_copymode() == 1)  {
		/* In one copy mode we destroy it without asking permission. */
		if (add_to_expire_list(obj) == STD_OK) {
		    MON_EVENT("expire: delete expired dir (1-copy mode)");
		}
	    } else {
		/* In two-copy mode, Soap 0 is the "master" for deleting
		 * expired directories.  Here we add the expired dir to a
		 * list.  The list will be sent to Soap 1, which will
		 * delete the directories that are expired according to him
		 * as well.  The others are sent back with their corrected
		 * time-to-live attribute.
		 */
		if (sp_whoami() == 0 && num_expired < MAXEXPIRED) {
		    expired[num_expired++] = obj;
		}
	    }
	}
    }
    
    if (num_expired > 0) {
	assert(get_copymode() == 2);
	assert(sp_whoami() == 0);

	if (num_expired == MAXEXPIRED) {
	    /* give a hint to increase the size of the expiration buffer */
	    MON_EVENT("expire: expiration buffer full");
	}

	err = send_expired(num_expired, expired);
    }

    if (err != STD_OK) {
	message("expire: could not send expired dirs (%s)", err_why(err));
    }

    return err;
}

int
expire_age(hdr)
header *hdr;
{
    if (!for_me(hdr)) {
	return 0;
    }

    if (!fully_initialised()) {
	hdr->h_status = STD_NOTNOW;
    } else {
	hdr->h_status = expire_do_age();
    }

    sp_end();
    return 0;
}

/*
 * We've received an STD_AGE command.
 * First make sure (for security) that it was sent to my private port
 * and that the client has full rights turned on in the capability.
 * Then inform the other SOAP server, using SP_AGE to its private port,
 * and then act like we got an SP_AGE command ourself:
 */
int
expire_std_age(hdr)
header *hdr;
{
    int     try;
    errstat err;

    /* We might be busy performing an operation, so we retry a few times
     * if we cannot get the global lock immediately.
     */
    err = STD_NOTNOW;
    for (try = 0; try < 5; try++) {
	if (for_me(hdr)) {
	    err = STD_OK;
	} else {
	    err = ERR_CONVERT(hdr->h_status);
	}

	if (err == STD_NOTNOW) {
	    MON_EVENT("busy during STD_AGE");
	    (void) sleep(1);
	} else {	    /* STD_OK or a bad capability */
	    break;
	}
    }

    if (err != STD_OK) {
	hdr->h_status = err;
	message("std_age failed (%s)", err_why(err));
	return 0;
    }

    if (!fully_initialised()) {
	hdr->h_status = STD_NOTNOW;
	sp_end();
	return 0;
    }
    
    MON_EVENT("std_age");
    
    if (get_copymode() == 2) {
	/* Must tell the other SOAP server to age as well */
	bufsize size;

	hdr->h_command = SP_AGE;
	err = to_other_side(3, hdr, NILBUF, 0, hdr, NILBUF, 0, &size);

	if (err != STD_OK) {
	    message("other side didn't accept SP_AGE (%s)", err_why(err));
	    /* Still, we must age the directories ourselves.
	     * If the other side is really gone, we'll find out in a moment.
	     */
	}
    }

    hdr->h_status = expire_do_age();
    sp_end();
    return 0;
}



void
expire_init()
{
    mu_init(&mu_Expired);
}
