/*	@(#)dirsvr.c	1.1	96/02/27 10:02:37 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "amoeba.h"
#include "group.h"
#include "caplist.h"
#include "semaphore.h"
#include "exception.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "module/buffers.h"
#include "module/mutex.h"
#include "module/rnd.h"
#include "module/ar.h"
#include "module/prv.h"
#include "thread.h"
#include "monitor.h"

#include "global.h"
#include "main.h"
#include "superfile.h"
#include "recover.h"
#include "modlist.h"
#include "sp_print.h"
#include "timer.h"
#include "sp_adm_cmd.h"
#include "dirmarshall.h"
#include "seqno.h"
#include "caching.h"
#include "ax/server.h"
#include "barrier.h"

/* external soap variables */
extern port		_sp_getport;
extern capability 	_sp_generic;
extern capability 	my_supercap;

/* debugging */
#define DEBUG_LEVEL	1
#define GRP_DEBUG	1	/* group kernel debug level */

#define dbmessage(lev, list) if (lev <= DEBUG_LEVEL) { message list; }

/* group defines */    
#define LOGDATA 	13	/* log of the maximum size broadcast msg  */
#define MAXDATA		(1 << LOGDATA)
#define MAXGROUP 	5	/* maximum number of members in the group */
#define LOGBUF 		6	/* log of the number of buffers used */
#define SYNCTIME	8000	/* (msec) time between sychronization */
#define ALIVETIME	5000	/* (msec) time between checking liveness */
#define REPLYTIME	1000	/* (msec) time before resending a msg */
#define LARGEMESS	2000	/* when to use method 2 */

/* default number of server threads: */
#define NTASKS		8

/* general values */
#define SVR_STACKSIZE	(SP_BUFSIZE + 4096 + 4096)
#define GRP_STACKSIZE	SVR_STACKSIZE

/* The default minimum number of members needed to allow updates
 * depends on the total numbers of members (up or down).
 * For a group of 2 members, both have to be up (otherwise a network
 * partition would result in inconsistencies).
 * For a group of 3 members, at least two servers should be up.
 */
#define MIN_COPIES(nmem) ((nmem) / 2 + 1)

/* group variables */
static g_indx_t   Grp_max_members = 0;
static g_indx_t   Grp_this_member = 0;
static g_id_t	  Grp_id = -1;
static g_indx_t   Grp_memlist[MAXGROUP];
static int 	  Grp_more_mess = 0;
static g_indx_t   Grp_nmembers;
static grpstate_t Grp_state;
static g_seqcnt_t Grp_seqno;
static int	  Grp_nthreads = 0; /* group threads listening */
static int	  Grp_missed_update = 0;
static long       Grp_time = 0;

/* For recovery, there are 3 modes: */
#define MD_RECOVERING	0	/* still determining partners; bidding  */
#define MD_UP_TO_DATE	1	/* my directory state is up to date     */
#define MD_FUNCTIONING	2	/* all members' directory states are ok */

static int Grp_mode = MD_RECOVERING;
static int Grp_best_mode = MD_RECOVERING;

/* synchronization variables */
static int        Awaiting_bids;	/* restricts use of Sem_recovering   */
static semaphore  Sem_recovering;	/* wait till enough members joined   */
static semaphore  Sem_functioning;	/* wait till enough members recovered*/
static semaphore  Sem_leaving;		/* wait till I get my own leave msg  */
static semaphore  Sem_need_recovery;	/* wait till recovery is needed      */

static struct barrier *Grp_barrier;

/* For each server task we reserve a result structure statically.
 * For "write" commands, the group thread finds the slot to return
 * the result by means of the thread index, which is added to the
 * request header.
 */
#define encode_src(mem, thread) ((thread << 8) | mem)
#define decode_member(enc)	(enc & 0xFF)
#define decode_thread(enc)	((enc >> 8) & 0xFF)

struct cmd_result {
    semaphore  res_sema;		/* waiting for the result */
    header     res_hdr;
    char       res_buf[SP_BUFSIZE];
    bufsize    res_size;
} Result[NTASKS + 1];
/* The last slot is used in case the reply is done by an other member */

/* statistics: */
static int Read_requests = 0;
static int Write_requests = 0;

/* number of replicas required to be in full operation: */
static int Required_replicas;

/* forward declarations: */
static void set_aliveness       _ARGS((g_indx_t memlist[], int nmem));
static void rebuild_group       _ARGS((void));
static void update_config_vec   _ARGS((void));

/* 
 * Auxilairy routines.
 */

int dirsvr_functioning()
{
    return (Grp_mode == MD_FUNCTIONING);
}

int get_sp_min_copies()
{
    return Required_replicas;
}

int get_write_op()
{
    return Write_requests;
}

int get_read_op()
{
    return Read_requests;
}

int get_total_member()
{
    return Grp_nmembers;
}

long get_group_seqno()
{
    return Grp_seqno;
}

errstat
send_to_group(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    hdr->h_port = _sp_getport;
    return grp_send(Grp_id, hdr, buf, (uint32) size);
}

/*
 * Support routines for sp_server.
 */

void
process_group_requests()
{
    /* We can start processing a read request, as soon as all the write
     * requests, that were generated prior to it, are processed.
     * To determine this, we have to examine the current group state.
     */
    g_seqcnt_t expect, nextseq, unstable, until;
    errstat    err;

    if ((err = grp_info(Grp_id, &_sp_getport, &Grp_state,
			Grp_memlist, Grp_max_members)) != STD_OK)
    {
	scream("process_group_requests: grp_info failed, err = %d", err);
	return;
    }

    expect = Grp_state.st_expect;
    nextseq = Grp_state.st_nextseqno;
    unstable = Grp_state.st_unstable;

    if (expect != nextseq || expect != unstable) {
	/* something interesting happened */
	dbmessage(2, ("grp_info: seq=%ld, expect=%ld, next=%ld, unstable=%ld",
		      (long) Grp_seqno, (long) expect,
		      (long) nextseq, (long) unstable));
    }

    until = (unstable > expect) ? unstable : expect;
    bar_wait_for(Grp_barrier, (long) until);
}

