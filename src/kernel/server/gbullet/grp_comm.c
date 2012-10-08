/*	@(#)grp_comm.c	1.1	96/02/27 14:07:18 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * GRP_COMM.C
 *
 *	This file contains code for building up the group connection,
 *	receiving group requests and dispatching them to the appropriate
 *	group implementation stubs.
 *
 * Author: Kees Verstoep
 * Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#ifdef USER_LEVEL
#include "thread.h"
#else
#include "sys/proto.h"
#include "sys/kthread.h"
#endif
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
#ifndef SOAP_GROUP
#define SOAP_GROUP      /* temp hack to get the DIRSVR group commands.. */
#endif
#include "soap/soapsvr.h"
#include "module/buffers.h"
#include "module/mutex.h"
#include "module/rnd.h"
#include "module/ar.h"
#include "module/prv.h"
#include "ax/server.h"
#include "monitor.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "stats.h"
#include "bs_protos.h"
#include "preempt.h"
#include "grp_defs.h"
#include "grp_comm.h"
#include "grp_impl.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

#define LOCK_SEND	1
#ifdef LOCK_SEND
/* Try to avoid a bug in the group code that causes hanging sends.
 */
static errstat grp_xsend _ARGS((g_id_t,header *,bufptr,uint32)) ;
#else
# define grp_xsend grp_send
#endif

/* We have missing members, I should reply for them when out-of-date
 * information comes in.
 */

int	bs_mopup ;

extern  Superblk Superblock;     /* in-core copy of superblock */

#define MAXBUILDTIME  ((interval) (1800 * 1000)) /* thirty minutes */
#define MAXSENDRETRY		200
#define SENDFAILWAITTIME	((interval)5000)


/* debugging */
#define GRP_DEBUG	0	/* group kernel debug level */

/* group defines */    
#define LOGDATA 	13	/* log of the maximum size broadcast msg  */
#define MAXDATA		(1 << LOGDATA)
#define LOGBUF 		6	/* log of the number of buffers used */
#define SYNCTIME	8000	/* (msec) time between sychronization */
#define ALIVETIME	5000	/* (msec) time between checking liveness */
#define REPLYTIME	1000	/* (msec) time before resending a msg */
#define LARGEMESS	2000	/* when to use method 2 */

/* group variables */
static port       Grp_port;
static g_indx_t   Grp_max_members ;
static g_indx_t	  Grp_resilience ;
static g_indx_t   Grp_this_member ;
static g_id_t	  Grp_id ;
static g_indx_t   Grp_memlist[MAXGROUP];
static int 	  Grp_more_mess ;
static g_indx_t   Grp_nmembers;
static grpstate_t Grp_state;
static int	  Grp_nthreads ; /* group threads listening */

static int        My_static_id ;
static int        Did_my_static_id ;
static capability my_supercap;

/* For recovery, there are 3 modes: */
#define MD_RECOVERING	0	/* still determining partners; bidding  */
#define MD_FUNCTIONING	2	/* the members ttl states are ok */

static int Grp_mode ;

/* synchronization variables */
static int        Awaiting_bids;	/* restricts use of Sem_recovering   */
static semaphore  Sem_recovering;	/* wait till enough members joined   */
static semaphore  Sem_functioning;	/* wait till enough members recovered*/
static semaphore  Sem_leaving;		/* wait till I get my own leave msg  */
static semaphore  Sem_need_recovery;	/* wait till recovery is needed      */
static semaphore  waiter_sema ;		/* Just here for timeouts */



/* number of replicas required to be in full operation: */
static int Required_replicas;

/* forward declarations: */
static void set_aliveness       _ARGS((g_indx_t memlist[], int nmem));
static void rebuild_group       _ARGS((void));
static void update_config_vec   _ARGS((void));
static int members_exist	_ARGS((void));
static int uptodate_vec		_ARGS((void));


/* 
 * Auxilairy routines.
 */

void
bs_gc_init() {
	Grp_id = -1;
	Grp_mode = MD_RECOVERING;
}

void
grp_init_port(id, p, cap)
int id;
port *p;
capability *cap;
{
    My_static_id = id;
    Did_my_static_id = 1 ;
    Grp_port = *p;
    my_supercap = *cap;
}

int grp_static_id()
{
    if ( Did_my_static_id ) return My_static_id;
    return -1 ;
}

