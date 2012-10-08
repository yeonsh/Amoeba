/*	@(#)recover.c	1.1	96/02/27 10:02:54 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <assert.h>
#include <stdlib.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <capset.h>
#include <stdcom.h>
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "monitor.h"
#include "module/buffers.h"
#include "module/rnd.h"
#include "module/stdcmd.h"
#include "semaphore.h"
#include "thread.h"

#include "global.h"
#include "main.h"
#include "superfile.h"
#include "misc.h"
#include "caching.h"
#include "sp_impl.h"
#include "sp_adm_cmd.h"
#include "filesvr.h"
#include "dirmarshall.h"
#include "dirfile.h"
#include "seqno.h"
#include "modlist.h"
#include "timer.h"
#include "dirsvr.h"
#include "recover.h"

#ifdef DEBUG
#define dbmessage(list)		message list
#else
#define dbmessage(list)		{}
#endif

/* Recovery state: */
#define ST_INITIAL	0
#define ST_RECOVERING	1
#define ST_RECOVERED	2

static int    Recovery_state = ST_INITIAL;
static objnum Last_dir_recovered;

/* Synchronisation variables implementing locking + timeouts: */
static semaphore Batch_sema;		/* recovery thread blocks on this */
static semaphore Group_thread_sema;	/* group thread blocks on this */

/* Handle 50 directories maximum at a time */
#define MAX_BATCHSIZE	50

/* A batch should be handled within a certain time (30 secs),
 * otherwise we'll time out and start all over again.
 */
#define RECOVERY_TIME	((interval) (30 * 1000))

#define STATEBUFSIZE	8192
#define STACKSIZE	8192

/* Buffer to send over sequence numbers during recovery: */
static char   State_buf[STATEBUFSIZE];

static struct dir_info {
    objnum     dir_num;	
    sp_seqno_t dir_seq;
} Dir_list[MAX_BATCHSIZE];
    
void
highest_dir_seqno(seqno)
sp_seqno_t *seqno;
{
    /* Examine all directories to see which one has the highest sequence
     * number. The global sequence number will be set to this value.
     * In case there is a modification list present, this value still may
     * have to be increased somewhat.
     */
    objnum  obj;
    errstat err;

    sp_begin();
    zero_seq_no(seqno);
    for (obj = 1; obj < _sp_entries; obj++) {
	struct sp_table *st = &_sp_table[obj];

	if (sp_in_use(st)) {
	    /* Make sure its in cache; we need the sequence number */
	    if ((err = sp_refresh(obj, SP_LOOKUP)) != STD_OK) {
		panic("highest_dir_seqno: refresh of %ld failed (%s)",
		      obj, err_why(err));
	    }
	    if (less_seq_no(seqno, &st->st_dir->sd_seq_no)) {
		*seqno = st->st_dir->sd_seq_no;
		message("highest_dir_seqno: dir %ld has higher seq (%ld, %ld)",
			(long) obj, hseq(seqno), lseq(seqno));
	    }
	}
    }
    sp_end();
}