static void
sp_read(res, req_hdr, req_buf, req_size)
struct cmd_result *res;
header 	*req_hdr;
char 	*req_buf;
int	 req_size;
{
    command cmd = req_hdr->h_command;
    errstat err;

    /* A read request; perform it immediately. */
    Read_requests++;

    res->res_size = sp_trans(req_hdr, req_buf, req_size,
			     res->res_buf, SP_BUFSIZE);
    res->res_hdr = *req_hdr;

    err = ERR_CONVERT(res->res_hdr.h_status);
    if (err != STD_OK) {
	/* May be perfectly valid: many sp_lookup()s in case of command
	 * startup and include-file location will generate a STD_NOTFOUND.
	 */
	dbmessage(2, ("%s failed (%s)", cmd_name(cmd), err_why(err)));
    }
}

#define MAX_WRITE_TRIES		3

static errstat
sp_write(res, threadnr, in_hdr, in_buf, in_buf_end, in_size)
struct cmd_result *res;
int      threadnr;
header 	*in_hdr;
char 	*in_buf;
char 	*in_buf_end;
int	 in_size;
{
    int     try;
    errstat err;

    Write_requests++;

    if (in_hdr->h_command == SP_CREATE) {
	/* Add the random field for the directory to be created.
	 * This has to be done here, because otherwise each member would
	 * generate a different one!
	 * Note that for legal sp_create requests, there will always be
	 * enough room left.
	 */
	port check;

	if (in_buf + in_size + PORTSIZE > in_buf_end) {
	    res->res_hdr.h_status = STD_ARGBAD;
	    return STD_ARGBAD;
	}

	/* Insert the checkfield in front of the original request */
	uniqport(&check);
	(void) memmove((_VOIDSTAR) (in_buf + PORTSIZE),
		       (_VOIDSTAR) in_buf, (size_t) in_size);
	(void) buf_put_port(in_buf, in_buf_end, &check);

	/* adapt request size: */
	in_size += PORTSIZE;
    }

    /* Put extra information in the request header, that allows the group
     * thread to see if and where it should deliver the result.
     * The h_offset field can be used for this, as no Soap request uses that,
     * currently.
     */
    in_hdr->h_offset = encode_src(sp_whoami(), threadnr);

    for (try = 0; try < MAX_WRITE_TRIES; try++) {
	/* We can only execute a modification if enough members are alive.
	 * Note: it can happen that the group is rebuilt during an iteration,
	 * so we have to check each time.
	 */
	if (Grp_mode != MD_FUNCTIONING) {
	    scream("not enough members functioning to execute write cmd");
	    res->res_hdr.h_status = STD_NOTNOW;
	    return STD_NOTNOW;
	}

	err = send_to_group(in_hdr, in_buf, in_size);
	if (err != STD_OK) {
	    /* Send failed;  we retry a few times */
	    dbmessage(1, ("grp_send failed (%s)", err_why(err)));
	    if (try < MAX_WRITE_TRIES - 1) {
		/* Allow the group receive some time to rebuild the 
		 * group before retrying.  
		 */
		sleep(2);
		dbmessage(1, ("retry operation after rebuilding group"));
	    }
	} else {
	    /* block till the result is computed by the group thread: */
	    sema_down(&res->res_sema);
	    return STD_OK; /* though the operation itself may have failed */
	}
    }

    /* No luck; the problems persist */
    dbmessage(1, ("no more retries"));
    res->res_hdr.h_status = STD_NOTNOW;
    return err;
}

static void
sp_server(param, parsize)
char *param;	/* contains the id of this thread */
int   parsize;
{
    struct cmd_result *my_res;
    int     my_thread_nr;
    bufsize n;
    header  req_hdr;
    char    req_buf[SP_BUFSIZE];
    char   *req_buf_end = &req_buf[sizeof(req_buf)];
    errstat err;

    assert(parsize == sizeof(long));
    my_thread_nr = * (long *) param;
    my_res = &Result[my_thread_nr];

    for (;;) {
	req_hdr.h_port = _sp_getport;
	n = getreq(&req_hdr, req_buf, sizeof(req_buf));
	if (ERR_STATUS(n)) {
	    message("sp_server: getreq failed (%s)",
		    err_why((errstat) ERR_CONVERT(n)));
	    continue;
	}

	dbmessage(2, ("got %s request size %d obj %d",
		      cmd_name(req_hdr.h_command),
		      n, prv_number(&req_hdr.h_priv)));

	my_res->res_hdr.h_status = STD_SYSERR;
	my_res->res_size = 0;

	switch (req_hdr.h_command) {
	case STD_INFO:		/* read operations */
	case SP_LIST:
	case SP_GETMASKS:
	case SP_LOOKUP:
	case SP_SETLOOKUP:
	case STD_RESTRICT:
	    /* First handle the pending write requests that arrived before
	     * this read request.
	     */
	    process_group_requests();

	    sp_read(my_res, &req_hdr, req_buf, (int) n);
	    break; 

	case SP_APPEND:		/* write operations */
	    make_room(1);
	    /* fallthrough */
	case STD_TOUCH:
	case STD_NTOUCH:
	case STD_DESTROY:
	case SP_DISCARD:	
	case SP_CREATE:
	case SP_REPLACE:
	case SP_CHMOD:
	case SP_DELETE:
	case SP_INSTALL:
	case SP_PUTTYPE:
	    /* In this case we don't have to wait untill the still outstanding
	     * group requests are handled:  we just insert a new request in the
	     * queue.
	     */
	    err = sp_write(my_res, my_thread_nr,
			   &req_hdr, req_buf, req_buf_end, (int) n);
	    if (err != STD_OK) {
		dbmessage(1, ("sp_write failed (%s)", err_why(err)));
	    }
	    break;

	default:
	    dbmessage(1, ("bad command %d", req_hdr.h_command));
	    my_res->res_hdr.h_status = STD_COMBAD;
	    break;
	}

	putrep(&my_res->res_hdr, my_res->res_buf, my_res->res_size);
    }
}


/*
 * Grp_server listens to the group port and processes broadcast messages.
 */

static void
reset_group()
{
    header   hdr;
    g_indx_t new_members;
    
    dbmessage(1, ("try to recover"));

    hdr.h_port = _sp_getport;
    hdr.h_offset = Grp_this_member;
    hdr.h_extra = sp_whoami();
    hdr.h_command = SP_GRP_RESET;
    if ((new_members = grp_reset(Grp_id, &hdr, (g_indx_t) 1)) <= 0) {
	panic("group recovery failed (%s)", err_why((errstat) new_members));
    }
}