#ifdef LOCK_SEND
static errstat
grp_xsend(gid, hdr, data, size)
    g_id_t	gid;
    header	*hdr;
    bufptr	data;
    uint32	size ;
{
	errstat	err ;
	static mutex send_lock ;

	mu_lock(&send_lock);
	err= grp_send(gid, hdr, data, size);
	mu_unlock(&send_lock);
	return err;
}
#endif

#ifdef notdef
errstat
grp_send_request(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
  bpanic("grp_send_request");
    if (Grp_id >= 0) {
	hdr->h_port = Grp_port;
	return grp_xsend(Grp_id, hdr, buf, (uint32) size);
    } else {
	bwarn("tried to send group request while group not available");
	return STD_NOTNOW;
    }
}
#endif

/* same as above, but queue if busy */
void
bs_grp_trysend(hdr, buf, size, mode)
header *hdr;
char   *buf;
int     size;
int	mode;
{
    int			counter ;
    errstat		err ;

    hdr->h_port = Grp_port;
    
    /* extra always contains the sending id
     */
    hdr->h_extra = My_static_id ;
    counter=MAXSENDRETRY ;
    for(;;) {
	if (Grp_id < 0 ||
	   (err=grp_xsend(Grp_id, hdr, buf, (uint32) size))!=STD_OK ) {
	    dbmessage(1,("error %s from grp_send",err_why(err)));
	    if ( counter-- == 0 ) bpanic("Waiting too long in grp_trysend");
	    sema_trydown(&waiter_sema,SENDFAILWAITTIME) ;
	} else {
	    return ;
	}
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

    hdr.h_port = Grp_port;
    hdr.h_offset = Grp_this_member;
    hdr.h_extra = My_static_id;
    hdr.h_command = SP_GRP_RESET;
    if ((new_members = grp_reset(Grp_id, &hdr, (g_indx_t) 1)) <= 0) {
	gpanic(("group recovery failed (%s)", err_why((errstat) new_members)));
    }
}

void
bs_leave_group()
{
    header   hdr;
    errstat  err;

    hdr.h_port = Grp_port;
    hdr.h_command = SP_GRP_LEAVE;
    hdr.h_offset = Grp_this_member;
    hdr.h_extra = My_static_id;

    if ((err = grp_leave(Grp_id, &hdr)) != STD_OK) {
	/* Shouldn't fail; we really must leave the group to let the
	 * recovery protocol work.
	 */
	gpanic(("grp_leave() failed (%s)", err_why(err)));
    }

    /* wait till we received our own message back */
    dbmessage(1, ("Leaving the group"));
    sema_down(&Sem_leaving);

    dbmessage(1, ("Left the group"));
    Grp_id = -1;
}

static void
rebuild_group()
{
    /* One of the members failed, or a new member joined: rebuild the group.
     */
    errstat err;

    err = grp_info(Grp_id, &Grp_port,
		   &Grp_state, Grp_memlist, (g_indx_t) MAXGROUP);
    if (err != STD_OK) {
	/* Can this happen?  If so, what to do about it? */
	scream("rebuild: grp_info failed (%s)", err_why(err));
	return;
    }

    Grp_nmembers = Grp_state.st_total;
    dbmessage(1, ("rebuild: #members %d", Grp_nmembers));

    set_aliveness(Grp_memlist, (int) Grp_nmembers);

    if (members_exist()) {
	if (Grp_mode == MD_RECOVERING) {
	    /* now we can start asking for ttl's */
	    if (Awaiting_bids && (sema_level(&Sem_recovering) < 1)) {
		sema_up(&Sem_recovering);
	    }
	}
    } else {
	if (Grp_mode != MD_RECOVERING) {
	    bwarn("No member is aware of any ttl's");
	    Grp_mode = MD_RECOVERING;
	    sema_up(&Sem_need_recovery);
	}
    }
    update_config_vec();

}

/* forward declarations: */
static void    got_status       _ARGS((header *hdr, char *buf, int size));
static errstat broadcast_status _ARGS((void));

#if defined(USER_LEVEL)
/*ARGSUSED*/
static void
grp_server(p, s)
char *  p;
int     s;
#else  /* Kernel version */
static void
grp_server()
#endif
{
    /* Wait for a message from a member of the group and processes it
     * when it arrives. If the source of the message is another thread in
     * this process, wake the thread up after the message is processed.
     * The thread will send the result back to the client process.
     */
    int     size;	/* bufsize? */
    header  req_hdr;
    char    req_buf[BS_GRP_REQSIZE];
    int	    go_on = 1;
    int	    in_r ;
    int     in_diff ;
    
    while (go_on && (Grp_id >= 0)) {
	req_hdr.h_port = Grp_port;
	size = grp_receive(Grp_id, &req_hdr, req_buf, (uint32) sizeof(req_buf),
			   &Grp_more_mess);

	if (size >= 0) {
		in_r= (req_hdr.h_extra>>8)&0xFF ;
		req_hdr.h_extra &= 0xFF ;
		switch(req_hdr.h_command) {
		case SP_GRP_RESET:
		    MON_EVENT("GRP_RESET");
		    rebuild_group();
		    gs_memb_crash() ;
		    break;
		case SP_GRP_JOIN:
		    MON_EVENT("GRP_JOIN");
		    rebuild_group();
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
		case SP_SHUTDOWN:
		    go_on = 0;
		    break;
		default:		/* soap commands */
		    grp_got_req(&req_hdr, req_buf, size);
		    break;
		}
		grp_handled_req();
	} else {
	    dbmessage(1, ("grp_receive failed (%s)", err_why((errstat) size)));

	    /* Assume a member failed.  TODO: check for specific errors. */
	    reset_group();
	}
    }

    Grp_nthreads--;
#ifdef USER_LEVEL
    thread_exit();
#else
    exitthread(NULL) ;
#endif
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
    int16      gst_seen;	/* The members it has seen all inodes   */
    int32      gst_time;	/* the time according to this member    */
    Seqno_t    gst_seqno;	/* its sequence number (useless)	*/
    capability gst_supercap;	/* the capabililty for its admin thread */
};
    
static struct group_status Grp_status[MAXGROUP]; /* group member status */

/* config vec flags: */
#define CM_MEMBER(mem)	(1 << (mem))
#define CM_MEMBER_MASK	0x00ff	/* bit vector of members in last group */


static int
my_guru()
{
    int sp_id, best, i;
    struct group_status *gstp;

    /* find the `next higher' active member */
    best = -1;

    for (i = 0; i < Grp_max_members; i++) {
	sp_id= (Grp_this_member+i)%Grp_max_members ;
	gstp = &Grp_status[sp_id];

	if (gstp->gst_alive && gstp->gst_valid) {
	    if (gstp->gst_mode==MD_RECOVERING) {
		dbmessage(1, ("ignore seq of recovering member %d",sp_id));
	    } else {
		best = sp_id ;
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

static int
has_lowest_id(sp_id)
int sp_id;
{
    int mem_vec, lower_id_vec;

    mem_vec = uptodate_vec();
    lower_id_vec = (1 << sp_id) - 1;
    return ((mem_vec & (1 << sp_id)) != 0 && (mem_vec & lower_id_vec) == 0);
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
		dbmessage(1, ("Member %d (grp member %d) is still alive",
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
    if ( ! ( survived & (1<<Superblock.s_member) ) ) message("I am dead???!!");

    died &= ~survived;

    dbmessage(1, ("Died member vector: 0x%x", died));

    if ( has_lowest_id(My_static_id) ) {
	bs_mopup=1 ;
	dbmessage(3,("I am the hunky dory"));
    } else {
	bs_mopup=0 ;
    }
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
	dbmessage(1, ("new config vector 0x%x", vec));
	set_copymode(vec);
	flush_copymode();
    }
}

static errstat
get_up_to_date(mode)
    int	       mode;
{
    int        sp_best;
    errstat    err = STD_OK;

    sp_best = my_guru();
    if ( sp_best<0 ) return STD_NOTNOW ;

    dbmessage(1, ("Need to get up to date with member %d", sp_best));


    if ((err = grp_get_state(sp_best,mode,(interval)MAXBUILDTIME)) == STD_OK) {
	Grp_mode = MD_FUNCTIONING;
	gs_allow_aging();
    }
    return err;
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

	if (sp_id == My_static_id) {	/* use my real mode */
	    mem_mode = Grp_mode;
	} else {
	    struct group_status *gstp = &Grp_status[sp_id];

	    if (gstp->gst_valid && gstp->gst_alive) {
		mem_mode = gstp->gst_mode;
	    } else {			/* skip this one */
		continue;
	    }
	}

	if (mem_mode == MD_FUNCTIONING) {
	    member_vec |= (1 << sp_id);
	}
    }

    return member_vec;
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

    svrindex = hdr->h_extra & 0xFF ; 
    if (svrindex < 0 || svrindex >= Grp_max_members) {
	scream("bad svr index %d in SP_GRP_STATUS message", svrindex);
	return;
    }

    dbmessage(12,("status from member %d[%d]",svrindex,grpindex));

    /* Check for two diffent members with same static id */
    if ( svrindex==My_static_id && grpindex!=Grp_this_member ) {
	/* Ouch.... */
	gpanic(("Two invocations of same member, quitting...")) ;
    }

    /* unmarshall the status info */
    p = buf;
    end = buf + size;

    gstp = &Grp_status[svrindex];
    p = buf_get_int16(p, end, &member_statevec);
    p = buf_get_int16(p, end, &gstp->gst_mode);
    p = buf_get_int16(p, end, &gstp->gst_seen);
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

    dbmessage(11, ("status from member %d: mode=%d; state=0x%x",
		      svrindex, gstp->gst_mode, gstp->gst_seen));
    
    /* See if this member doesn't have an up-to-date view of the set of
     * up-to-date members.  If so, tell it about that, otherwise it might
     * never speak to us.
     */
    my_statevec = uptodate_vec();
    if (((~member_statevec & my_statevec) & CM_MEMBER_MASK) != 0) {
	dbmessage(11, ("send member %d (vec = 0x%x) my state vec (0x%x)",
		      svrindex, member_statevec, my_statevec));
	do_broadcast = 1;
    }

    /* See if we have new members that broadcasted their inodes */
    gs_flagnew(gstp->gst_seen) ;

    /* My statevec changed.
     */
    if (saved_statevec != my_statevec) {
	dbmessage(11, ("my statevec is now 0x%x (was 0x%x)",
		      my_statevec, saved_statevec));
	saved_statevec = my_statevec;
	if ( has_lowest_id(My_static_id) ) {
	    bs_mopup=1 ;
	    dbmessage(3,("I am the hunky dory"));
	} else {
	    bs_mopup=0 ;
	}
    }

    update_config_vec();

    if (members_exist()) {
	if (Grp_mode == MD_RECOVERING) {
	    /* now we can start asking for ttl's */
	    if (Awaiting_bids && (sema_level(&Sem_recovering) < 1)) {
		sema_up(&Sem_recovering);
	    }
	}
    } else {
	if (Grp_mode != MD_RECOVERING) {
	    bwarn("No member is aware of any ttl's");
	    Grp_mode = MD_RECOVERING;
	    sema_up(&Sem_need_recovery);
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
    Seqno_t my_seqno;
    header     hdr;
    char       buf[200];
    char      *p, *end;

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
    my_seqno=0 ;
    p = buf_put_int16(p, end, (int16) uptodate_vec());
    p = buf_put_int16(p, end, (int16) Grp_mode);
    p = buf_put_int16(p, end, (int16) bs_members_seen);
    p = buf_put_int32(p, end, (int32) sp_mytime());
    p = buf_put_seqno(p, end, &my_seqno);
    p = buf_put_cap  (p, end, &my_supercap);
    if (p == NULL) {
	return STD_SYSERR;
    }

    MON_EVENT("grp_send status");
    dbmessage(12, ("sending status to group"));

    hdr.h_command = SP_GRP_STATUS;
    hdr.h_offset = Grp_this_member;
    bs_grpreq(&hdr, buf, p - buf, SEND_PRIORITY|SEND_NOWAIT);
    return STD_OK ;
}

static void
update_config_vec()
{
    /* write a new configuration vector to the superblock,
     * containing a bit for every member functioning.
     */
    int sp_id, nfunctioning;
    int vec;

    vec=0 ;
    nfunctioning = 0;
    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	struct group_status *gstp = &Grp_status[sp_id];

	/* Is it correct to include both UP_TO_DATE and FUNCTIONING members? */
	/* For the gbullet it is.... */
	if (gstp->gst_valid && gstp->gst_alive &&
	    gstp->gst_mode == MD_FUNCTIONING)
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
members_exist()
{
    int sp_id;
    struct group_status *gstp;

    for (sp_id = 0; sp_id < Grp_max_members; sp_id++) {
	gstp = &Grp_status[sp_id];

	if (gstp->gst_alive && gstp->gst_valid &&
	    gstp->gst_mode==MD_FUNCTIONING) {
		return 1 ;
	}
    }
    return 0;
}


void
bs_be_zombie()
{
	Grp_mode= MD_RECOVERING ;
	broadcast_status() ;
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
/*	    Mode should be SEND_NOWAIT when called from grp_receive thread.
 *	    This never happans at the moment. If that happans, one has
 *	    to look at grp_get_state to reduce the group messages sent
 *	    by send_inodes. They should be sent by a separate thread.
 */

static errstat
recover_group(mode)
    int	     mode;
{
    header   hdr;
    errstat  err;

    /*
     * Phase 1
     */
    Grp_mode = MD_RECOVERING;

    if (Grp_id < 0) {
	int try, ntries;

#ifdef notdef
	ntries = 3 + 2 * My_static_id;
#else
	ntries = 1 + My_static_id;
#endif
	for (try = 0; try < ntries; try++) {
	    hdr.h_port = Grp_port;
	    hdr.h_offset = Grp_this_member;
	    hdr.h_extra = My_static_id;
	    hdr.h_command = SP_GRP_JOIN;
	    Grp_id = grp_join(&hdr);
	    if (Grp_id < 0) {
		message("grp_join failed (%s)", err_why((errstat) Grp_id));
	    } else {
		message("grp_join succeeded at try %d!", try);
		break;
	    }
#ifdef USER_LEVEL
	    sleep(2);
#endif
	}
    }

    if (Grp_id < 0) {
	static	done_this_before ;

	if ( done_this_before ) {
	    gpanic(("current implementation can not handle group recreation"));
	}
	done_this_before=1 ;
	message("building the group myself");
	Grp_id = grp_create(&Grp_port, Grp_resilience, Grp_max_members,
			    (uint32) LOGBUF, (uint32) LOGDATA);
	if (Grp_id < 0) {
	    gpanic(("group create failed (%s)", err_why((errstat) Grp_id)));
	}
	if ( grp_init_create() ) {;
	    Grp_mode = MD_FUNCTIONING;
	    gs_flagnew( 1<<Superblock.s_member );
	    gs_allow_aging();
	    bs_allow_ages();
	}
    }

    err = grp_set(Grp_id, &Grp_port,
		  (interval) SYNCTIME, (interval) REPLYTIME,
		  (interval) ALIVETIME, (uint32) LARGEMESS);
    if (err != STD_OK) {
	gpanic(("grp_set failed (%s)", err_why(err)));
    }
#ifdef GRP_DEBUG
    /* Set group debug level in the kernel by means of the following hack: */
    (void) grp_set(Grp_id, &Grp_port,
		  (interval) SYNCTIME, (interval) REPLYTIME,
		  (interval) GRP_DEBUG, (uint32) LARGEMESS);
#endif

    /*
     * Phase 2
     */
    if ((err = grp_info(Grp_id, &Grp_port, &Grp_state,
			Grp_memlist, (g_indx_t) MAXGROUP)) != STD_OK) {
	gpanic(("grp_info failed (%s)", err_why(err)));
    }
    Grp_nmembers = Grp_state.st_total;
    Grp_this_member = Grp_state.st_myid;
    Grp_more_mess = 0;

    dbmessage(1, ("st_expect = %ld; st_nextseq = %ld; st_unstable = %ld",
		  Grp_state.st_expect, Grp_state.st_nextseqno,
		  Grp_state.st_unstable));
    dbmessage(1, ("Grp_nmembers = %d; Grp_this_member = %d",
		  Grp_state.st_total, Grp_state.st_myid));
    
    /* make sure there is a group thread listening */
    if (Grp_nthreads == 0) {

#ifdef USER_LEVEL
	if (!thread_newthread(grp_server,GRP_STACKSIZE,(char *) NULL, 0 )) {
	    gpanic(("cannot start the grp_server thread"));
	}
#else
	NewKernelThread(grp_server, GRP_STACKSIZE) ;
#endif
	Grp_nthreads++;
    }

    invalidate_bids();

    /* From now on do a sema_up on Sem_recovering if enough bids arive */
    Awaiting_bids = 1;

    /* make our sequence number known */
    if ((err = broadcast_status()) != STD_OK) {
	gpanic(("could not broadcast status (%s)", err_why(err)));
    }
/* Could just be your own, then it is too fast.
 *
 */
    if ( Grp_mode == MD_FUNCTIONING ) {
	return STD_OK ;
    }

    /* Waiting for others to announce themselves */
    (void) sema_trydown(&Sem_recovering, MAXBUILDTIME);

    /* Not interested in more bids */
    Awaiting_bids = 0;

    /*
     * End of Phase 2: see if we have enough members.
     * If so, try to get up to date.
     */
    while ( members_exist() && (err = get_up_to_date(mode)) != STD_OK) {
	scream("could not get up to date");
    }
    if ( Grp_mode!=MD_FUNCTIONING ) {
	/* This happens when new members arrive in an active group and
	 * all up-to-date members quit before any new ones are fully
	 * functional.
	 * Broadcast an init_ttl message to those willing to hear it.
	 * These members thereby become functional.
	 */
	scream("could not find enough members, initing ttls");
	if ( grp_init_create() ) {
	    Grp_mode = MD_FUNCTIONING;
	    gs_allow_aging();
	    bs_allow_ages();
	}
    }

    if (err == STD_OK) {
	/* Make known that we are up-to-date.
	 */
	/* The inodes have been sent. announce that ... */
	gs_flagnew( (int)(bs_members_seen | (1<<Superblock.s_member ) ));
	if ((err = broadcast_status()) != STD_OK) {
	    scream("could not broadcast status (%s)", err_why(err));
	}
    } else {
	bs_leave_group();
    }

    return err;
}

/*
 * Become member of the group or create the group.
 */

void
grp_startup(min_copies, nmembers)
int min_copies;
int nmembers;
{
    if (min_copies <= 0) {
	Required_replicas = MIN_COPIES(nmembers);
    } else {
	Required_replicas = min_copies;
    }

    /* Allow the actual group to be bigger. This allows `double' members
     * to enter the group and find out that they are not welcome. And
     * works around a bug in the group code that surfaces when
     * the sequencer does a `grp_leave'.
     */
    Grp_max_members = nmembers + 2 ;
    Grp_resilience = nmembers -1 ;
    if (Grp_max_members > MAXGROUP) {
	gpanic(("bad nmembers parameter %d (max %d)",
		Grp_max_members, MAXGROUP));
    }

    sema_init(&Sem_recovering, 0);
    sema_init(&Sem_need_recovery, 0);
    sema_init(&Sem_leaving, 0);
    sema_init(&Sem_functioning, 0);
    sema_init(&waiter_sema,0) ;

    /* wait until a group of sufficient size has been created */

    init_bids();

    /* wait until will have recovered and enough members are up-to-date: */
    while (Grp_mode != MD_FUNCTIONING) {

	if (Grp_mode == MD_RECOVERING) {
	    while (recover_group(SEND_CANWAIT) != STD_OK) {
		scream("group recovery failed; retry");
	    }
	}

	/* wait till enough other members are up-to-date as well */
	if (Grp_mode != MD_FUNCTIONING) {
	    if (sema_trydown(&Sem_functioning, MAXBUILDTIME) != 0) {
		scream("not enough members up-to-date yet; restart");
		bs_leave_group();
	    }
	}
    }

}

/* External interaction, should not change any field, information only */
/* Return ASCII member status */
char *
grp_mem_status(member)
	int		member;
{
    /* Produce ASCII member information in a buffer.
     */
    char *str;
    struct group_status *gstp ;

    if (member > MAXGROUP) {
	    return "number too high" ;
    }

    gstp = &Grp_status[member];
    if (gstp->gst_valid && gstp->gst_alive ) {
	switch (gstp->gst_mode) {
	case MD_FUNCTIONING: str="functioning" ; break ;
	case MD_RECOVERING : str="recovering" ; break ;
	default:	     str="unknown" ; break ;
	}
	return str ;
    }
    return "not present" ;
}

int
grp_member_present(member)
	int		member;
{
    struct group_status *gstp ;

    if (member > MAXGROUP) {
	    return 0 ;
    }

    gstp = &Grp_status[member];
    if (gstp->gst_valid && gstp->gst_alive &&
        gstp->gst_mode== MD_FUNCTIONING) {
		return 1 ;
    }
    return 0 ;
}

/* Pass a request on to another member */
errstat
grp_pass_request(member)
int			member;
{
    struct group_status *gstp ;
    errstat		err ;

    if (member > MAXGROUP) {
	    return STD_SYSERR ;
    }

    gstp = &Grp_status[member];
    if ( !(gstp->gst_valid && gstp->gst_alive &&
        gstp->gst_mode== MD_FUNCTIONING) ) {
		return STD_NOTNOW ;
    }
    err= grp_forward(Grp_id, &Grp_port,(g_indx_t)gstp->gst_id) ;
    dbmessage(4,("Passing request to member %d, id %ld, g_id %ld, returns %s",
	member,(long)Grp_id,(long)gstp->gst_id,err_why(err)));
    if ( err!=STD_OK ) return err ;
    /* We completely lost control from here on.. signal this */
    return BULLET_LAST_ERR ;
}