static errstat
update_dirs(buf, end, ndirs, exp_first)
char   *buf;
char   *end;
int     ndirs;
objnum *exp_first; /* sanity check: force rising object numbers */
{
    /* Unmarshall sp_table from the other side and update our copy. */
    struct sp_dir   *sd;
    struct sp_table *st;
    char   *p;
    int     i;

    p = buf;

    for (i = 0; i < ndirs && p != NULL; i++) {
	int16 	flags, time, present;
	objnum	obj;
	errstat err;

	p = buf_get_objnum(p, end, &obj);
	p = buf_get_int16 (p, end, &present);
	if (p == NULL) {
	    return STD_SYSERR;
	}

	if (obj < *exp_first || obj >= _sp_entries) {
	    panic("update_dirs: unexpected dir %ld (first = %ld)",
		  obj, *exp_first);
	}

	sp_begin();

	if (!present) {
	    /* this directory must be removed */
	    message("update_dirs: remove dir %ld", (long) obj);

	    if (sp_in_use(&_sp_table[obj])) {
		remove_dir(obj);
	    }
	} else {
	    message("update_dirs: update dir %ld", (long) obj);

	    p = buf_get_int16(p, end, &flags); /* ignored, currently */
	    p = buf_get_int16(p, end, &time);

	    /* Currently we only sent over whole directories.  Dirs taking
	     * more than 30K should be sent over in several transactions.
	     */
	    p = buf_get_dir(p, end, &sd, &err);
	    if (p == NULL || err != STD_OK) {
		panic("update_dirs: incomplete directory");
	    }

	    st = &_sp_table[obj];
	    if (sp_in_use(st)) {
		remove_dir(obj);
	    }

	    st->st_dir = sd;
	    st->st_flags = SF_IN_CORE;

	    cache_new_dir(obj);	/* add it to the cache */
	    set_time_to_live(obj, (int) time);

	    /* Write to the bullet service. */
	    if ((err = ml_write_dir(obj)) != STD_OK) {
		/* Note: as it is probably a problem with the (local)
		 * bulletsvr, we cannot do anything but panic() in this case.
		 */
		panic("cannot install new version of dir %ld (%s)",
		      (long) obj, err_why(err));
	    }
	}

	sp_end();

	/* force rising dir numbers */
	*exp_first = obj + 1;
    }

    return STD_OK;
}

/* Currently we just use Amoeba5 transactions, which are (documented at least)
 * of size 30K max.  Bigger directories are sent over in multiple chuncks.
 * In Amoeba6 we should be able to do it in a single transaction.
 */
#define MAX_TRANS_SIZE	30000

/* bits set in h_extra */
#define FLAG_BIG	0x00ff	/* a mask for the seqno in a multi-part dir */
#define FLAG_LASTPART	0x0100	/* last part of a multi-part dir */
#define FLAG_LASTDIR	0x0200	/* last dir in this batch */

static long
get_request(hdr, buf, size)
header *hdr;
char   *buf;
long    size;
{
    header  rethdr;
    port    saveport;
    long    received;
    long    off;
    int     partseq;

    saveport = hdr->h_port;
    received = 0;
    partseq = 0;
    off = 0;

    while (received >= 0) {
	bufsize recsize, inmax;

	inmax = (off + MAX_TRANS_SIZE < size) ? MAX_TRANS_SIZE : (size - off);

	hdr->h_port = saveport;
	recsize = getreq(hdr, buf + off, inmax);
	if (ERR_STATUS(recsize)) {
	    scream("svr_state: getreq failed (%s)",
		   err_why((errstat) ERR_CONVERT(recsize)));
	    continue;
	}

	if ((hdr->h_command != SP_DIRS) || ((hdr->h_extra & FLAG_BIG) == 0)) {
	    /* A small single-part transaction.  See if that's possible. */
	    if (partseq != 0) {
		scream("recover: expected multi-part trans: seq %d", partseq);
		rethdr.h_status = STD_ARGBAD;
		putrep(&rethdr, NILBUF, (bufsize) 0);
		received = -1;
	    } else {
		received = recsize;
	    }
	    break;
	} else {
	    /* Member of a multi-part transaction. Check the seqno. */
	    int req_seq = (hdr->h_extra & FLAG_BIG);

	    if (req_seq == (partseq + 1)) {
		off += recsize;
		received += recsize;
		partseq++;
		if ((hdr->h_extra & FLAG_LASTPART) != 0) {
		    message("SP_DIRS: %d parts; %ld bytes", req_seq, received);
		    break;
		} else {    /* acknowledge this one and wait for more */
		    message("SP_DIRS: got part %d: %ld bytes",
			    req_seq, (long) recsize);
		    rethdr.h_status = STD_OK;
		    putrep(&rethdr, NILBUF, (bufsize) 0);
		}
	    } else {
		scream("recover: received part out of order (%d != %d)",
		       req_seq, partseq + 1);
		rethdr.h_status = STD_ARGBAD;
		putrep(&rethdr, NILBUF, (bufsize) 0);
		received = -1;
	    }
	}
    }

    return received;
}