static void
leave_group()
{
    header   hdr;
    errstat  err;

    hdr.h_port = _sp_getport;
    hdr.h_command = SP_GRP_LEAVE;
    hdr.h_offset = Grp_this_member;
    hdr.h_extra = sp_whoami();

    if ((err = grp_leave(Grp_id, &hdr)) != STD_OK) {
	/* Shouldn't fail; we really must leave the group to let the
	 * recovery protocol work.
	 */
	panic("grp_leave() failed (%s)", err_why(err));
    }

    /* wait till we received our own message back */
    dbmessage(1, ("Leaving the group"));
    sema_down(&Sem_leaving);

    dbmessage(1, ("Left the group"));
    Grp_id = -1;
}

static int enough_members();

static void
rebuild_group()
{
    /* One of the members failed, or a new member joined: rebuild the group.
     * If the number of members is smaller than Required_replicas,
     * we can only continue full operation when a new member joins.
     */
    errstat err;

    err = grp_info(Grp_id, &_sp_getport,
		   &Grp_state, Grp_memlist, (g_indx_t) MAXGROUP);
    if (err != STD_OK) {
	/* Can this happen?  If so, what to do about it? */
	scream("rebuild: grp_info failed (%s)", err_why(err));
	return;
    }

    Grp_nmembers = Grp_state.st_total;
    dbmessage(1, ("rebuild: #members %d", Grp_nmembers));

    set_aliveness(Grp_memlist, (int) Grp_nmembers);

    if (enough_members()) {
	switch (Grp_mode) {
	case MD_RECOVERING:
	    /* now we can start bidding */
	    if (Awaiting_bids && (sema_level(&Sem_recovering) < 1)) {
		sema_up(&Sem_recovering);
	    }
	    break;

	case MD_UP_TO_DATE:
	case MD_FUNCTIONING:
	    dbmessage(1, ("enough members to continue functioning"));
	    /* install a new config vec */
	    update_config_vec();
	    break;
	}
    } else {
	dbmessage(1, ("not enough members (%d < %d) to continue functioning",
		      Grp_nmembers, Required_replicas));
	if (Grp_mode != MD_RECOVERING) {
	    Grp_mode = MD_RECOVERING;
	    sema_up(&Sem_need_recovery);
	}
    }
}

/*
 * To avoid blocking the entire group while sending over consistent state
 * to a joining member, we have implemented a more flexible algorithm,
 * allowing a member to recover the directories in multiple consecutive
 * chuncks.  In this solution, the joining and helping member arer only
 * temporarily blocked during the consistent transfer of a batch directories.
 *
 * If the joining member receives (in its group thread) a directory update
 * during recovery, it must perform it in case it alreadt has an up-to-date
 * version of the directory concerned.
 * Otherwise the update will only be performed by the other members
 * (which form a majority of the group), and the joining member will get
 * that new version in a later batch.
 *
 * The problem is that this does not work with commands that (potentially)
 * operate on multiple directories (SP_CREATE and SP_INSTALL).
 * Some directories may already have been recovered, and others might not.
 * The simple solution (though not very flexible) that we currently implement
 * is just plainly to refuse executing them, and restart recovery altogether.
 *
 * An alternative would be first to look carefully which directories
 * are involved (SP_CREATE: the parent + a new one; SP_INSTALL: inspect the
 * request) and see whether all directories involved are already recovered.
 * If so, we can just execute the request as usual.  If not, we should either
 * ignore the command (in case *all* directories involved are not yet
 * recovered) or restart recovery as in the simple solution.
 */

#define safe_cmd(cmd) (((cmd) != SP_CREATE) && ((cmd) != SP_INSTALL))

static void
handle_group_cmd(req_hdr, req_buf, req_size)
header *req_hdr;
char   *req_buf;
int     req_size;
{
    struct cmd_result *res;
    int     from_member;
    int     from_thread;

    /* Always increment the global seqno, irrespective of the fact whether
     * the write operation actually succeeds.  A joining member may,
     * depending on whether the directory is known to be up-to-date,
     * not be able to handle certain updates.  Hence it doesn't know the
     * outcome.
     * Note: currently we increment the seqno on every group command.
     * For some commands don't change the directory contents (e.g., STD_TOUCH)
     * this isn't actually necessary.
     */
    incr_global_seqno();

    from_member = decode_member(req_hdr->h_offset);
    from_thread = decode_thread(req_hdr->h_offset);
    if (from_member < 0 || from_member >= Grp_max_members) {
	panic("grp_server: message from unknown member %d", from_member);
    }

    if (from_member == sp_whoami()) {
	if (from_thread < 0 || from_thread >= NTASKS) {
	    panic("grp_server: message from bad thread %d", from_thread);
	}
	res = &Result[from_thread];
    } else {
	res = &Result[NTASKS]; /* ignore result */
    }

    dbmessage(2, ("request %s from (%d, %d)", cmd_name(req_hdr->h_command),
		  from_member, from_thread));

    if ((Grp_mode != MD_FUNCTIONING) && !safe_cmd(req_hdr->h_command)) {
	/* see comment above */
	panic("cannot handle %s request during recovery",
	      cmd_name(req_hdr->h_command));
    }

    if ((Grp_mode == MD_FUNCTIONING) ||
	rec_recovered(prv_number(&req_hdr->h_priv)))
    {
	command cmd = req_hdr->h_command;
	errstat err;

	priv2pub(&req_hdr->h_port, &req_hdr->h_port);
	res->res_size = sp_trans(req_hdr, req_buf, req_size,
				 res->res_buf, SP_BUFSIZE);
	err = ERR_CONVERT(req_hdr->h_status);
	switch (err) {
	case STD_OK:
	    break;

	case STD_NOMEM:
	    /* This is serious: we could not execute this command because of
	     * lacking memory space, even though we tried to uncache other
	     * directories.  It is highly likely that the other members don't
	     * have this problem, so to avoid inconsistent states we must
	     * panic() here.
	     */
	    panic("out of memory performing %s on %ld",
		  cmd_name(cmd), (long) prv_number(&req_hdr->h_priv));
	    break;

	default:
	    /* `Normal' failures (illegal APPENDS etc.) that happen at all
	     * members consistently.
	     */
	    dbmessage(1, ("%s failed (%s)", cmd_name(cmd), err_why(err)));
	    break;
	}
    } else {
	Grp_missed_update++;

	scream("cannot handle %s: not yet recovered; Grp_seqno = %ld",
	       cmd_name(req_hdr->h_command), (long) Grp_seqno);
	req_hdr->h_status = STD_NOTNOW;
	res->res_size = 0;
    }

    if (from_member == sp_whoami()) {
	/* wake up the initiating thread */
	res->res_hdr = *req_hdr;
	sema_up(&res->res_sema);
    }
}