static errstat
send_dir(hdr, buf, size)
header *hdr;
char   *buf;
long    size;
{
    bufsize retsize;
    errstat err;
    header  rethdr;

    hdr->h_command = SP_DIRS;
    if (size <= MAX_TRANS_SIZE) {
	retsize = trans(hdr, buf, (bufsize) size, &rethdr, NILBUF, (bufsize)0);
	if (ERR_STATUS(retsize)) {
	    err = ERR_CONVERT(retsize);
	} else {
	    err = ERR_CONVERT(rethdr.h_status);
	}
    } else {
	int     save_extra = hdr->h_extra;
	long    off = 0;
	int     seq = 1;

	message("sending over directory of %ld bytes", size);
	
	err = STD_OK;
	while (size > 0 && err == STD_OK) {
	    bufsize chunck = (size > MAX_TRANS_SIZE) ? MAX_TRANS_SIZE : size;
	    
	    hdr->h_extra = (save_extra | seq);
	    if (chunck == size) {
		hdr->h_extra |= FLAG_LASTPART;
	    }
	    retsize = trans(hdr, buf + off, (bufsize) chunck,
			    &rethdr, NILBUF, (bufsize) 0);
	    if (ERR_STATUS(retsize)) {
		err = ERR_CONVERT(retsize);
	    } else {
		err = ERR_CONVERT(rethdr.h_status);
	    }

	    if (err == STD_OK) {
		off += chunck;
		size -= chunck;
		seq++;
	    }
	}
    }

    return err;
}

static void
svr_state(param, parsize)
char *param;	/* the getport we should use */
int   parsize;
{
    port     getport;
    header   hdr;
    char    *iobuf;
    char    *p, *end;
    errstat  status;
    int	     go_on = 1;
    objnum   last_dir;
    int      batch_nr;
    long     batch_seq;

    assert(parsize == sizeof(port));
    getport = *((port *) param);

    batch_nr = 0;
    batch_seq = 0;
    last_dir = 0;
    /* Last dir seqno received; we expect a the directories to arrive in
     * rising order; each batch terminated with a request having h_extra == 0.
     */

    /* Maybe we could use the io buffer from the dirfile module here,
     * but it's a bit dangerous considering the fact that it will be locked
     * at least until the first transaction arrives.
     * So to be on the safe side just allocate it dynamically.
     */
    iobuf = (char *) malloc((size_t) IOBUF_SIZE);
    if (iobuf == NULL) {
	panic("could not allocate %ld byte buffer to receive updates",
	      (long) IOBUF_SIZE);
    }

    while (go_on) {
	long size;

	hdr.h_port = getport;
	if ((size = get_request(&hdr, iobuf, (long) IOBUF_SIZE)) < 0) {
	    break;
	}

	p = iobuf;
	end = &iobuf[size];

	status = STD_SYSERR; /* to be overwritten */

	switch (hdr.h_command) {
	case SP_DIRS:
	    if (hdr.h_offset != batch_seq + 1) {
		scream("svr_state: batch %d: got seq %ld instead of %d",
		       batch_nr, hdr.h_offset, batch_seq + 1);
		status = STD_ARGBAD;
		break;
	    } else {
		batch_seq = hdr.h_offset;
	    }

	    if ((hdr.h_extra & FLAG_LASTDIR) != 0) {
		/* Last one in this batch; it contains the global seqno
		 * to use from now on.
		 */
		sp_seqno_t global_seq, new_global_seq;

		if ((p = buf_get_seqno(p, end, &new_global_seq)) != NULL) {
		    dbmessage(("svr_state: batch %d: got last one (%ld)",
			       batch_nr, batch_seq));

		    /* see if we need to update the global seq */
		    get_global_seqno(&global_seq);

#if 1
		    /* temporary sanity check.. */
		    if (hseq(&new_global_seq) != 0) {
			scream("don't trust new global seq (%ld, %ld)",
			       hseq(&new_global_seq), lseq(&new_global_seq));
		    } else
#endif
		    if (!eq_seq_no(&global_seq, &new_global_seq)) {
			message("new global seqno: [%ld, %ld]",
				hseq(&new_global_seq), lseq(&new_global_seq));
			set_global_seqno(&new_global_seq);
		    }

		    /* prepare new batch */
		    batch_nr++;
		    batch_seq = 0;

		    /* awake the thread waiting for this batch to arrive */
		    sema_up(&Batch_sema);
		    status = STD_OK;
		} else {
		    status = STD_ARGBAD;
		}
	    } else {
		/* perform the updates contained in this msg */
		message("got SP_DIRS request with %d updates", hdr.h_size);
		status = update_dirs(p, end, (int) hdr.h_size, &last_dir);
	    }
	    break;

	case STD_DESTROY:
	    message("svr_state: got a STD_DESTROY request; leaving");
	    go_on = 0;
	    status = STD_OK;
	    break;

	default:
	    scream("svr_state: bad command %d", hdr.h_command);
	    status = STD_COMBAD;
	    break;
	}

	hdr.h_status = status;
	putrep(&hdr, NILBUF, (bufsize) 0);
    }

    thread_exit();
}
	