/* foward declarations: */
static void    got_status       _ARGS((header *hdr, char *buf, int size));
static errstat broadcast_status _ARGS((void));

static void
grp_server(start_seqno, longsize)
long *start_seqno;
int   longsize;
{
    /* Wait for a message from a member of the group and processes it
     * when it arrives. If the source of the message is another thread in
     * this process, wake the thread up after the message is processed.
     * The thread will send the result back to the client process.
     */
    int     size;	/* bufsize? */
    header  req_hdr;
    char    req_buf[SP_BUFSIZE];
    int	    go_on = 1;
    errstat err;
    
    assert(longsize == sizeof(*start_seqno));

    while (go_on && (Grp_id >= 0)) {
	req_hdr.h_port = _sp_getport;
	size = grp_receive(Grp_id, &req_hdr, req_buf, (uint32) sizeof(req_buf),
			   &Grp_more_mess);

	if (size >= 0) {
	    Grp_seqno++;
	    if (Grp_seqno > *start_seqno) {
		switch(req_hdr.h_command) {
		case SP_GRP_RESET:
		    MON_EVENT("GRP_RESET");
		    rebuild_group();
		    break;
		case SP_GRP_JOIN:
		    MON_EVENT("GRP_JOIN");
		    rebuild_group();
		    if ((err = broadcast_status()) != STD_OK) {
			scream("cannot broadcast status (%s)", err_why(err));
		    }
		    break;
		case SP_GRP_LEAVE:
		    MON_EVENT("GRP_LEAVE");
		    if (req_hdr.h_offset == Grp_this_member) {
			dbmessage(1, ("leaving the group myself"));
			go_on = 0;
			/* wake up the thread initiating the leave */
			sema_up(&Sem_leaving);
		    } else {
			dbmessage(1, ("member %ld has left",
				      (long) req_hdr.h_offset));
			rebuild_group();
		    }
		    break;
		case SP_GRP_STATUS:
		    MON_EVENT("GRP_STATUS");
		    got_status(&req_hdr, req_buf, size);
		    break;
		case SP_DIR_SEQS:
		    {
			int joiner = req_hdr.h_extra;
			int helper = req_hdr.h_offset;

			MON_EVENT("GRP_DIRSEQS");
			if (helper == sp_whoami()) {
			    err = rec_send_updates(&req_hdr, req_buf, size);
			    if (err != STD_OK) {
				scream("failed to send updates to Soap%d (%s)",
				       joiner, err_why(err));
			    }
			} else if (joiner == sp_whoami()) {
			    rec_await_updates();
			} /* else it's none of our business */
		    }
		    break;
		case SP_AGE:
		    MON_EVENT("GRP_AGE");
		    sp_age(&req_hdr);
		    break;
		case SP_DEL_DIR:
		    MON_EVENT("GRP_DEL_DIR");
		    age_directory(&req_hdr, req_buf, size);
		    break;
		case SP_TOUCH_DIRS:
		    MON_EVENT("GRP_TOUCH_DIRS");
		    got_touch_list(&req_hdr, req_buf, size);
		    break;
		case SP_SHUTDOWN:
		    go_on = 0;
		    break;
		default:		/* soap commands */
		    handle_group_cmd(&req_hdr, req_buf, size);
		    break;
		}
	    } else {
		dbmessage(1, ("skip %d", Grp_seqno));
	    }

	    /* Awake the threads doing an sp_read() that were waiting
	     * for the current message to be processed.
	     */
	    bar_wake_up(Grp_barrier, (long) Grp_seqno);
	} else {
	    dbmessage(1, ("grp_receive failed (%s)", err_why((errstat) size)));

	    /* Assume a member failed.  TODO: check for specific errors. */
	    reset_group();
	}
    }

    Grp_nthreads--;
    thread_exit();
}

/*
 * 	G R O U P    R E C O V E R Y
 */

/* After a join that makes the group sufficiently big, all members broadcast
 * an extra SP_GRP_STATUS request, telling what their sequence number is.
 * The bid contains the global sequence number saved in block 0.
 */

/* status about the members: */
struct group_status {
    int        gst_alive;	/* currently member of the group        */
    int	       gst_valid;	/* rest of the structure initialised?   */
    int	       gst_id;		/* the static id of this member         */
    int16      gst_mode;	/* is it completely up to date?         */
    int16      gst_statevec;	/* its group state as stored on block 0 */
    int32      gst_time;	/* the time according to this member    */
    sp_seqno_t gst_seqno;	/* its sequence number			*/
    capability gst_supercap;	/* the capabililty for its admin thread */
};
    
static struct group_status Grp_status[MAXGROUP]; /* group member status */

/* config vec flags: */
#define CM_NEWSTYLE	0x1000	/* indicating new-style copymode       */
#define CM_SURVIVED	0x0200	/* member stayed alive		       */
#define CM_RECOVERING	0x0100	/* bit set during recovery             */
#define CM_MEMBER(mem)	(1 << (mem))
#define CM_MEMBER_MASK	0x00ff	/* bit vector of members in last group */


static int
best_seqno(seqno)
sp_seqno_t *seqno;
{
    int sp_id, best;

    /* return the member having the highest seqno */
    zero_seq_no(seqno);
    best = -1;

    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	struct group_status *gstp = &Grp_status[sp_id];

	if (gstp->gst_alive && gstp->gst_valid) {
	    if (less_seq_no(seqno, &gstp->gst_seqno)) {
		/* Status of this member is better or equal.
		 * Only update best in case this member wasn't recovering.
		 */
		if ((gstp->gst_statevec & CM_RECOVERING) != 0) {
		    dbmessage(1, ("ignore seq of recovering Soap %d", sp_id));
		} else {
		    best = sp_id;
		    *seqno = gstp->gst_seqno;
		}
	    }
	}
    }

    return best;
}

static void
invalidate_bids()
{
    /* invalidates the bids at the start of a new bidding round */
    int sp_id;

    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	Grp_status[sp_id].gst_valid = 0;
    }
}

static int
nbits(vec)
int vec;
{
    /* return number of bits < Grp_max_members set in vec */
    int bit, nbits;

    nbits = 0;
    for (bit = 0; bit < Grp_max_members; bit++) {
	if ((vec & (1 << bit)) != 0) {
	    nbits++;
	}
    }

    return nbits;
}

static void
set_aliveness(memlist, nmem)
g_indx_t memlist[];
int      nmem;
{
    int died;		/* bit vector of members that died */
    int survived;	/* bit vector of members that stayed alive */
    int sp_id, mem_id;

    /* Clear all the alive fields, after having saved them in the "died"
     * bit vector.  The members that survived will be removed from this
     * vector after we found them.
     */
    died = 0;
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	if (Grp_status[sp_id].gst_alive) {
	    died |= (1 << sp_id);
	    Grp_status[sp_id].gst_alive = 0;
	}
    }

    /* then set the alive fields for the members that we know */
    survived = 0;
    for (mem_id = 0; mem_id < nmem; mem_id++) {
	int member;
	int found;

	member = memlist[mem_id];
	assert(member >= 0);
	assert(member < Grp_max_members);

	/* see if we have a member with this group id in our table */
	found = 0;
	for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	    if (Grp_status[sp_id].gst_valid &&
		Grp_status[sp_id].gst_id == member)
	    {
		Grp_status[sp_id].gst_alive = 1;
		dbmessage(1, ("Soap %d (member %d) is still alive",
			      sp_id, member));
		survived |= (1 << sp_id);
		found = 1;
		break;
	    }
	}
	if (!found) {
	    dbmessage(1, ("set_aliveness: no member with gid %d", member));
	}
    }

    died &= ~survived;

    dbmessage(1, ("Died member vector: 0x%x", died));
}

static void
init_bids()
{
    /* modify state to contain no living members */
    set_aliveness(Grp_memlist, 0);
    invalidate_bids();
}

static void
write_config_vec(vec)
int vec;
{
    static int prev_config_vec;

    if (prev_config_vec == 0 || vec != prev_config_vec) {
	prev_config_vec = vec;
	dbmessage(1, ("writing new config vector 0x%x", vec));
	set_copymode(vec);
	flush_copymode();
    }
}

static errstat
get_up_to_date()
{
    sp_seqno_t highest_bid, my_seqno;
    int        sp_best;
    errstat    err = STD_OK;

    assert(enough_members());

    sp_best = best_seqno(&highest_bid);
    assert(sp_best >= 0);

    dbmessage(1, ("collect_bids: Soap %d has the highest bid [%ld, %ld]",
		  sp_best, hseq(&highest_bid), lseq(&highest_bid)));

    get_global_seqno(&my_seqno);
    if ((sp_best != sp_whoami()) &&
	(less_seq_no(&my_seqno, &highest_bid) ||
	 (eq_seq_no(&my_seqno, &highest_bid) && (Grp_missed_update > 0))))
    {
	/* Get up to date w.r.t. the state of the member with the
	 * highest sequence number.
	 */
	int vec;

	dbmessage(1, ("Need to get up to date with Soap %d", sp_best));

	/* Set the recovery bit; it will be cleared as soon as we
	 * are completely up-to-date.
	 */
	vec = get_copymode();
	vec |= CM_RECOVERING;
	write_config_vec(vec);

	if ((err = rec_get_state(sp_best)) == STD_OK) {
	    Grp_mode = MD_UP_TO_DATE;
	    /* Note: not yet MD_FUNCTIONING; maybe others have to get up to
	     * date first as well!  The switch to FUNCTIONING state will be
	     * done in all UP_TO_DATE members atomically, as a result of a
	     * broadcast_status group request.
	     */
	}
    } else {
	if (!eq_seq_no(&my_seqno, &highest_bid)) {
	    scream("get_up_to_date: id: %d; mine: (%ld,%ld); high: (%ld,%ld)",
		   sp_best, hseq(&my_seqno), lseq(&my_seqno),
		   hseq(&highest_bid), lseq(&highest_bid));
	    panic("don't trust it");
	}

	/* see comment above */
	Grp_mode = MD_UP_TO_DATE;
	dbmessage(1, ("I am completely up to date"));
    }

    return err;
}

/*
 * Support routines for the group recovery procedure
 */

static long *
alloc_long(val)
long val;
{
    long *mem;

    if ((mem = (long *) malloc(sizeof(long))) == NULL) {
	panic("cannot allocate integer");
    }

    *mem = val;
    return mem;
}

static int
uptodate_vec()
{
    /* create the up-to-date member vector */
    int member_vec;
    int sp_id;

    member_vec = 0;
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	int mem_mode;

	if (sp_id == sp_whoami()) {	/* use my real mode */
	    mem_mode = Grp_mode;
	} else {
	    struct group_status *gstp = &Grp_status[sp_id];

	    if (gstp->gst_valid && gstp->gst_alive) {
		mem_mode = gstp->gst_mode;
	    } else {			/* skip this one */
		continue;
	    }
	}

	if (mem_mode == MD_UP_TO_DATE || mem_mode == MD_FUNCTIONING) {
	    member_vec |= (1 << sp_id);
	}
    }

    return member_vec;
}

long
sp_time()
{
    /* Return the time consistent for the group, but if that's not available
     * yet (typically during recovery) use the time as provided by my own
     * timer.
     */
    if (Grp_time == 0) {
	message("no group time yet");
	return sp_mytime();
    } else {
	return Grp_time;
    }
}

int
has_lowest_id(sp_id)
int sp_id;
{
    int mem_vec, lower_id_vec;

    mem_vec = uptodate_vec();
    lower_id_vec = (1 << sp_id) - 1;
    return ((mem_vec & (1 << sp_id)) != 0 && (mem_vec & lower_id_vec) == 0);
}

void
broadcast_time()
{
    /* Only broadcast the time if I have the lowest id */
    errstat err;

    if ((Grp_mode == MD_FUNCTIONING) && has_lowest_id(sp_whoami())) {
	if ((err =  broadcast_status()) != STD_OK) {
	    scream("could not broadcast the time (%s)", err_why(err));
	}
    }
}