int
rec_recovered(obj)
objnum obj;
{
    /* Called by the dirsvr module during recovery to see whether it
     * already should perform updates on the directory given as argument.
     */
    int ok = (Recovery_state != ST_INITIAL && obj <= Last_dir_recovered);

    message("rec_recovered(%ld) = %d (last = %ld)",
	    (long) obj, ok, (long) Last_dir_recovered);
    return ok;
}

errstat
rec_get_state(helper)
int         helper; /* server supposed to be helping me */
{
    header  hdr;
    char   *p, *end;
    port    rcv_pubport;
    port   *rcv_getport;
    objnum  obj;
    errstat err;

    Recovery_state = ST_RECOVERING;
    Last_dir_recovered = 0;

    sema_init(&Batch_sema, 0);
    sema_init(&Group_thread_sema, 0);

    /* Create a port to receive state information and start a thread
     * listening to it.
     */
    if ((rcv_getport = (port *) malloc(sizeof(port))) == NULL) {
	panic("get_state: cannot allocate port");
    }
    uniqport(rcv_getport);
    priv2pub(rcv_getport, &rcv_pubport);

    if (!thread_newthread(svr_state, STACKSIZE,
			  (char *) rcv_getport, sizeof(port)))
    {
	panic("couldn't start state server thread");
    }

    /* Send our directory sequence numbers to the member helping us to recover.
     * We must send them to the group in order to be sure that no directory
     * updates arriving in the meantime are missed.
     * The helping member will send the updates needed to the thread we
     * just created.
     */
    err = STD_OK;
    for (obj = 1; obj < _sp_entries; ) {
	/* create a batch of <dir, seq> pairs and send them to the group */
	int i;

	p = State_buf;
	end = &State_buf[sizeof(State_buf)];

	/* first add the port for sending the updates to */
	p = buf_put_port(p, end, &rcv_pubport);

	sp_begin();

	for (i = 0; i < MAX_BATCHSIZE && obj < _sp_entries; i++, obj++) {
	    struct sp_table *st;

	    p = buf_put_objnum(p, end, obj);

	    st = &_sp_table[obj];
	    if (sp_in_use(st) && (sp_refresh(obj, SP_LOOKUP) == STD_OK)) {
		dbmessage(("current dir %ld has seqno [%ld, %ld]", obj,
			   hseq(&st->st_dir->sd_seq_no),
			   lseq(&st->st_dir->sd_seq_no))); 
		p = buf_put_seqno(p, end, &st->st_dir->sd_seq_no);
	    } else {
		/* use zero seqno to indicate absent directory */
		static sp_seqno_t zero_seqno;

		p = buf_put_seqno(p, end, &zero_seqno);
	    }
	}

	sp_end();

	if (p == NULL) {
	    err = STD_SYSERR;
	    break;
	}
	
	dbmessage(("sending %d SP_DIR_SEQS to group; %ld bytes",
		   i, (long) (p - State_buf)));

	hdr.h_command = SP_DIR_SEQS;
	hdr.h_offset = helper;		/* the Soap we want help from */
	hdr.h_extra = sp_whoami();	/* that's me */
	err = send_to_group(&hdr, State_buf, (p - State_buf));

	if (err == STD_OK) {
	    /* Now wait until we got the updates for this batch,
	     * or until we time out, in which case we must start all
	     * over again.
	     */
	    message("awaiting updates for directories < %ld", (long) obj);
	    if (sema_trydown(&Batch_sema, RECOVERY_TIME) == 0) {
		Last_dir_recovered = obj - 1;
	    } else {
		scream("timed out waiting for batch of updates");
		err = STD_NOTNOW;
	    }

	    /* the group thread can now continue processing again (it was
	     * blocked when we received our own SP_DIR_SEQS request).
	     */
	    sema_up(&Group_thread_sema);
	} else {
	    scream("could not send SP_DIR_SEQS to group (%s)", err_why(err));
	}

	if (err != STD_OK) {
	    break;
	}
    }

    /* Now ask the svr_state thread to go away. */
    {
	capability svrcap;
	errstat err1;

	svrcap.cap_port = rcv_pubport;
	/* TODO: add checkfield checking */
	if ((err1 = std_destroy(&svrcap)) != STD_OK) {
	    message("could not destroy svr_state thread (%s)", err_why(err1));
	}
    }

    if (err == STD_OK) {
	Recovery_state = ST_RECOVERED;
    } else {
	/* we'll have to try again */
	Recovery_state = ST_INITIAL;
    }

    return err;
}