static void
got_status(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    static int saved_statevec;
    struct group_status *gstp;
    g_indx_t   grpindex;
    int	       svrindex;
    int16      member_statevec, my_statevec;
    char      *p, *end;
    int	       do_broadcast = 0;

    grpindex = hdr->h_offset;
    if (grpindex >= Grp_max_members) {
	scream("bad group index %ld in SP_GRP_STATUS message", (long)grpindex);
	return;
    }

    svrindex = hdr->h_extra;
    if (svrindex < 0 || svrindex >= Grp_max_members) {
	scream("bad svr index %d in SP_GRP_STATUS message", svrindex);
	return;
    }
    
    p = buf;
    end = buf + size;

    /* unmarshall the status info */
    gstp = &Grp_status[svrindex];
    p = buf_get_int16(p, end, &member_statevec);
    p = buf_get_int16(p, end, &gstp->gst_mode);
    p = buf_get_int16(p, end, &gstp->gst_statevec);
    p = buf_get_int32(p, end, &gstp->gst_time);
    p = buf_get_seqno(p, end, &gstp->gst_seqno);
    p = buf_get_cap  (p, end, &gstp->gst_supercap);
    if (p == NULL) {
	scream("bad SP_GRP_STATUS format");
	return;
    }

    gstp->gst_id = grpindex;
    gstp->gst_alive = 1;
    gstp->gst_valid = 1;

    if (Grp_mode != MD_FUNCTIONING) {
	dbmessage(1, ("status from Soap %d: (%ld, %ld); mode=%d; state=0x%x",
		      svrindex, hseq(&gstp->gst_seqno), lseq(&gstp->gst_seqno),
		      gstp->gst_mode, gstp->gst_statevec));
    }
    
    /* See if this member doesn't have an up-to-date view of the set of
     * up-to-date members.  If so, tell it about that, otherwise it might
     * never enter FUNCTIONING mode.
     */
    my_statevec = uptodate_vec();
    if (((~member_statevec & my_statevec) & 0xFF) != 0) {
	dbmessage(1, ("send Soap %d (vec = 0x%x) my state vec (0x%x)",
		      svrindex, member_statevec, my_statevec));
	do_broadcast = 1;
    }

    /* If my statevec changed, rebroadcast my state as well.
     * The other members are interested in my view of the member state
     * for recovery purposes.
     */
    if (saved_statevec != my_statevec) {
	dbmessage(1, ("my statevec is now 0x%x (was 0x%x)",
		      my_statevec, saved_statevec));
	saved_statevec = my_statevec;
	do_broadcast = 1;
    }

    switch (Grp_mode) {
    case MD_RECOVERING:
	/* We are not yet fully recovered, so see if there are now enough
	 * members alive to decide who is capable to provide us the most
	 * up-to-date directory table.
	 */
	if (enough_members()) {
	    dbmessage(1, ("enough members are alive for full recovery"));
	    if (Awaiting_bids && (sema_level(&Sem_recovering) < 1)) {
		sema_up(&Sem_recovering);
	    }
	} else {
	    dbmessage(1, ("need status of more members for full recovery"));
	}
	break;

    case MD_UP_TO_DATE:
	/* We are up-to-date, but until now there weren't enough other
	 * members up-to-date.  Check if this now is the case.
	 */
	{
	    int n_uptodate = nbits(member_statevec);

	    if (n_uptodate >= Required_replicas) {
		if ((member_statevec & (1 << sp_whoami())) != 0) {
		    dbmessage(1, ("FUNCTIONING (%d up to date)!", n_uptodate));
		    Grp_best_mode = Grp_mode = MD_FUNCTIONING;
		    sema_up(&Sem_functioning);
		    update_config_vec();
		} else {
		    dbmessage(1, ("Config vec 0x%x does not contain me",
				  member_statevec));
		    do_broadcast = 1;
		}
	    } else {
		dbmessage(1, ("not yet enough members (%d < %d) up-to-date",
			      n_uptodate, Required_replicas));
	    }
	}
	break;

    case MD_FUNCTIONING:
	update_config_vec();
	break;
    }

    /* Update the time if we are now functioning and when we got it from the
     * member with the lowest id.
     */
    if (Grp_mode == MD_FUNCTIONING) {
	if (has_lowest_id(svrindex)) {
	    Grp_time = gstp->gst_time;
	}
    }

    if (do_broadcast) {
	errstat err;

	/* TODO: make sure that an avalanche of status broadcasts cannot
	 * happen.  Maybe we also need help from a thread that periodically
	 * sends our status to the group during recovery.
	 */
	if ((err = broadcast_status()) != STD_OK) {
	    scream("could not broadcast status (%s)", err_why(err));
	}
    }
}

static errstat
broadcast_status()
{
    sp_seqno_t my_seqno;
    header     hdr;
    char       buf[200];
    char      *p, *end;
    int16      configvec;

    p = buf;
    end = buf + sizeof(buf);

    /* The status being broadcast currently consists of:
     * - a vector of members I know to be up-to-date
     * - the group index of this member
     * - the (static) index of this member (debugging?)
     * - whether this member is already recovered
     * - the group configuration vector as stored on superblock 0
     * - my current timer value
     * - the sequence number of this member
     * - the capability that can be used to contact this process for recovery
     */
    get_global_seqno(&my_seqno);
    configvec = get_copymode();
    if (Grp_best_mode == MD_FUNCTIONING) {
	configvec |= CM_SURVIVED;
    }

    p = buf_put_int16(p, end, (int16) uptodate_vec());
    p = buf_put_int16(p, end, (int16) Grp_mode);
    p = buf_put_int16(p, end, configvec);
    p = buf_put_int32(p, end, (int32) sp_mytime());
    p = buf_put_seqno(p, end, &my_seqno);
    p = buf_put_cap  (p, end, &my_supercap);
    if (p == NULL) {
	return STD_SYSERR;
    }

    MON_EVENT("grp_send status");
    dbmessage(2, ("sending status to group"));

    hdr.h_command = SP_GRP_STATUS;
    hdr.h_offset = Grp_this_member;
    hdr.h_extra = sp_whoami();
    return send_to_group(&hdr, buf, p - buf);
}

static void
init_copymode()
{
    /* Inspect the copy mode on disk, and see if it need to be fixed.
     * The "copymode" in the original Soap server was just 1 or 2,
     * but now we need to store more information.
     */
    int copymode;

    copymode = get_copymode();
    if ((copymode & CM_NEWSTYLE) == 0) {
	int member, set;

	dbmessage(1, ("old style copymode, installing new config vec"));

	/* modify it to contain a configuration with the least required
	 * number of members that includes us.
	 */
	copymode = CM_MEMBER(sp_whoami());
	for (member = 0, set = 1;
	     (member < Grp_max_members) && (set < Required_replicas);
	     member++)
	{
	    if (member != sp_whoami()) {
		copymode |= CM_MEMBER(member);
		set++;
	    }
	}

	/* also set the "new-style" bit, and write it to disk */
	copymode |= CM_NEWSTYLE;
	write_config_vec(copymode);
    }

    message("config vector is 0x%x", copymode);
}

static void
update_config_vec()
{
    /* write a new configuration vector to the superblock,
     * containing a bit for every member functioning.
     */
    int sp_id, nfunctioning;
    int vec;

    vec = CM_NEWSTYLE;
    nfunctioning = 0;
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	struct group_status *gstp = &Grp_status[sp_id];

	/* Is it correct to include both UP_TO_DATE and FUNCTIONING members? */
	if (gstp->gst_valid && gstp->gst_alive &&
	    (gstp->gst_mode == MD_UP_TO_DATE ||
	     gstp->gst_mode == MD_FUNCTIONING))
	{
	    vec |= CM_MEMBER(sp_id);
	    nfunctioning++;
	}
    }

    if (nfunctioning >= Required_replicas) {
	write_config_vec(vec);
    } else {
	message("don't write config vec 0x%x (not enough members)", vec);
    }
}

static int
enough_members()
{
    int sp_id, best;
    int n_valid;
    sp_seqno_t highseq;

    /* first check that a majority is up */
    n_valid = 0;
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	if (Grp_status[sp_id].gst_alive && Grp_status[sp_id].gst_valid) {
	    n_valid++;
	}
    }

    if (n_valid < Required_replicas) {
	message("enough_members: need status of %d more members",
		Required_replicas - n_valid);
	return 0;
    }

    /* If we are no longer recovering, we don't have to check the member
     * states for the other members.  We only need to check that the
     * recovered living members still form a majority.
     */
    if (Grp_mode != MD_RECOVERING) {
	return (nbits(uptodate_vec()) >= Required_replicas);
    }

    best = best_seqno(&highseq);
    if (best < 0) {
	/* This is possible (though not very likely) in case all current
	 * members crahshed while recovering from a (now absent) member.
	 */
	scream("enough_members: no member has a correct seqno");
	return 0;
    }

    /* Now take the state vecs into account.
     * If one of the members is a survivor of a previous group incarnation
     * and all the current other members have a lower seqno, then we can be
     * sure that no absent member has a higher seqno.
     *
     * Actually, in case of "Grp_max_members >= 5" this is still not optimal:
     * two members that survived from the same group have the same seqno,
     * and this should also be allowed.  So in general we must also take
     * the last functional member state for that member into account.
     * For "Grp_max_members == 3" our current simple rule is good enough
     * in most cases.
     */
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	struct group_status *gstp;

	gstp = &Grp_status[sp_id];
	if (gstp->gst_valid && gstp->gst_alive &&
	    (gstp->gst_statevec & CM_SURVIVED) != 0)
	{
	    if (eq_seq_no(&gstp->gst_seqno, &highseq)) {
		int id;

		for (id = 0; id < Grp_max_members; id++) {
		    if (id != sp_id) {
			struct group_status *mem_stp = &Grp_status[id];

			if (mem_stp->gst_valid && mem_stp->gst_alive &&
			    !less_seq_no(&mem_stp->gst_seqno, &highseq))
			{
			    message("seq Soap%d >= seq Soap%d", id, sp_id);
			    break;
			}
		    }
		}

		if (id == Grp_max_members) {
		    message("surviving Soap%d has highest seq (%ld, %ld)",
			    sp_id, hseq(&highseq), lseq(&highseq));
		    return 1;
		}
	    } else {
		message("surviving Soap %d is not up-to-date", sp_id);
		/* keep looking for more survivors */
	    }
	}
    }

    /* If there was no survivor with the highest seqno, we must take a look
     * at the dependencies as stored on the commit block of the respective
     * members.
     */
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	struct group_status *gstp;

	gstp = &Grp_status[sp_id];
	if (gstp->gst_alive && gstp->gst_valid) {
	    /* Check if this member is not dependent on a member that is
	     * currently not part of the group.
	     * We only need to check the members having the highest (valid)
	     * sequence number.
	     */
	    if ((Grp_mode == MD_FUNCTIONING) ||
		eq_seq_no(&gstp->gst_seqno, &highseq))
	    {
		int member_vec, mem;

		member_vec = (gstp->gst_statevec & CM_MEMBER_MASK);
		for (mem = 0; mem < Grp_max_members; mem++) {
		    if (((member_vec & CM_MEMBER(mem)) != 0) &&
			(!Grp_status[mem].gst_alive ||
			 !Grp_status[mem].gst_valid))
		    {
			dbmessage(1, ("Soap%d requires absent Soap%d !",
				      sp_id, mem));
			return 0;
		    }
		}
	    } else {
		dbmessage(1, ("enough_members: Soap%d depends on Soap%d",
			      sp_id, best));
	    }
	}
    }

    /* passed all checks */
    return 1;
}



/*
 * Group Recovery Algorithm:
 *
 * Phase1:  During a certain period (poss. member dependent) try to grp_join().
 *	    If none of these tries succeeds: create a group ourselves.
 *	    After having joined or created the group: enter Phase 2.
 *
 * Phase2:  Wait until there are enough members.
 *	    After a certain period, a member will decide that we have
 *	    waited long enough, and to avoid possible deadlock
 *	    (i.e. the situation where we have several independent groups,
 *	    each lacking enough members) all existing members should leave
 *	    the group.  The recovery must than be restarted in Phase 1.
 */

#define MAXBUILDTIME  ((interval) (180 * 1000)) /* three minutes */