static errstat
get_dir_seqnos(buf, end, dirlist, maxdirs, ndirp)
char		*buf;
char		*end;
struct dir_info *dirlist;
int		 maxdirs;
int		*ndirp;
{
    int ndirs;
    char *p;

    p = buf;
    ndirs = 0;
    while ((p != NULL && p < end) && (ndirs < maxdirs)) {
	p = buf_get_objnum(p, end, &dirlist[ndirs].dir_num);
	p = buf_get_seqno (p, end, &dirlist[ndirs].dir_seq);
	if (p != NULL) {
	    if (bad_objnum(dirlist[ndirs].dir_num)) {
		return STD_ARGBAD;
	    }
	    ndirs++;
	}
    }

    if (p != end) {
	/* <obj, seq> pairs did not fit the buffer exactly */
	return STD_SYSERR;
    }

    *ndirp = ndirs;
    return STD_OK;
}

errstat
rec_send_updates(inhdr, inbuf, insize)
header *inhdr;
char   *inbuf;
int     insize;
{
    /* A new member asks for directory updates by means of a SP_DIR_SEQS
     * request to the group.  Send them.
     */
    sp_seqno_t  global_seq;
    char        seqbuf[sizeof(sp_seqno_t)];
    port	update_port; /* to send updates to */
    char       *p, *end;
    int		i, n_dirs;
    errstat	err;
    int		dircount;
    header	hdr;
    
    /* The request starts with a port to send updates to; get it first */
    if ((p = buf_get_port(inbuf, inbuf + insize, &update_port)) == NULL) {
	return STD_SYSERR;
    }

    err = get_dir_seqnos(p, inbuf + insize, Dir_list, MAX_BATCHSIZE, &n_dirs);
    if (err != STD_OK) {
	scream("malformed directory seqno list (%s)", err_why(err));
	return err;
    }
	
    dbmessage(("rec_send_updates: helping Soap %d, with a batch of %d dirs",
	       inhdr->h_extra, n_dirs));

    /* Now send over the directories that need to be updated.
     * For convenience, we only do it on at a time, currently.
     * TODO: make sure recovery of one batch doesn't block the group thread
     * too long.
     */
    dircount = 0;
    for (i = 0; i < n_dirs; i++) {
	struct sp_table *st;
	objnum     obj;
	int        need_update;
	sp_seqno_t seqno;

	obj = Dir_list[i].dir_num;
	seqno = Dir_list[i].dir_seq;

	need_update = 1; /* we need to update it unless the seqno is the same*/

	st = &_sp_table[obj];
	if (sp_in_use(st)) {
	    int ttl;

	    /* sp_refresh normally resets the time-to-live field, but we
	     * surely don't want that here, so restore it afterwards.
	     */
	    ttl = get_time_to_live(obj);
	    err = sp_refresh(obj, SP_LOOKUP);
	    set_time_to_live(obj, ttl);

	    if (err == STD_OK) {
		if (eq_seq_no(&seqno, &st->st_dir->sd_seq_no)) {
		    need_update = 0;
		} else if (less_seq_no(&seqno, &st->st_dir->sd_seq_no)) {
		    message("dir %ld: received seq(%ld,%ld) < mine (%ld,%ld)",
			    obj, hseq(&seqno), lseq(&seqno),
			    hseq(&st->st_dir->sd_seq_no),
			    lseq(&st->st_dir->sd_seq_no)); 
		} else {
		    /* should never happen! */
		    scream("Protocol bug: %ld: received (%ld,%ld) > (%ld,%ld)",
			   obj, hseq(&seqno), lseq(&seqno),
			   hseq(&st->st_dir->sd_seq_no),
			   lseq(&st->st_dir->sd_seq_no)); 
		}
	    } else {
		scream("rec_send_updates: refresh for %ld failed (%s)",
		       (long) obj, err_why(err));
		/* We break off without sending the other side any more
		 * updates.  It will time out after a while, and try again.
		 * Note: this will mean that we cannot bring up another
		 * member when the problem persists.
		 * Know any better solution?
		 */
		return err;
	    }
	} else {
	    static sp_seqno_t zero_seqno;

	    if (eq_seq_no(&seqno, &zero_seqno)) {
		/* this directory still does not exist */
		need_update = 0;
	    } else {
		message("dir %ld should be thrown away", (long) obj);
	    }
	}

	if (need_update) {
	    char   *iobuf;
	    header  hdr;

	    hdr.h_port = update_port;
	    hdr.h_extra = 0;	/* set to FLAG_LASTDIR in the last update */
	    hdr.h_size = 1;	/* number of updates in this rpc */
	    hdr.h_offset = ++dircount; /* sanity check */

	    iobuf = sp_get_iobuf();
	    p = iobuf;
	    end = iobuf + IOBUF_SIZE;

	    /* And now the directory itself */
	    p = buf_put_objnum(p, end, obj);
	    if (sp_in_use(&_sp_table[obj])) {
		message("sending over directory %ld", (long) obj);
		p = buf_put_int16(p, end, (int16) 1);
		p = buf_put_int16(p, end, (int16) st->st_flags);
		p = buf_put_int16(p, end, (int16) st->st_time_left);
		p = buf_put_dir  (p, end, st->st_dir);
	    } else {
		message("telling other side to remove %ld", (long) obj);
		p = buf_put_int16(p, end, (int16) 0);
	    }
	    if (p == NULL) {
		/* The io buffer is guaranteed to be able to hold a flattened
		 * directory and 1000 bytes extra, so this shouldn't happen.
		 */
		err = STD_SYSERR;
	    } else {
		err = send_dir(&hdr, iobuf, (long) (p - iobuf));
	    }

	    sp_put_iobuf(iobuf);

	    if (err != STD_OK) {
		message("rec_send_updates: SP_DIRS trans failed (%s)",
			err_why(err));
		return err;
	    }
	}
    }

    /* now tell the other side we're finished */
    hdr.h_port = update_port;
    hdr.h_extra = FLAG_LASTDIR;	/* last one */
    hdr.h_size = 0;		/* contains no dirs */
    hdr.h_offset = ++dircount;

    /* Add the current global seqnr.  The other side needs it so that it
     * knows what sequence numbers to use in future directory modifications
     * (actually, we only need to tell it once, but it's simpler this way,
     * and it provides an additional sanity check).
     */
    get_global_seqno(&global_seq);
    p = buf_put_seqno(seqbuf, &seqbuf[sizeof(seqbuf)], &global_seq);
    assert(p != NULL);
	    
    return send_dir(&hdr, seqbuf, (long) (p - seqbuf));
}

void
rec_await_updates()
{
    dbmessage(("rec_await_updates: block"));

    if (sema_trydown(&Group_thread_sema, RECOVERY_TIME) == 0) {
	dbmessage(("rec_await_updates: ready"));
    } else {
	message("rec_await_updates: timed out");
    }
}