errstat
recover_group()
{
    long    *grp_startseq = NULL;
    header   hdr;
    errstat  err;

    init_copymode();

    /*
     * Phase 1
     */
    Grp_mode = MD_RECOVERING;

    if (Grp_id < 0) {
	int try, ntries;

	ntries = 3 + 2 * sp_whoami();
	for (try = 0; try < ntries; try++) {
	    hdr.h_port = _sp_getport;
	    hdr.h_offset = Grp_this_member;
	    hdr.h_extra = sp_whoami();
	    hdr.h_command = SP_GRP_JOIN;
	    Grp_id = grp_join(&hdr);
	    if (Grp_id < 0) {
		message("grp_join failed (%s)", err_why((errstat) Grp_id));
	    } else {
		message("grp_join succeeded at try %d!", try);
		break;
	    }

	    sleep(2);
	}
    }

    if (Grp_id < 0) {
	g_indx_t resilience = Grp_max_members - 1;

	message("building the group myself");
	Grp_id = grp_create(&_sp_getport, resilience, Grp_max_members,
			    (uint32) LOGBUF, (uint32) LOGDATA);
	if (Grp_id < 0) {
	    panic("group create failed (%s)", err_why((errstat) Grp_id));
	}
    }

    err = grp_set(Grp_id, &_sp_getport,
		  (interval) SYNCTIME, (interval) REPLYTIME,
		  (interval) ALIVETIME, (uint32) LARGEMESS);
    if (err != STD_OK) {
	panic("grp_set failed (%s)", err_why(err));
    }
#ifdef GRP_DEBUG
    /* Set group debug level in the kernel by means of the following hack: */
    (void) grp_set(Grp_id, &_sp_getport,
		  (interval) SYNCTIME, (interval) REPLYTIME,
		  (interval) GRP_DEBUG, (uint32) LARGEMESS);
#endif

    /*
     * Phase 2
     */
    if ((err = grp_info(Grp_id, &_sp_getport, &Grp_state,
			Grp_memlist, (g_indx_t) MAXGROUP)) != STD_OK) {
	panic("grp_info failed (%s)", err_why(err));
    }
    Grp_nmembers = Grp_state.st_total;
    Grp_seqno = Grp_state.st_expect;
    Grp_this_member = Grp_state.st_myid;
    Grp_more_mess = 0;

    dbmessage(1, ("st_expect = %ld; st_nextseq = %ld; st_unstable = %ld",
		  Grp_state.st_expect, Grp_state.st_nextseqno,
		  Grp_state.st_unstable));
    dbmessage(1, ("Grp_nmembers = %d; Grp_this_member = %d",
		  Grp_state.st_total, Grp_state.st_myid));
    
    /* allocate the group sequence number */
    grp_startseq = alloc_long((long) Grp_seqno);

    /* make sure there is a group thread listening */
    if (Grp_nthreads == 0) {
	if (!thread_newthread(grp_server, GRP_STACKSIZE,
			      (char *) grp_startseq, sizeof(*grp_startseq))) {
	    panic("cannot start the grp_server thread");
	}
	Grp_nthreads++;
    }

    invalidate_bids();

    /* From now on do a sema_up on Sem_recovering if enough bids arive */
    Awaiting_bids = 1;

    /* make our sequence number known */
    if ((err = broadcast_status()) != STD_OK) {
	panic("could not broadcast status (%s)", err_why(err));
    }

    if (!enough_members()) {
	message("waiting for enough members to join");

	/* wait until enough members have joined or until the timer expires */
	(void) sema_trydown(&Sem_recovering, MAXBUILDTIME);
    }

    /* Not interested in more bids */
    Awaiting_bids = 0;

    /*
     * End of Phase 2: see if we have enough members.
     * If so, try to get up to date.
     */
    if (enough_members()) {
	if ((err = get_up_to_date()) != STD_OK) {
	    scream("could not get up to date");
	}
    } else {
	scream("could not find enough members");
	err = STD_NOTNOW;
    }

    if (err == STD_OK) {
	/* Make our (possibly updated) sequence number known,
	 * as well as we the fact that we are up-to-date.
	 * This is needed for deciding whether enough members are now
	 * up-to-date to become fully operational again.
	 */
	if ((err = broadcast_status()) != STD_OK) {
	    scream("could not broadcast status (%s)", err_why(err));
	}
    } else {
	leave_group();
    }

    return err;
}

/*
 * Become member of the group or create the group.
 */

void
startgroup(min_copies, nmembers)
int min_copies;
int nmembers;
{
    sp_seqno_t dir_seqno, ml_seqno;
    int nstarted;
    int i;

    if (min_copies <= 0) {
	Required_replicas = MIN_COPIES(nmembers);
    } else {
	Required_replicas = min_copies;
    }

    Grp_max_members = nmembers;
    if (Grp_max_members > MAXGROUP) {
	panic("bad nmembers parameter (max %d)", MAXGROUP);
    }

    for (i = 0; i < NTASKS + 1; i++) {
	sema_init(&Result[i].res_sema, 0);
    }
    sema_init(&Sem_recovering, 0);
    sema_init(&Sem_need_recovery, 0);
    sema_init(&Sem_leaving, 0);
    sema_init(&Sem_functioning, 0);

    Grp_barrier = bar_create(0L /* we don't know the right value yet */);

    /* first recover all pending modifications */
    highest_dir_seqno(&dir_seqno);

#if 1
/* temporary sanity check; something went wrong .. */
    if (hseq(&dir_seqno) != 0) {
	scream("unexpected highest dir seq (%ld, %ld); normalizing it..",
	       hseq(&dir_seqno), lseq(&dir_seqno));
	dir_seqno.seq[0] = 0;
    }
#endif

    ml_recover(&ml_seqno);
#if 1
/* temporary sanity check; something went wrong .. */
    if (hseq(&ml_seqno) != 0) {
	scream("unexpected highest modlist seq (%ld, %ld); normalizing it..",
	       hseq(&ml_seqno), lseq(&ml_seqno));
	ml_seqno.seq[0] = 0;
    }
#endif

    message("highest dir seqno is (%ld, %ld)",
	    hseq(&dir_seqno), lseq(&dir_seqno));
    message("highest modlist seqno is (%ld, %ld)",
	    hseq(&ml_seqno), lseq(&ml_seqno));
    if (less_seq_no(&dir_seqno, &ml_seqno)) {
	set_global_seqno(&ml_seqno);
    } else {
	set_global_seqno(&dir_seqno);
    }

    /* wait until a group of sufficient size has been created */
    message("group port = %s", ar_port(&_sp_getport));

    init_bids();

    /* from now on we try to save modifications in the mod list */
    ml_use();

    start_adm_server();

    /* wait until will have recovered and enough members are up-to-date: */
    while (Grp_mode != MD_FUNCTIONING) {

	if (Grp_mode == MD_RECOVERING) {
	    while (recover_group() != STD_OK) {
		scream("group recovery failed; retry");
	    }
	}

	/* wait till enough other members are up-to-date as well */
	if (Grp_mode != MD_FUNCTIONING) {
	    if (sema_trydown(&Sem_functioning, MAXBUILDTIME) != 0) {
		scream("not enough members up-to-date yet; restart");
		leave_group();
	    }
	}
    }

    sp_print_table();

    for (nstarted = 0; nstarted < NTASKS; nstarted++) {
        if (!thread_newthread(sp_server, SVR_STACKSIZE,
			      (char*)alloc_long((long)nstarted), sizeof(long)))
	{
            scream("thread_newthread sp_server failed");
            break;
        }
    }
    if (nstarted == 0) {
        panic("no worker threads started");
    } else {
        message("%d threads started", nstarted);
    }

    for (;;) {
	/* From now on, act as a recovery thread.
	 * First wait until our help is needed.
	 */
	sema_down(&Sem_need_recovery);

	while (recover_group() != STD_OK) {
	    scream("group recovery failed; retry");
	}
    }
}

