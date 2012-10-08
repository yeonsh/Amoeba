/*	@(#)grp_bullet.c	1.1	96/02/27 14:07:15 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	Author: Ed Keizer, 1995
 */

/*
 * GRP_BULLET.C
 *
 *	This file contains the group bulletsvr specific code.
 *	It contains the stubs called from grp_comm.c, among other things.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "amoeba.h"
#include "group.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#ifdef USER_LEVEL
#include "thread.h"
#else
#include "sys/proto.h"
#include "sys/kthread.h"
#endif
#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/prv.h"
#include "module/buffers.h"
#include "module/mutex.h"
#include "module/strmisc.h"
#include "byteorder.h"
#include "preempt.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_comm.h"
#include "grp_bullet.h"
#include "grp_defs.h"
#include "event.h"
#include "alloc.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

extern  Superblk Superblock;     /* in-core copy of superblock */
extern  Superblk DiskSuper;      /* big-endian disk version */
extern	Inode *		Inode_table;
extern	peer_bits *	Inode_reptab;
extern	Inodenum	bs_local_inodes_free;
extern	b_fsize		Blksizemin1;

/* Debugging output redirection */
extern	int		bs_print_msgs ;
extern	int		bs_log_msgs ;
extern	int		bs_ls_sick ;

/* The size of the super blocks, also start of inodes */
extern disk_addr	bs_super_size ;

/* A mask indicating the servers of which we have inode information */
peer_bits		bs_members_seen ;

int      		bs_debug_level ;

/* Data structure that remembers what all servers announced about the
 * resources they have available.
 */

static struct member_resource {
	Inodenum	in_free ;	/* # of free inodes */
	disk_addr	bl_free ;	/* # of free blocks */
	disk_addr	total_blocks ;	/* # of blocks */
} bs_member_resources[S_MAXMEMBER] ;

static mutex res_lock ; /* To protect the above */

/* Flag used to tell the server that new inodes should not be allocated.
 * Used when exiting nicely and when int PASSIVE mode
 */
static int		no_creation ;

/* Global state
 */

static mutex		gstate_lock ;
static int		gstate ;	/* BS_OK, BS_DEAD, BS_WAITING */

static b_event		state_waiting ;
static int		sw_inited ;	/* Set if the above is inited */

#ifdef USER_LEVEL
/* Amoeba user level */
#include "module/syscall.h"
#define getmilli	sys_milli
#else
/* Amoeba kernel definition is done by sys/proto.h */
#endif

/* Events for incoming group packets */
b_event	bs_config_event ;

/* Send queue management */
struct send_elem {
	struct send_elem	*next ;	 /* Chain */
	int		     reserved ;      /* Memory source */
	header		  hdr ;
	uint32		  size ;
	char		    buf[BS_GRP_REQSIZE] ;
};
 
/* Queue administration */
static mutex		Queue_mutex ;	/* mutex for Queue admin */
static struct send_elem	*Queue_head ;	/* Head of busy Queue */
static struct send_elem	*Queue_tail ;	/* Tail of busy Queue */
static int		Queue_active ;	/* # in queue + busy */
static int		Queue_waiting ; /* # of normals waiting */

/* Queue semaphores, used outside Queue_mutex */
static semaphore	Queue_throttle ;/* delay sending */
static semaphore	Queue_count ;	/* # in queue */

/* Free Queue administration */
static mutex		FreeQ_mutex ;	/* mutex for Queue_free */
static struct send_elem	*Queue_free ;	/* Head of free Queue */
static int		FreeQ_waiting ;	/* Sema_down's on FreeQ_empty */
static semaphore	FreeQ_empty ;	/* waiting for buffer to get free */

/* Unprotected */
static int		FreeQ_needed ;	/* # of buffers we are short */


static void	recv_config();
static void	recv_inodes();
static void	recv_resources();
static errstat	send_config();
static errstat	change_config();
static void	take_inode _ARGS((Inodenum,Inode *,Inode *,int));
static void	send_super _ARGS((int));
static void	bs_grp_depart _ARGS((void));

/* Initialize structures needed for the group */
void
bs_grp_init() {
	Res_size		numbytes;
	struct send_elem	*qb ;
	struct send_elem	*send_next ;
	int			i ;

	event_init(&bs_config_event);


	dbmessage(63,("in bs_grp_init"));
	/* Initialize queue management */
	sema_init(&Queue_throttle,1) ;
	sema_init(&Queue_count,0) ;
	sema_init(&FreeQ_empty,0) ;
	mu_init(&FreeQ_mutex);
	mu_init(&Queue_mutex);

	numbytes = GRP_SEND_BUFS * sizeof (struct send_elem);
	/* Round up to blocksize */
	numbytes += Blksizemin1;
	numbytes &= ~Blksizemin1;
	qb = (struct send_elem *) a_alloc(A_CACHE_MEM, numbytes);
	/* Initialize the free Q */
	Queue_free=qb ;
	for ( i=1 ; i<GRP_SEND_BUFS ; i++ ) {
		qb->reserved=1 ;
		send_next= ++qb ;
		qb->next= send_next ;
		qb= send_next ;
	}
	qb->reserved=1 ;
	qb->next= (struct send_elem *)0 ;
	bs_gc_init() ;
	gs_rep_init();
	
}

char *
buf_put_seqno(p, e, valp)
char *p;
char *e;
int16 *valp;
{
    return buf_put_int16(p, e, *valp);
}

void
get_global_seqno(seq)
Seqno_t *seq;
{
    *seq = 0;
    dbmessage(92,("Getting global seqno %d",0)) ;
}

/* Get Group bullet server status.
 * Done by sending a get request to the member we want
 * the status from.
 * That member should broadcasts all relevant information.
 * All servers receive the same and should (re)send when they
 * `hear' incorrect information especaily regarding replicas.
 */

errstat
grp_get_state(bestid,mode,maxdelay)
int		bestid;
int		mode;
interval	maxdelay;
{
	header		hdr ;
	errstat		err;
	b_event		*p_event ;
	int		num_event ;

	dbmessage(3,("grp_get_state(%d)", bestid));

	num_event=get_event(&p_event) ;
	event_incr(p_event);
	/* Prepare the header */
	hdr.h_command= BS_UPDATE;
	hdr.h_offset= 0 ;
	hdr.h_size= num_event ;
	hdr.h_extra= Superblock.s_member ;
	err= bs_grp_single(bestid,&hdr, (bufptr)0, 0);
	if ( err!= STD_OK ) {
		dbmessage(3,("grp_get_state failed(%s)",err_why(err))) ;
		return err ;
	}
	if ( mode&SEND_NOWAIT ) {
	    /* give mode to gs_later_ttl, ttl_announce, gs_age_sweep,
	     * flush_ttls.
	     */
	    bwarn("Incomplete grp_get_state");
	}
	send_super(mode);
	if ( Inode_table ) {
		bs_send_resources(mode) ;
	}

	/* Now wait for the event signalling that we are up to date
	 * has come in.
	 */ 
	if ( event_trywait(p_event,maxdelay)<0 ) {
		return STD_NOTNOW ;
	}
	gs_later_ttl() ;
	/* Make sure that the differences in opinion are sent out. */
	bs_publist(mode) ;
	return err;
}

/* Specific broadcast items */
static void
send_super(mode) {
	Superblk	NewSuper ;

	/* Broadcast configuration */
	super_lock();
	message("Broadcasting configuration");
	NewSuper= Superblock ;
	super_free() ;
	send_config(&NewSuper,mode) ;
}

void
bs_update(hdr)
	header	*hdr;
{
	errstat	err ;
	header	ack_hdr ;

	dbmessage(3,("update request"));

	ack_hdr= *hdr ;

	send_super(SEND_CANWAIT) ;

	/* Do a full reply only if the inode table is there */
	if ( !Inode_table ) {
		err= STD_NOTNOW ;
	} else {
		gs_send_ttls() ;
		bs_send_resources(SEND_CANWAIT) ;
		err= STD_OK ;
	}

	/* The other one is basically up, return the RPC, but
	 * stay around to send the inodes.
	 */
	hdr->h_status= err ;
	hdr->h_size = 0;
#ifdef USE_AM6_RPC
	rpc_putrep(hdr, (bufptr)0, hdr->h_size);
#else
	putrep(hdr, (bufptr)0, hdr->h_size);
#endif
	/* Now send acknowledgement */
	ack_hdr.h_command= BS_UPDATE ;
	ack_hdr.h_offset= ack_hdr.h_extra ;
	ack_hdr.h_extra= Superblock.s_member ;
	bs_grpreq(&ack_hdr, (bufptr)0, 0, SEND_CANWAIT);
	
	/*
	 * And broadcast current inode state.
	 * Requestor will respond for inconsistencies.
	 */
	while ( bs_send_inodes((Inodenum)1,Superblock.s_numinodes-1,
				1,0,SEND_CANWAIT)!=STD_OK ) ;

	return ;
}

static void
gs_updated(hdr)
	header	*hdr;
{
    dbmessage(5,("Update finished from member %d to %d, event %d",
		hdr->h_extra,hdr->h_offset,hdr->h_size)) ;
    /* Wake up your slave .... */
    if ( hdr->h_offset==Superblock.s_member ) {
	/* It is one of ours */
	event_wake(map_event((int)hdr->h_size)) ;
    }
}

long
sp_mytime()
{
    return 0;
}

/*
 * We don't store the config vec on disk, currently.
 */

static int Mem_copymode;

int
get_copymode()
{
    return Mem_copymode;
}

void
set_copymode(mode)
int mode;
{
    Mem_copymode = mode;
}

void
flush_copymode()
{
}

/*
 * Group stubs called from grp_comm.c
 */

int grp_init_create()
{
	/* The check for inode_table is there to catch the race condition
	 * in which a new member is welcomed and immediatly after that
	 * the group has to be rebuilt for some other reason AND this
	 * member is the first one to create a new group.
	 */
	if ( Inode_table ) {
		gs_first_ttl() ;
		return 1 ;
	} else {
		return 0 ;
	}
}

/*ARGSUSED*/
void grp_got_req(hdr, buf, size)
header   *hdr;
char     *buf;
int      size;
{
    switch ( hdr->h_command ) {
    case BS_GRP_CONFIG :
	recv_config(buf,size) ;
	return ;
    case BS_INODES:
	recv_inodes(hdr, buf, size);
	return ;
    case BS_RESOURCES:
	recv_resources(hdr, buf, size);
	return ;
    case STD_AGE:
	gs_do_age(hdr) ;
	return;
    case BS_CHECKLIFE:
	gs_checklife(hdr) ;
	return;
    case BS_SETLIFES:
	gs_setlifes(hdr, buf, size) ;
	return ;
    case BS_AGE_DELAY:
	gs_delay_age(hdr) ;
	return ;
    case BS_UPDATE:
	gs_updated(hdr) ;
	return ;
    }
    message("grp_got_req: ignored (command = %d)", hdr->h_command);
}

void grp_handled_req()
{
#if 0
    message("handled group request #%ld", grpseq);
#endif
}

/* Get a send element. If not from the reserved space then from malloc.
 */
static struct send_elem *
bs_grpbuf(mode)
int	mode;
{
    struct send_elem	*qb ;

    for (;;) {
	mu_lock(&FreeQ_mutex) ;
	if ( Queue_free ) {
	    qb= Queue_free ;
	    Queue_free= qb->next ;
	    mu_unlock(&FreeQ_mutex) ;
	    return qb ;
	}
	if ( mode&SEND_NOWAIT ) {
	    /* Hmm, no more free buffer. Try to get one */
	    qb= (struct send_elem *)malloc(sizeof(struct send_elem));
	    if ( qb ) {
		FreeQ_needed++ ;
		qb->reserved=1 ; /* hang on to the buffer */
	    }
	    mu_unlock(&FreeQ_mutex) ;
	    bwarn("Out of send queue, needing %d",FreeQ_needed);
	    return qb ;
	} else {
	    /* Can wait for the buffer */
	    FreeQ_waiting++ ;
	    mu_unlock(&FreeQ_mutex);
	    bwarn("Out of send queue buffers, waiting ......");
	    sema_down(&FreeQ_empty);
	    /* And round we go .... */
	}
    }
}

/* release a buffer to the free queue */

static void
bs_grp_rel(qb)
struct send_elem	*qb ;
{
    if ( qb->reserved ) {
	/* Put buffer back into free Q */
	mu_lock(&FreeQ_mutex) ;
	qb->next= Queue_free ;
	Queue_free= qb ;
	if ( FreeQ_waiting ) {
	    FreeQ_waiting-- ;
	    sema_up(&FreeQ_empty) ;
	}
	mu_unlock(&FreeQ_mutex) ;
    } else {
	/* release memory to malloc */
	FreeQ_needed-- ;
	free((_VOIDSTAR)qb) ;
    }
}

/* Send a group message. If it can not wait, queue it. */
void
bs_grpreq(hdr, buf, size, mode)
header *hdr;
char   *buf;
int     size;
int	mode;
{
   struct send_elem	*qb ;

    if ( mode&SEND_NOWAIT ) {
	/* Can not run the chance of waiting. Insert copy into
	 * queue and let a separate thread handle the sending.
	 */
	qb= bs_grpbuf(mode) ;
	if ( !qb ) {
	    /* Yuk, can't get any more memory. Let's hope we won't block. */
	    bpanic("Possible grp_send deadlock") ;
	    bs_grp_trysend(hdr, buf, (int)size, mode) ;
	} else {
	    /* Copy data into buffer */
	    qb->hdr= *hdr ;
	    qb->size= size ;
	    memcpy((_VOIDSTAR)qb->buf,(_VOIDSTAR)buf,(size_t)size) ;
	    /* insert element into queue */
	    mu_lock(&Queue_mutex);
	    Queue_active++ ;
	    if ( mode&SEND_PRIORITY ) {
		qb->next= Queue_head ;
		Queue_head= qb ;
		if ( !Queue_tail ) Queue_tail= qb ;
	    } else {
		qb->next= (struct send_elem *)0 ;
		if ( Queue_head ) {
		    Queue_tail->next= qb ;
		} else {
		    Queue_head=qb ;
		}
		Queue_tail=qb ;
	    }
	    mu_unlock(&Queue_mutex) ;
	    /* Alert sender */
	    sema_up(&Queue_count) ;
	}
    } else {
	/* Send it yourself, but give priotity to message from the queue
	 */
	mu_lock(&Queue_mutex) ;
	if ( Queue_active ) {
	    Queue_waiting++ ;
	    mu_unlock(&Queue_mutex) ;
	    sema_down(&Queue_throttle);
	} else {
	    mu_unlock(&Queue_mutex) ;
	}
	bs_grp_trysend(hdr, buf, (int)size, mode) ;
    }
}

/* Empty the send queue, this is a separate thread */
#if defined(USER_LEVEL)
/*ARGSUSED*/
void
bullet_send_queue(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
bullet_send_queue()
#endif
{
	struct send_elem	*qb ;

	for(;;) {
		sema_down(&Queue_count) ;
		dbmessage(9,("Queue send starting"));
		mu_lock(&Queue_mutex) ;
		qb= Queue_head ;
		if ( !qb ) bpanic("Sender q managment") ;
		Queue_head= qb->next ;
		if ( !Queue_head ) Queue_tail= (struct send_elem *)0 ;
		mu_unlock(&Queue_mutex) ;
		bs_grp_trysend(&qb->hdr, qb->buf, (int)qb->size, SEND_CANWAIT) ;
		dbmessage(9,("Queue send succeeded"));
		bs_grp_rel(qb) ;
		/* See whether we can release to sender to others */
		mu_lock(&Queue_mutex) ;
		if ( --Queue_active == 0 ) {
			if ( Queue_waiting ) {
				sema_mup(&Queue_throttle,Queue_waiting) ;
				Queue_waiting=0 ;
			}
		}
		mu_unlock(&Queue_mutex) ;
	}
}

/* Start all group activity. We want to be a full member */
int
bs_build_group()
{
    capability memcap;
    int me;
    int status;
    errstat	err;

    me = Superblock.s_member;
    /* sanity checks.. */
    if (me == S_UNKNOWN_ID || me < 0 || me >= Superblock.s_maxmember) {
	scream("bs_build_group: illegal member no %d\n", me);
	return 0;
    }
    assert( MAX_VERSION+VERSION_WINDOW>MAX_VERSION) ;

    /* No super_lock, we are alone */
    status = Superblock.s_memstatus[me].ms_status;
    if (status != MS_NEW && status != MS_ACTIVE && status != MS_PASSIVE ) {
	scream("bs_build_group: strange status %d\n", status);
	return 0;
    }

    /* the member cap in the superblock is a get-capability */
    memcap = Superblock.s_membercap;
    priv2pub(&memcap.cap_port, &memcap.cap_port);
    grp_init_port(me, &Superblock.s_groupcap.cap_port, &memcap);

    grp_startup(1, (int) Superblock.s_maxmember);

    /* If we are new and here, we can change our own status to active ... */
    super_lock();
    status = Superblock.s_memstatus[me].ms_status;
    super_free();
    if ( status==MS_NEW ) {
	err= bs_change_status((int)Superblock.s_member,MS_ACTIVE
			,(capability *)0,0);
	if ( err!=STD_OK ) return 0 ;
    }
    return 1;
}

void
encode_superblock(myendian, bigendian)
Superblk *myendian;
Superblk *bigendian;
{
    extern uint16 checksum();
    int i;

    *bigendian = *myendian;

    enc_l_be(&bigendian->s_magic);
    enc_s_be(&bigendian->s_version);
    enc_s_be(&bigendian->s_maxmember);
    enc_s_be(&bigendian->s_member);
    enc_s_be(&bigendian->s_def_repl);
    enc_s_be(&bigendian->s_blksize);
    enc_s_be(&bigendian->s_flags);
    enc_s_be(&bigendian->s_seqno);
    ENC_INODENUM_BE(&bigendian->s_numinodes);
    ENC_DISK_ADDR_BE(&bigendian->s_numblocks);
    ENC_DISK_ADDR_BE(&bigendian->s_inodetop);
    for (i = 0; i < myendian->s_maxmember; i++) {
	enc_s_be(&bigendian->s_memstatus[i].ms_status);
    }

    /* update checksum */
    bigendian->s_checksum = 0;
    bigendian->s_checksum = checksum((uint16 *) bigendian);
}

/* Set a new status in a copy of the Superblock */
/* Change: 1 if the Super Block has to be changed,
	   0 if the superblock does not have to be changed
 */

static int
set_status(NewSuper,newstatus,member)
    Superblk	*NewSuper ;
    int		newstatus ;
    int		member ;
{
    int		oldstatus ;

    if ( newstatus>MS_PASSIVE || newstatus<0 ) return -1 ;

    /* Take a copy of the Super Block and operate on that. */
    super_lock();
    *NewSuper=Superblock ;
    super_free();

    oldstatus=NewSuper->s_memstatus[member].ms_status ;
    if ( oldstatus != newstatus ) {
	if ( newstatus<oldstatus && newstatus!=MS_NONE ) return -1 ;
	NewSuper->s_memstatus[member].ms_status= newstatus ;
	NewSuper->s_seqno++ ;
	return 1 ;
    }
    return 0 ;
}


/* Select a new number on the basis of the current Superblock */

/* Change: 1 if the Super Block has to be changed,
	   0 if the superblock does not have to be changed
 */

static int
select_new(NewSuper,Change,membercap)
    Superblk	*NewSuper ;
    int		*Change ;
    capability	*membercap;
{
    int     	i;

    /* Take a copy of the Super Block and operate on that. */
    super_lock();
    *NewSuper=Superblock ;
    super_free();

    *Change=0 ;
    /* first see if the new member is already present: */
    for (i = 0; i < NewSuper->s_maxmember; i++) {
	if ((NewSuper->s_memstatus[i].ms_status != MS_NONE) &&
	    cap_cmp(&NewSuper->s_memstatus[i].ms_cap, membercap))
	{
	    if ( NewSuper->s_memstatus[i].ms_status!=MS_NEW ) {
		return STD_EXISTS ;
	    }
	    message("new member was already allocated: %d", i);
	    if ( NewSuper->s_memstatus[i].ms_status != MS_NEW ) {
		NewSuper->s_memstatus[i].ms_status= MS_NEW ;
		NewSuper->s_seqno++ ;
		*Change=1 ;
	    }
	    return i ;
	}
    }

    /* It was really new, so find a place to fit the new server cap */

    for (i = 0; i < NewSuper->s_maxmember; i++) {
	if (NewSuper->s_memstatus[i].ms_status == MS_NONE) {
	    message("new member gets index %d", i);
	    NewSuper->s_memstatus[i].ms_cap = *membercap;
	    NewSuper->s_memstatus[i].ms_status = MS_NEW;
	    NewSuper->s_seqno++ ;
	    *Change=1 ;
	    return i ;
	}
    }

    /* Found nothing */
    return STD_NOSPACE ;
}

/* Create a status message from a superblock */
/* Layout of status message
 * Check means that receiving members should check the information.
 * Changeable means that servers should accept new values.
 *	char		protocol ;	Check
 *	uint16		seqno ;		Changeable, Check
 *	uint16		maxmember ;	Check
 *	uint16		defrep ;	Changeable
 *	Inodenum	numinodes ;	Check
 *	port		fileport ;	Check
 *	capability	groupcap ;	Check
 *	capability	supercap ;	Check
 *	capability	logcap ;	Changeable
 *	struct {
 *		capability  cap;	Changeable
 *		uint16	    status;	Changeable
 *	} memstatus[S_MAXMEMBER] ;
 */

static char *
buf_put_config(msg_begin,msg_end,NewSuper)
    char	*msg_begin, *msg_end ;
    Superblk	*NewSuper ;
{
    register int	i ;
    register char	*p ;

    p= msg_begin ;

    *p++ = BS_PROTOCOL ;
    p = buf_put_int16(p, msg_end, (int16)NewSuper->s_seqno) ;
    p = buf_put_int16(p, msg_end, (int16)NewSuper->s_maxmember) ;
    p = buf_put_int16(p, msg_end, (int16)NewSuper->s_def_repl) ;
    p = buf_put_int16(p, msg_end, (int16)NewSuper->s_flags) ;
    p = buf_put_int32(p, msg_end, (int32)NewSuper->s_numinodes) ;
    p = buf_put_port (p, msg_end, &NewSuper->s_fileport) ;
    p = buf_put_cap  (p, msg_end, &NewSuper->s_groupcap) ;
    p = buf_put_cap  (p, msg_end, &NewSuper->s_supercap) ;
    p = buf_put_cap  (p, msg_end, &NewSuper->s_logcap) ;
    for ( i=0 ; i<NewSuper->s_maxmember ; i++ ) {
	p = buf_put_cap  (p, msg_end, &NewSuper->s_memstatus[i].ms_cap) ;
	p = buf_put_int16(p, msg_end, (int16)NewSuper->s_memstatus[i].ms_status) ;
    }
    return p ;
}

static char *
buf_get_config(msg_begin,msg_end,NewSuper)
    char	*msg_begin, *msg_end ;
    Superblk	*NewSuper ;
{
    register int	i ;
    register char	*p ;
    int32		tmp_numinodes ;

    p= msg_begin ;

    /* If the protocol numbers do not match, forget it all ... */
    if ( *p++ != BS_PROTOCOL ) {
	message("mismatch in protocol numbers: expected %d, got %d",
		BS_PROTOCOL, *(p-1)) ;
	return 0 ;
    }
    p = buf_get_int16(p, msg_end, (int16 *)&NewSuper->s_seqno) ;
    p = buf_get_int16(p, msg_end, (int16 *)&NewSuper->s_maxmember) ;
    p = buf_get_int16(p, msg_end, (int16 *)&NewSuper->s_def_repl) ;
    p = buf_get_int16(p, msg_end, (int16 *)&NewSuper->s_flags) ;
    p = buf_get_int32(p, msg_end, &tmp_numinodes) ;
    NewSuper->s_numinodes= tmp_numinodes ;
    p = buf_get_port (p, msg_end, &NewSuper->s_fileport) ;
    p = buf_get_cap  (p, msg_end, &NewSuper->s_groupcap) ;
    p = buf_get_cap  (p, msg_end, &NewSuper->s_supercap) ;
    p = buf_get_cap  (p, msg_end, &NewSuper->s_logcap) ;
    for ( i=0 ; i<NewSuper->s_maxmember ; i++ ) {
	p = buf_get_cap  (p, msg_end, &NewSuper->s_memstatus[i].ms_cap) ;
	p = buf_get_int16(p, msg_end, (int16 *)&NewSuper->s_memstatus[i].ms_status) ;
    }
    return p ;
}

static errstat
send_config(NewSuper,mode)
    Superblk	*NewSuper ;
    int		mode ;
{
    char    	config_msg[450] ;
    char	*conf_end ;
    int		conf_size ;
    header	hdr ;

    /* Create the message */
    conf_end= buf_put_config(config_msg, config_msg+sizeof config_msg,
					NewSuper) ;
    if ( conf_end==NULL ) return STD_SYSERR ;
    conf_size= conf_end-config_msg ;
    hdr.h_command = BS_GRP_CONFIG ;

    message("Sending status, seqno= %d",NewSuper->s_seqno) ;
    /* Send the message */
    bs_grpreq(&hdr, (bufptr)config_msg, conf_size, mode);
    return STD_OK ;
}

/* Send the new configuration and wait for the results to come in */

static errstat
change_config(NewSuper,restricted)
    Superblk	*NewSuper ;
    int		restricted ;
{
    long	start_time ;
    interval	remaining ;
    errstat	err ;

    
    if ( restricted ) {
	if ( bs_grp_missing()!=0 ) return STD_NOTNOW ;
    }

    /* Announce the wait */
    event_incr(&bs_config_event) ;

    err = send_config(NewSuper,SEND_CANWAIT);
    ENABLE_PREEMPTION;
    if (err != STD_OK) {
	event_cancel(&bs_config_event) ;
	return err;
    }

    /* Wait for message to be received */
    remaining=10000 ; /* 10 seconds */
    start_time=getmilli() ;
    do {
	if ( Superblock.s_seqno==NewSuper->s_seqno ) {
	    /* We have the expected sequence number */
	    return STD_OK ;
	}
	if ( event_trywait(&bs_config_event,remaining)<0 ) {
	    return STD_SYSERR ;
	}
	remaining=getmilli()-start_time ;
    } while ( remaining>0 ) ;
    return STD_SYSERR ;
}

/* Called by sdt_setparams */
errstat
gs_chsuper(newrep,newrepval,newflag,newflagval,newlog,newlogval)
	int	newrep ;
	int	newrepval;
	int	newflag;
	int	newflagval;
	int	newlog;
	capability	*newlogval;
{
    Superblk	NewSuper ;
    int		i ;
    errstat	err ;
    int		Changed ;
    int		changing_flags ;


    /* Make sure all members switch over to a new superblock
     * with new status.
     */
    if ( bs_grp_missing() ) return STD_NOTNOW ;
    message("Changing superblock params");
    for (i=0;;i++) {
	super_lock() ;
	NewSuper= Superblock ;
	super_free() ;

	Changed=0 ;
	if ( newrep && newrepval!=NewSuper.s_def_repl ) {
	    NewSuper.s_def_repl= newrepval ;
	    Changed=1 ;
	}
	changing_flags= (NewSuper.s_flags^newflagval)&newflag ;
	if ( changing_flags ) {
	    NewSuper.s_flags ^= changing_flags ;
	    Changed=1 ;
	}
	if ( newlog &&
	     memcmp((_VOIDSTAR) &NewSuper.s_logcap, (_VOIDSTAR) newlogval,
			sizeof(capability)) != 0
	   ) {
	    NewSuper.s_logcap= *newlogval ;
	    Changed=1 ;
	}
	if ( !Changed ) return STD_OK ;
	/* Safety, retry counter */
	if ( i>10 ) return err ;
	NewSuper.s_seqno++ ;
	err= change_config(&NewSuper,1);
	if ( err== STD_NOTNOW ) return err ;
	/* We have a new Super Block, if it has the new status, the next
	 * execution of the loop should end the loop.
	 */
    }
}

/* Change the status for a single member */
/* Returns the new member number when S_UNKNOWN_ID & MS_NEW are
 * the parameters. membercap is only used in this case.
 */
errstat
bs_change_status(member,newstatus,membercap,restricted)
    int		member;
    int		newstatus;
    capability	*membercap;

{
    Superblk	NewSuper ;
    int		i ;
    int		newmember ;
    errstat	err ;
    int		SuperChanged ;


    /* Make sure all members switch over to a new superblock
     * with new status.
     */
    message("Setting new status");
    for (i=0;;i++) {
	if ( member==S_UNKNOWN_ID ) {
	    if ( newstatus!=MS_NEW ) return STD_ARGBAD ;
	    SuperChanged=0 ;
	    newmember= select_new(&NewSuper,&SuperChanged,membercap);
	    if ( newmember<0 || !SuperChanged ) {
		return newmember ;
	    }
	} else switch ( set_status(&NewSuper,newstatus,member) ) {
	case 0 :
	    /* No need to go to group */
	    return STD_OK ;
	case -1 :
	    return STD_ARGBAD ;
	/* Others fall through */
	}
	/* Safety, retry counter */
	if ( i>10 ) return err ;
	err= change_config(&NewSuper,restricted);
	if ( err== STD_NOTNOW ) return err ;
	/* We have a new Super Block, if it has the new status, the next
	 * execution of the loop ends the loop.
	 */
    }


    /* Superblock does not need to be written to disk, this
     * has been done by recv_config.
     */
}

/* Initialize the inode table according to the sizes mentioned
 * in the superblock, with help from the other members.
 *
 * As soon as this has happened, we can start thinking about becoming
 * a fully functional member. For that we need a bunch of free inodes 
 * to start with.  Getting some information about the files existing at
 * other members would also be useful, in order to avoid locates.
 * Etc. etc.
 */

static errstat
init_server()
{
    /* Check whether we have enough inode space */
    if (Superblock.s_inodetop > Superblock.s_numblocks)
    {
	message("Disk space for inodes > size of the disk");
	return STD_ARGBAD;
    }

    /* Set all inodes to unknown for now. Get info later. */
    message("Initializing inodes") ;
    set_inodes() ;

    return STD_OK ;
}

errstat
gs_help_member(membercap)
capability *membercap;
{
    /* Send the superblock info over to the joining member.
     * After it has accepted it (and written to disk?), we must tell the
     * other members about it.  The inode table of the new
     * member then has to be brought up to date, after which
     * it may become fully functional.
     *
     */
    header  	hdr;
    bufsize 	siz;
    errstat 	err;
    char	config_msg[450] ;
    char	*conf_end ;
    bufsize	conf_size ;
    int		newmember ;
    uint16	newstatus ;

    /* Allocate new member number and have all servers agree on it */

    newmember= bs_change_status(S_UNKNOWN_ID,MS_NEW,membercap,1) ;
    if ( newmember<0 ) return newmember ; /* Error */

    /* Welcome the new member */
    super_lock();
    conf_end= buf_put_config(config_msg, config_msg+sizeof config_msg,
					&Superblock) ;
    super_free();
    if ( conf_end==NULL ) return STD_SYSERR ;
    conf_size= conf_end-config_msg ;
    hdr.h_command = BS_WELCOME;
    hdr.h_port = membercap->cap_port;
    hdr.h_priv = membercap->cap_priv;
    hdr.h_size = newmember;
    siz = trans(&hdr, (bufptr) config_msg, conf_size,
		&hdr, (bufptr) NULL, (bufsize) 0);
    if (ERR_STATUS(siz)) {
	err = ERR_CONVERT(siz);
    } else {
	err = ERR_CONVERT(hdr.h_status);
    }

    if (err != STD_OK) {
	scream("help_member failed (%s)", err_why(err));
	bs_change_status(newmember,MS_NONE,(capability *)0,0) ;
	return err ;
    }
    /* OK; so the other side got it. We should have gotten a new superblock
     * with an entry for the new server.
     */
    super_lock();
    newstatus=Superblock.s_memstatus[newmember].ms_status ;
    super_free();
    if ( !( newstatus == MS_ACTIVE || newstatus==MS_NEW) ) {
	message("Expected new member %d to be activated",newmember) ;
	return STD_SYSERR ;
    }
    return STD_OK ;
}

errstat
gs_be_welcome(my_index, buf, end)
int my_index;
bufptr buf, end;
{
    char	*p ;
    Superblk	TmpSuper ;
    Superblk	CopySuper ;
    errstat	err ;
    long	numbytes ;

    message("I seem to be welcome (index = %d)", my_index);

    /* Start a new member, first create a temporary superblock with
     * enough information to contact the others.
     * Then try to become fully active.
     */

    super_lock() ;

    TmpSuper= Superblock ;
    CopySuper=Superblock ;
    p = buf_get_config(buf, end, &TmpSuper) ;

    /* err is used to create a single failure point */
    err= STD_OK ;

    if ( p==NULL ) {
	err= STD_ARGBAD ;
    }

    /* Now check for the validity of the request */
    if ( Superblock.s_member!=S_UNKNOWN_ID && Superblock.s_member!=my_index) {
	    err= STD_DENIED ;
    }

    if ( err!=STD_OK ) {
	super_free();
	return err ;
    }

    /* Insert my number into the superblock */
    TmpSuper.s_member = my_index;

    /* Calculate the inode top */
    numbytes = TmpSuper.s_numinodes * sizeof (Inode);
    /* Round up to blocksize */
    numbytes += Blksizemin1;
    numbytes &= ~Blksizemin1;
    TmpSuper.s_inodetop= numbytes >> Superblock.s_blksize -1 + bs_super_size ;

    Superblock = TmpSuper;

    /* Give others a chance */
    super_free() ;

    /* Try to initialize the server */
    err=init_server() ;
    if ( err!=STD_OK ) {
	super_lock();
	Superblock=CopySuper ;
	super_free();
	return err ;
    }

    /* Do write superblock back to disk */
    /* From this moment on we can boot and become a full member */
    encode_superblock(&Superblock, &DiskSuper);
    write_superblk(&DiskSuper);

    return STD_OK ;

}

/* Check the super block */
static int
super_check(NewSuper,OldSuper)
	Superblk	*NewSuper;
	Superblk	*OldSuper;
{
    /* Check for differences */
    if ( NewSuper->s_maxmember!=OldSuper->s_maxmember ) {
	bwarn("Number of members increased from %d to %d",
			OldSuper->s_maxmember,NewSuper->s_maxmember) ;
	if ( NewSuper->s_maxmember>S_MAXMEMBER ) {
	    bwarn("Ignored the new max member count") ;
	    return 0 ;
	}
    }
    if ( NewSuper->s_numinodes!=OldSuper->s_numinodes ) {
	bwarn("Config message specified different number of inodes");
	return 0 ;
    }
    if ( !PORTCMP(&NewSuper->s_fileport,&OldSuper->s_fileport) ) {
	bwarn("Config message specified different file port") ;
	return 0 ;
    }
    if ( !bs_cap_eq(&NewSuper->s_supercap,&OldSuper->s_supercap) ) {
	bwarn("Config message specified different supercap") ;
	return 0 ;
    }
    if ( !bs_cap_eq(&NewSuper->s_logcap,&OldSuper->s_logcap) ) {
	message("Config message specified different logcap") ;
    }
    return 1 ;
}


/* A new configuration came in, first check its consistency. If its sequence
 * number is lower than mine or the same, forget the new configuration.
 * If the sequence number is lower, broadcast my configuration.
 * It the sequence number is higher then mine, copy its contents into
 * my superblock and take action....
 */

static void
recv_config(buf,size)
    char	*buf ;
    int		size ;
{
    Superblk	NewSuper ;
    uint16	oldstatus ;
    uint16	newstatus ;
    int		me ;
    char	*ptr ;
    int		member ;
    

    /* Create the template for buf_get .. to work with */
    super_lock() ;
    NewSuper=Superblock ;

    ptr= buf+size ;
    ptr=buf_get_config(buf,ptr,&NewSuper) ;
    if ( ptr==NULL || super_check(&NewSuper,&Superblock)==0 ) {
	/* Failure to decode, ignore ... */
	super_free() ;
	return ;
    }
    /* If new one is less then 3000 ahead */
    /* The way the comparison is taken accounts for wraparound */

    message("Receiving seqno %u, having %u",
		(unsigned)NewSuper.s_seqno, (unsigned)Superblock.s_seqno) ;
    if ( NewSuper.s_seqno-Superblock.s_seqno>=3000 ) {
	/* Super block sequence number differs too much */
	if ( Superblock.s_seqno-NewSuper.s_seqno>=3000 ) {
	    bwarn("Ignoring seqno %u, having %u",
		(unsigned)NewSuper.s_seqno, (unsigned)Superblock.s_seqno);
	} else {
	    dbmessage(5,("Receiving seqno %u, having %u",
		(unsigned)NewSuper.s_seqno, (unsigned)Superblock.s_seqno)) ;
	}
	super_free() ;
	return ;
    }

    if ( NewSuper.s_seqno<=Superblock.s_seqno ) {
	dbmessage(17,("Ignored old or current Config message"));
	super_free() ;
	return ;
    }
    me= Superblock.s_member ;
    oldstatus= Superblock.s_memstatus[me].ms_status ;
    newstatus= NewSuper.s_memstatus[me].ms_status ;

    /* Check whether we have someone go away */
    for ( member=0 ; member<S_MAXMEMBER ; member++ ) {
	if ( NewSuper.s_memstatus[member].ms_status==MS_NONE &&
	     Superblock.s_memstatus[member].ms_status!=MS_NONE ) {
	    /* A server disappeared, do a replication check */
	    gs_lostrep();
	}
    }

    /* We believe the superblock by the time we are here. */
    Superblock=NewSuper ;
    /* And write the thing to disk */
    encode_superblock(&Superblock, &DiskSuper);
    write_superblk(&DiskSuper);

    super_free() ; /* Free the lock */

    /* Awake waiting threads */
    event_wake(&bs_config_event);

    /* Check our own status
     */

    if ( newstatus==MS_NONE ) {
	message("status \"%s\" received for this server",
		bs_grp_ms(newstatus)) ;
	bs_be_zombie() ;
	/* (void)bs_retreat(1); hangs now */
#ifdef USER_LEVEL
	exit(0);
#else
	bs_set_state(BS_DEAD) ;
#endif
    }
    if ( newstatus==oldstatus ) return ;
    switch ( oldstatus ) {
    case MS_NEW:
    case MS_NONE:
 	break ;
    case MS_ACTIVE:
	switch ( newstatus ) {
	case MS_PASSIVE:
	    bs_create_mode(0) ;
	    break;
	case MS_NEW:
	    bpanic("New received for active server");
	}
	break ;
    case MS_PASSIVE:
	switch ( newstatus ) {
	case MS_ACTIVE:
	    bs_create_mode(1) ;
	    break;
	case MS_NEW:
	    bpanic("New received for active server");
	}
	break ;
    }

}

/* Setting and clearing of no_create ...
 */
void
bs_create_mode(newflag)
int	newflag ;
{
	no_creation= newflag ;
}

/* State checking. This is global server state. Set_state sets the state
 * check_state acts accoring to the state.
 */

void
bs_set_state(new_state) int new_state ; {
	int	copy_gstate ;

	mu_lock(&gstate_lock) ;
	if ( new_state!=BS_OK && !sw_inited ) {
		event_init(&state_waiting) ;
		sw_inited=1 ;
	}
	copy_gstate= gstate ;
	gstate=new_state ;
	mu_unlock(&gstate_lock) ;
	if ( copy_gstate==BS_WAITING && new_state==BS_OK ) {
		event_wake(&state_waiting) ;
	}
}

/*
 * This is check by each thread at `empty' times. That is for threads
 * doing getreq's, just before and after the getreq and for threads doing
 * sleep's just after the sleep.
 */
void
bs_check_state() {
	int	copy_gstate ;

	mu_lock(&gstate_lock) ;
	copy_gstate= gstate ;
	mu_unlock(&gstate_lock) ;
	if ( copy_gstate!=BS_OK ) {
		event_incr(&state_waiting) ;
		event_wait(&state_waiting) ;
	}
}

/* Find out information in inode. May pass on request */
/* TODO: in the first two minutes the server is up, it should wait for
 * its brethren when it receives a request for information that could only
 * be known to them. This could be implemented by retrying the code below
 * until the two minutes have passed.
 */
errstat bs_inq_inode(inode)
Inodenum		inode;
{
    register Inode *	ip;	/* pointer to the inode */
    errstat	err ;

    ip = Inode_table + inode;
    if ( ip->i_owner != S_UNKNOWN_ID ) {
	err = grp_pass_request((int)ip->i_owner);
	if ( err==BULLET_LAST_ERR ) return BULLET_LAST_ERR ;
    } 
    return STD_NOTNOW ;
}

/* Mark inodes a publishable. Actual sending is left to sweeper. */
#define N_PUBLISH	50	/* Max # of publishable ranges */
/* A sorted list of inode ranges */
static struct {
    Inodenum	first;
    Inodenum	last;
} i_publish[N_PUBLISH] ;
static int	i_pub_nr ;
static mutex	i_pub_mutex ;	

/* Publish the actual inodes. To prevent a deadlock, take the last
 * entries from the table in a loop. Until the table is empty.
 */
void
bs_publist(mode)
    int		mode;
{
    Inodenum     first ;
    Inodenum     number ;

    mu_lock(&i_pub_mutex) ;
    while ( i_pub_nr ) {
	i_pub_nr-- ;
	first= i_publish[i_pub_nr].first ;
	number = i_publish[i_pub_nr].last - first + 1 ;
	mu_unlock(&i_pub_mutex) ;
	while ( bs_send_inodes(first,number,0,0,mode)!=STD_OK ) ;
	if ( mode&SEND_NOWAIT ) {
	    /* Called from the grp_receive thread. Do the minumum to
	     * create some rooom in the table.
	     */
	    return;
	}
	mu_lock(&i_pub_mutex) ;
    }
    mu_unlock(&i_pub_mutex) ;

}

void
bs_publish_inodes(first,number,mode)
    Inodenum	first;
    Inodenum	number;
    int		mode;
{
    register int i,j ;
    Inodenum	 last ;
    int		 meshed ;

    last= first+number-1 ;
    mu_lock(&i_pub_mutex) ;
    dbmessage(20,("publishing %ld to %ld",first,last)) ;
    while ( i_pub_nr >= N_PUBLISH ) {
	mu_unlock(&i_pub_mutex) ;
	bs_publist(mode) ;
	mu_lock(&i_pub_mutex) ;
    }
    meshed=0 ; /* Did insert entry into table, check rest of entries? */
    for ( i=0 ; i<i_pub_nr ; i++ ) {
	if ( last+1<i_publish[i].first ) {
	    /* Insert an entry, need to shift rest 1 up */
	    dbmessage(21,("Inserting publish entry"))
	    for ( j=i_pub_nr ; j>i ; j-- ) {
		i_publish[j]=i_publish[j-1] ;
	    }
	    i_pub_nr++ ;
	    i_publish[i].first= first ;
	    i_publish[i].last= last ;
	    mu_unlock(&i_pub_mutex) ;
	    return ;
	}
	if ( i_publish[i].last + 1 >= first && first>=i_publish[i].first ) {
	    /* Overlap, start of new interval falls into current or
	       extends it */
	    if ( last<= i_publish[i].last ) {
		/* list was already present, just return */
		dbmessage(21,("Repeated publish request"));
		mu_unlock(&i_pub_mutex) ;
		return ;
	    }
	    /* Entry extends tail, re-adjust length */
	    dbmessage(21,("Extending publish tail"));
	    i_publish[i].last = last ;
	    meshed=1 ;
	} 
	if ( last>=i_publish[i].first-1 && first<=i_publish[i].first ) {
	    /* Overlap, tail of new interval falls into current or
	       extends it */
	    if ( first>= i_publish[i].first && last<= i_publish[i].last ) {
		/* list was already present, just return */
		dbmessage(21,("Repeated publish request"));
		mu_unlock(&i_pub_mutex) ;
		return ;
	    }
	    if ( first<i_publish[i].first ) {
		/* Extend head of range */
		dbmessage(21,("Extending publish head"));
		i_publish[i].first=first;
		meshed=1 ;
	    }
	    if ( last>i_publish[i].last ) {
		/* Extend head of range */
		dbmessage(21,("Extending publish tail"));
		i_publish[i].last=last;
		meshed=1 ;
	    }
	    if ( !meshed ) {
		/* list was already present, just return */
		dbmessage(21,("Repeated publish request"));
		mu_unlock(&i_pub_mutex) ;
		return ;
	    }
	}
	if ( meshed ) {
	    /* We took over the current entry, check whether
	       folllowing ones collapse.
	     */
	    last= i_publish[i].last ;
	    for ( j=i+1 ; j<i_pub_nr ; j++ ) {
		if ( i_publish[j].first-1 <= last ) {
		    /* Overlap */
		    if ( i_publish[j].last>last ) {
			/* extending tail */
			i_publish[i].last=last ;
			j++ ; break ;
		    }
		} else break ;
	    }
	    /* j now point to first entry to be copied to i+1 */
	    if ( j==i+1 ) {
		dbmessage(21,("Finished publish, no copying"));
		mu_unlock(&i_pub_mutex) ;
		return ;
	    }
	    while ( j<i_pub_nr ) {
		i++ ;
		i_publish[i]=i_publish[j] ;
		j++ ;
	    }
	    i_pub_nr=i+1 ;
	    dbmessage(21,("Finish publish, %d entries remain",i_pub_nr));
	    mu_unlock(&i_pub_mutex) ;
	    return ;
	}	
    }
    /* No overlap, add at end */
    dbmessage(21,("Adding publish entry"))
    i_publish[i_pub_nr].first= first ;
    i_publish[i_pub_nr].last= first+number -1 ;
    i_pub_nr++ ;
    mu_unlock(&i_pub_mutex) ;
}

/* Send a range of inodes */
/* Message format:
	h_offset		first inode #
	h_size			# of inodes
	h_extra			sending member #


	A sequence of:
	    owner		1 byte
	    flag		1 byte (I_INUSE,I_PRESENT,I_DESTROY,I_INTENT)
	    version		4 bytes
	    if ( I_INUSE )
		random		portsize bytes
		size		4 bytes
	    if ( I_LOCKED )
		replicas	2 bytes (bit mask)
 */

errstat
bs_send_inodes(first,number,send_repl,locked,mode)
    Inodenum	first;
    Inodenum	number;
    int		send_repl;	/* send out replica info too */
    int		locked;		/* non-zero if inodes already locked */
    int		mode;		/* mode for bs_grpreq */
{
    struct send_elem	*qb ;
    char		*buf ;
    char		*end ;
    register char	*p ;
    Inodenum		n_to_do ;
    Inodenum		current ;
    register Inode *	ip;
    register char	*lastp;
    int32		l_stub ;

/* Unhappiness:
   a:	thread 1 - publish -> overflow -> send -> lock inode
	thread 2 - lock inode -> send
	-> deadlock
 */
    qb= bs_grpbuf(mode);

    if ( !qb ) {
	bwarn("Could not allocate buffer in bs_send_inodes\n") ;
	return STD_NOSPACE ;
    }

    /* Prepare the header */
    qb->hdr.h_command= BS_INODES;
    qb->hdr.h_extra= Superblock.s_member;
    qb->hdr.h_offset= first ;

    buf= qb->buf ;

    /* Start filling the buffer */
    end= buf+BS_GRP_REQSIZE-1 ;
    p= buf ;
    current=first ;
    ip= Inode_table+ first ;
    for ( n_to_do= number ; n_to_do>0 ; ) {
	if ( n_to_do%500==499 ) threadswitch();
	lastp=p ;
	if ( !locked ) lock_inode(current,L_READ) ;
	if ( p<end-2 ) {
	    *p++ = ip->i_owner ;
	    *p = ip->i_flags ;
	    if ( send_repl && (ip->i_flags&I_INUSE) ) {
		*p |= I_REPTO ;
	    }
	    p++ ;
	} else p=0 ;
	l_stub= ip->i_version ;
	DEC_VERSION_BE(&l_stub) ;
	p= buf_put_int32(p, end, l_stub) ;
	if ( ip->i_flags&I_INUSE ) {
	    l_stub= ip->i_size ;
	    DEC_FILE_SIZE_BE(&l_stub) ;
	    p= buf_put_int32(p, end, l_stub) ;
	    p= buf_put_port(p, end, &ip->i_random) ;
	    if ( send_repl ) {
	    	p= buf_put_int16(p, end, (int16)Inode_reptab[current]) ;
	    }
	}
	if ( !locked ) unlock_inode(current) ;
	/* If p==NULL we failed to put the last in, send the buffer
	 * and retry the last.
	 */
	if ( p==NULL ) {
	    qb->hdr.h_size= current-qb->hdr.h_offset ;
	    dbmessage(5,("broadcasting inodes %ld..%ld",qb->hdr.h_offset,current-1));
	    bs_grpreq(&qb->hdr, buf, lastp-buf, mode) ;
	    p= buf ; 
	    qb->hdr.h_offset= current ;
	} else {
	    ip++ ; current++; n_to_do-- ;
	}
    }
    /* We never get here with p==buf */
    assert( p!=buf ) ;
    qb->hdr.h_size= current-qb->hdr.h_offset ;
    dbmessage(5,("broadcasting inodes %ld..%ld",qb->hdr.h_offset,current-1));
    bs_grpreq(&qb->hdr, buf, p-buf,mode) ;
    bs_grp_rel(qb) ;

    return STD_OK ;
}

/* Internal states for recv_inodes */
#define	IN_FREE		0	/* Free inode */
#define IN_COMMITTED	1	/* Committed inode */
#define IN_DESTROYED	2	/* Destroyed */
#define IN_MAX		2	/* Max IN value */

static char *in_names[IN_MAX+1] = {
	"free", "committed", "destroyed"
} ;

/* Switches for action to take when comparing inode information */
#define ACT_NONE	0	/* no action */
#define ACT_BARF	1	/* Can't happen */
#define ACT_COMPARE	2	/* Compare inodes */
#define ACT_UPDATE	3	/* Update inode information */
#define ACT_DESTROY	4	/* Kill inode info by owner */
#define ACT_REPLY	5	/* Respond to out-of-date inode */
#define ACT_MOPUP	6	/* ACT_REPLY when bs_mopup is set */
#define ACT_AS_IF	7	/* ACT_MOPUP when owner is missing */

static owner_action [3][3] = {
/* remote:	   	FREE		COMMITTED	DESTROYED */
/* local: FREE */     {	ACT_NONE,	ACT_BARF,	ACT_BARF	},
/*	  COMMITTED */{	ACT_REPLY,	ACT_COMPARE,	ACT_DESTROY	},
/*	  DESTROYED */{	ACT_BARF,	ACT_BARF,	ACT_BARF	}
} ;

static from_owner_action [3][3] = {
/* remote:		FREE		COMMITTED	DESTROYED */
/* local: FREE */     {	ACT_NONE,	ACT_UPDATE,	ACT_BARF	},
/*	  COMMITTED */{	ACT_BARF,	ACT_COMPARE,	ACT_BARF	},
/*	  DESTROYED */{	ACT_BARF,	ACT_MOPUP,	ACT_BARF	}
} ;

static no_owner_action [3][3] = {
/* remote:		FREE		COMMITTED	DESTROYED */
/* local: FREE */     {	ACT_NONE,	ACT_UPDATE,	ACT_DESTROY	},
/*	  COMMITTED */{	ACT_AS_IF,	ACT_COMPARE,	ACT_DESTROY	},
/*	  DESTROYED */{	ACT_AS_IF,	ACT_AS_IF,	ACT_NONE	}
} ;

static void
recv_inodes(hdr, buf, size)
header *hdr;
char   *buf;
int     size;
{
    unsigned			n_to_do ; /* Short enough to stay an 'int' */
    register char		*p, *end ;
    int				lstate, gstate ;
    register struct Inode	*ip ;
    Inodenum			current ;
    int				sender ;
    struct Inode		New ;
    struct Inode		Old ;
    int				Changed ;
    uint32			Old_version ;
    int				action ;
    int16			repmask ;
    int16			sender_mask ;
    int16			my_mask ;
    int16			tmp_mask ;
    int				s_mine ;
    int				s_his ;
    int				do_flush ;

    do_flush=0 ;
    end= buf+size ;
    current= hdr->h_offset ;
    n_to_do= hdr->h_size ;
    sender= hdr->h_extra ;
    sender_mask = 1<<sender ;
    my_mask= 1<<Superblock.s_member ;
    p= buf ;

    dbmessage(5,("receiving inode update for %ld..%ld from %d",hdr->h_offset,
			hdr->h_offset+n_to_do-1,sender));
    if ( sender==Superblock.s_member ) return ;
    for( n_to_do= hdr->h_size, ip=Inode_table + current ; n_to_do!=0 ;
	 				n_to_do--, ip++, current++) {
	if ( p<=end-2 ) {
	    New.i_owner= *p++ ;
	    New.i_flags= *p++ ;
	} else p=0 ;
	p= buf_get_int32(p, end, (int32 *)&New.i_version) ;
	if ( New.i_flags&I_INUSE ) {
	    p= buf_get_int32(p, end, &New.i_size) ;
	    p= buf_get_port(p, end, &New.i_random) ;
	    if (New.i_flags&I_REPTO) {
		/* get replica mask */
		p= buf_get_int16(p, end, &repmask) ;
	    } else {
		repmask= sender_mask ;
	    }
	} else {
	    repmask=0 ;
	}
	dbmessage(44,("inode %ld, flags 0%x, owner %d",current,New.i_flags,New.i_owner));
	if ( !p ) {
	    message("Received short inode buffer, %d remaining",n_to_do) ;
	    return ;
	}
	/* Now intertwine the info received with our own info */
	/* Most of the time we only need a read lock, but the current
	 * lock version does not distinguish between the two.
	 */
	Changed=0 ;
	lock_inode(current,L_WRITE) ;

	/* compare versions */
	/* Two dimensional switch table: ONE stands for "I have
	 * higher version", TWO stands for "he has better version". 
	 *
	 *		I am owner	He is owner	Neither is owner
	 * INCOMP	ignore		copy his info	ignore
	 * EQUAL	process		process		process
	 * ONE		ignore		complain	ignore
	 * TWO		complain	copy his info	copy his info
	 */
	Old_version=ip->i_version ;
	DEC_VERSION_BE(&Old_version);
	if ( Old_version!=New.i_version ) {
	    int result;
	    /* Actions to take when versions don't match */
	    result= bs_version_cmp(Old_version,New.i_version) ;
	    dbmessage(16,("version_cmp for %ld returns %d",current,result));
	    if ( Superblock.s_member==ip->i_owner ) switch (result) {
		/* I am owner, ignore previous versions, scream peers
		 * pretending they know a later version.
		 */
		case VERS_INCOMP:
		case VERS_ONE:
			/* Broadcast my new version .... */
			dbmessage(9,("Send out update info for %ld",current));

			unlock_inode(current) ;
			bs_publish_inodes(current,(Inodenum)1,SEND_NOWAIT) ;
			continue ;
		case VERS_TWO:
		 	bwarn("Peer(%d) has later version for my inode %ld, his_owner is %d",
						sender,current,New.i_owner);

			unlock_inode(current) ;
			continue ;
	    } if ( sender==ip->i_owner ) switch (result) {
		/* The other is owner, scream if my version seems better,
		 * ignore everything else.
		 */
		case VERS_INCOMP:
			bwarn("Owner has strange version for inode %ld",current);
		case VERS_TWO:
			take_inode(current,ip,&New,sender) ;
			Changed=1 ;
			break ;
		case VERS_ONE:
			dbmessage(9,("Have better version for %ld than owner(%d)",
						current,sender));
			/* This happens when inodes change ownership..
			   and the owner did not notice.
			   Resend..
			 */
			unlock_inode(current) ;
			bs_publish_inodes(current,(Inodenum)1,
						SEND_NOWAIT) ;
			continue ;
	    } else switch (result) {
		/* Neither is owner, believe other if it has later version.
		 */
		case VERS_INCOMP:
		case VERS_ONE:
			unlock_inode(current) ;
			if ( bs_mopup ) {
			    dbmessage(9,("Mop up update info for %ld",
						current));
			    bs_publish_inodes(current,(Inodenum)1,
						SEND_NOWAIT) ;
			}
			continue ;
		case VERS_TWO:
			take_inode(current,ip,&New,sender) ;
			Changed=1 ;
			break ;
	    }
	}
		
	if ( ip->i_owner!=New.i_owner &&
	     New.i_owner!=S_UNKNOWN_ID &&
	     ip->i_owner!=S_UNKNOWN_ID ) {
	    /* A last resort to change ownership of orphaned inodes.
	     * If the previous owner is not active any more beleive
	     * any new information.
	     */
	    int		dead_member = 0 ;
	    int		his_state ;

	    if ( ip->i_owner>=S_MAXMEMBER ) {
		if ( ip->i_owner!=S_UNKNOWN_ID ) dead_member=1 ;
	    } else {
		super_lock();
		his_state=Superblock.s_memstatus[ip->i_owner].ms_status ;
		super_free();
		if ( his_state<MS_ACTIVE ) dead_member=1 ;
	    }

	    if ( dead_member ) {
		if ( !Changed ) lock_ino_cache(current) ;
		ip->i_owner= New.i_owner ;
		Changed=1 ;
	    } else {
		bwarn("Inode %ld, %d says owned by %d, I say owned by %d",
			current,sender,New.i_owner,ip->i_owner) ;
		if ( New.i_owner > ip->i_owner ) {
		    /* Believe the highest numbered */
		    bwarn("Changing owner of %ld to %d",current,New.i_owner);
		    /* take_inode should not have happened here */
		    if ( !(ip->i_flags&I_ALLOCED) ) {
			if ( ip->i_owner==Superblock.s_member ) {
			    bs_local_inodes_free--;
			} else if ( New.i_owner==Superblock.s_member ) {
			    bs_local_inodes_free++;
			}
		    }
		    if ( !Changed ) lock_ino_cache(current) ;
		    ip->i_owner= New.i_owner;
		    Changed=1 ;
		} else {
		    /* Broadcast our info ... */
		    bs_publish_inodes(current,(Inodenum)1,SEND_NOWAIT) ;
		}
	    }
	} /* Ownership is secure */
	if ( ip->i_flags&I_DESTROY )     lstate=IN_DESTROYED;
	else if ( ip->i_flags&I_INUSE )  lstate=IN_COMMITTED;
	else				 lstate=IN_FREE;
	if ( New.i_flags&I_DESTROY )     gstate=IN_DESTROYED;
	else if ( New.i_flags&I_INUSE )  gstate=IN_COMMITTED;
	else				 gstate=IN_FREE;
	Old= *ip ;
	DEC_DISK_ADDR_BE(&Old.i_dskaddr);
	DEC_FILE_SIZE_BE(&Old.i_size);
	if ( ip->i_owner==Superblock.s_member ) {
	    action= owner_action[lstate][gstate] ;
	} else if ( New.i_owner==sender ) {
	    action= from_owner_action[lstate][gstate] ;
	} else {
	    action= no_owner_action[lstate][gstate] ;
	}
#if DEBUG_LEVEL>0
	if ( (New.i_flags&(I_INUSE|I_REPTO))==(I_INUSE|I_REPTO) && New.i_size ) {
	    /* Inode and replica mask are there, check .... */
	    /* First check whether the source info is
	     * consistent.
	     */
	    if ( New.i_flags&I_PRESENT ) {
		if ( !(repmask&sender_mask) ) {
		    bwarn("inode %d is present on %d, but not in repmask",
					current,sender);
		}
		repmask |= sender_mask ;
	    } else {
		if ( repmask&sender_mask ) {
		    bwarn("inode %d is not present on %d, but in repmask",
					current,sender);
		}
		repmask &= ~sender_mask ;
	    }
	}
#endif
	switch ( action ) {
	case ACT_NONE :
		dbmessage(14,("Ignoring inode %ld",current));
		if ( Changed ) writeinode(ip,ANN_NEVER,SEND_NOWAIT) ;
		break ;
	case ACT_BARF:
		message("Inode %ld, owner %d, has state '%s', other(%d) says '%s'",
			current, ip->i_owner,in_names[lstate],
			sender, in_names[gstate]);
		if ( Changed ) writeinode(ip,ANN_NEVER,SEND_NOWAIT) ;
		break ;
	case ACT_COMPARE:
		dbmessage(24,("Comparing inode %ld",current));
		if ( !PORTCMP(&Old.i_random,&New.i_random) ) {
		    message("Own inode %ld, owner %d, random number differs from other(%d)",
			current, ip->i_owner, sender) ;
		    if ( Changed ) unlock_ino_cache(current) ;
		    destroy_inode(current,1,ANN_DELAY|ANN_ALL,SEND_NOWAIT) ;
		} else if ( Old.i_size != New.i_size ) {
		    message("Inode %ld, owner %d, has size %ld, other(%d) says %ld",
			current, ip->i_owner, Old.i_size, sender, New.i_size) ;
		    if ( Changed ) unlock_ino_cache(current) ;
		    destroy_inode(current,1,ANN_DELAY|ANN_ALL,SEND_NOWAIT) ;
		} else {
		    if ( Changed ) writeinode(ip,ANN_NEVER, SEND_NOWAIT) ;
		    s_his=0 ;
	    	    /* Inode is there, update the replica mask */
	    	    /* We might set bits for replicas that just disappeared,
		     * but the storage servers will receive the same message
	    	     * and repeat the notification of disappearance.
		     * This does not work for the server from which the
		     * message originates. For that one broadcasts with
		     * inodes must be delayed until the big message is out.
		     */

		    tmp_mask= Inode_reptab[current] ;
		    /* Check for disappearing replicas */ 
		    if ( (tmp_mask&sender_mask) && !(repmask&sender_mask) ) {
			/* The contents of the inode disappeared */
			tmp_mask &= ~sender_mask ;
			if ( ip->i_owner==Superblock.s_member ) {
			    /* Do a replication check */
			    gs_lostrep();
			}
		    }
		    if ( Old.i_size ) {
			/* Check consistency of own reptab information */
			if ( Old.i_flags&I_PRESENT ) {
			    s_mine=1 ;
			    if ( !(tmp_mask&my_mask) ) {
				bwarn("Own replica of %ld not noted in reptab",
							current) ;
				tmp_mask |= my_mask ;
			    }
			} else {
			    s_mine=0 ;
			    if ( tmp_mask&my_mask ) {
				bwarn("Missing own replica of %ld noted in reptab",
							current) ;
				tmp_mask &= ~my_mask ;
			    }
			}
			if ( New.i_flags&I_REPTO ) {
			    /* Check whether the other's and my opinion on
			     * having a local replica fit.
			     */
			    if ( repmask&my_mask ) s_his=1 ;
			    if ( s_mine!=s_his ) {
				bs_publish_inodes(current,(Inodenum)1,
							SEND_NOWAIT);
				if ( s_his ) repmask &= ~my_mask ;
			    }
			}
			/* Finally, update the local info ... */
			Inode_reptab[current] = tmp_mask|repmask ;
		    }
		}
		break ;
	case ACT_UPDATE:
		if ( Changed ) unlock_ino_cache(current) ;
		dbmessage(12,("updating inode %ld on %d's info",
					current,sender));
		update_inode(current,&New) ;
		if ( (New.i_flags&(I_INUSE|I_REPTO))==(I_INUSE|I_REPTO) ) {
		    /* Check the replica mask */
		    /* We might set bits for replicas that just disappeared,
		     * but the storage servers will receive the same message
		     * and repeat the notification of disappearance.
		     * This does not work for the server from which the
		     * message originates. For that one broadcasts with
		     * inodes must be delayed until the big message is out.
		     */
		    if ( repmask&my_mask ) {
			repmask&= ~my_mask ;
			bs_publish_inodes(current,(Inodenum)1,SEND_NOWAIT);
		    }
		}
		Inode_reptab[current] = repmask ;
		break ;
	case ACT_DESTROY:
		if ( Changed ) unlock_ino_cache(current) ;
		dbmessage(3,("destroying %ld on %d's authority",
					current,sender));
		destroy_inode(current,1,ANN_MINE,SEND_NOWAIT) ;
		break ;
	case ACT_AS_IF:
		if ( grp_member_present((int)ip->i_owner) ) break ;
	case ACT_MOPUP :
		if ( !bs_mopup ) break ;
	case ACT_REPLY:
		/* Respond to out-of-date inode */
		dbmessage(9,("Mop up reply info for %ld",
						current));
		do_flush=1 ;
		unlock_inode(current) ;
		bs_publish_inodes(current,(Inodenum)1,SEND_NOWAIT) ;
		continue ;
	}
	unlock_inode(current) ;
    }
    if ( do_flush ) bs_publist(SEND_NOWAIT);
}

/* A new version came in. Free the current resources and update
 * ownership (if needed) and version number.
 */

static void
take_inode(current,ip,New,sender)
	Inodenum	current;
	Inode		*ip;
	Inode		*New;
	int		sender;
{
	

	/* Clear out contents of current inode */
	if (ip->i_flags & I_INUSE) {
		dbmessage(5,("getting rid of contents of %ld",current));
		/* We have something here, get rid of it */
		destroy_inode(current,1,0,SEND_NOWAIT) ;
		/* This might upgrade the version number, but
		 * we will overwrite that later.
		 */
	}

	/* We will change the inode, so lock its ino cache block */
	lock_ino_cache(current) ;

	/* Now resolve ownership */
	if ( ip->i_owner!=New->i_owner && New->i_owner!=S_UNKNOWN_ID ) {
	    /* Ownership changed. Always believe the incoming higher
	     * numbered version. But complain when you see one of your own
	     * changing hands.
	     */
	    /* The first two cases are if the sender says it is the owner */
	    /* The third case is if the sender pretends it is my inode
	     * and I think the sender owned the inode. This constitutes
	     * change of ownership.
	     * The remaining cases is when the sender is not the owner,
	     * These are ignored, unless the current owner is unknown.
	     */
	    if ( ip->i_owner==Superblock.s_member ) {
		    bwarn("Changing owner of %ld to %d",current,sender);
		    /* Must be a free inode at this stage */
		    bs_local_inodes_free--;
	    }
	    dbmessage(12,("Changing ownership of %ld from %d to %d",
				current,ip->i_owner,New->i_owner));
	    /* New ownership. */
	    ip->i_owner= New->i_owner;
	    if ( ip->i_owner==Superblock.s_member ) {
		/* Must be a free inode at this stage */
		bs_local_inodes_free++ ;
	    }

	} /* Ownership is secure */

	/* We now have a free inode, update version number, reset its
	 * lifetime and
	 * leave rest of processing to calling routine.
	 */
	ip->i_version= New->i_version ;
	ENC_VERSION_BE(&ip->i_version);
	ip->i_flags= 0;

}

/* A static routine that finds a fellow member that is active and willing
 * to part with the information. -1 is the error return.
 * We assume that the inode has been decoded.
 */

static int
find_fellow(ino,ip,restricted)
	Inodenum	ino;
	Inode		*ip;
	int		restricted;	/* Is only request for info */
{
	int		member ;
	uint16		memstatus ;
	peer_bits	rep_status ;
	int		i ;

	if ( !(ip->i_flags&I_INUSE) ) return -1 ;
	assert (!(ip->i_flags&I_PRESENT) ) ;
	member= Superblock.s_member ;
	rep_status= Inode_reptab[ino] ;
	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
		/* We try to take the next highest */
		member= (member+1)%S_MAXMEMBER ;
		if ( !( (rep_status>>member)&1 ) ) {
			/* We have no record of the file being there */
			continue ;
		}
		super_lock();
		memstatus= Superblock.s_memstatus[member].ms_status ;
		super_free();
		if ( memstatus==MS_ACTIVE ||
		    ( restricted && memstatus==MS_PASSIVE ) ) {
			if ( grp_member_present(member) ) return member ;
		}
	}
	/* Fall through, we do not know anything */
	return -1 ;	
}


/* Get file contents from a fellow member */
errstat
bs_member_read(ino, ip, buf, size)
	Inodenum ino ;
	struct Inode_cache *ip;
	bufptr buf;
	b_fsize size;
{
	capability	file_cap ;
	b_fsize		dummy ;
	int		member ;
	
	member= find_fellow(ino,(Inode *)ip,1) ;
	if ( member<0 ) return STD_NOTNOW ;
	dbmessage(4,("Reading from member %d",member));
	
	/* Build the capability */
	super_lock();
	file_cap.cap_port= Superblock.s_memstatus[member].ms_cap.cap_port ;
	super_free();
	if (prv_encode(&file_cap.cap_priv, ino, BS_RGT_ALL, &ip->i_random) < 0)
	{
	    return STD_SYSERR;
	}

	
	/* Read the file */
	return b_read(&file_cap, (b_fsize)0, (char *)buf, size, &dummy);
}

/* Try to create a new file */
/* If local space is tight we try to defer the request */
errstat
bs_new_inode(cpp)
Cache_ent **	cpp;		/* addr of pointer to original file */
{
    disk_addr	free_blocks ;
    int		member ;
    uint16	memstatus ;
    disk_addr	maxfree ;
    disk_addr	thisfree ;
    int		defer_member ;
    Cache_ent *	new;


    free_blocks = a_notused(A_DISKBLOCKS);
    if ( free_blocks<BS_DEFER_THRESHOLD ) {
	/* We are low on blocks, is there anyone else with
	   more than we have?
	 */
	maxfree= free_blocks ;

	for ( member=0 ; member<S_MAXMEMBER ; member++ ) {
		/* Deferring to yourself is issue here */
		if ( member==Superblock.s_member ) continue ;
		/* We only defer to members currently present */
		super_lock();
		memstatus= Superblock.s_memstatus[member].ms_status ;
		super_free();
		if ( memstatus==MS_ACTIVE && grp_member_present(member) ) {
			thisfree=bs_grp_bl_free(member) ;
			/* Deduct 20% */
			thisfree-= thisfree/5 ;
			if ( thisfree>maxfree && bs_grp_ino_free(member)>1 ) {
				maxfree= thisfree ;
				defer_member= member ;
			}
		}
	}
	
	if ( maxfree>free_blocks ) {
		if ( free_blocks!=bs_grp_bl_free((int)Superblock.s_member) ) {
			/* When number last broadcasted differs,
			   re-broadcast
			 */
			bs_send_resources(SEND_CANWAIT);
		}
		/* Somebody else has more space, try to use it */
		return grp_pass_request(defer_member) ;
	}
	/* Fall through, nobody is better of than you */
    }

    /* Try to create the file yourself */
    if ( no_creation || (new = new_file()) == 0)
    {
	/* It failed, perform last ditch attempt to defer */
	maxfree= 0 ;

	for ( member=0 ; member<S_MAXMEMBER ; member++ ) {
		/* Deferring to yourself is not an option we wish to use */
		if ( member==Superblock.s_member ) continue ;
		/* We only defer to members currently present */
		super_lock();
		memstatus= Superblock.s_memstatus[member].ms_status ;
		super_free();
		if ( memstatus==MS_ACTIVE && grp_member_present(member) ) {
			thisfree=bs_grp_bl_free(member) ;
			if ( thisfree>maxfree && bs_grp_ino_free(member)>0 ) {
				maxfree= thisfree ;
				defer_member= member ;
			}
		}
	}

	if ( maxfree>0 ) {
		if ( free_blocks!=bs_grp_bl_free((int)Superblock.s_member)
				||
		     bs_local_inodes_free!=bs_grp_ino_free((int)Superblock.s_member)) {
			/* When number last broadcasted differs,
			   re-broadcast
			 */
			bs_send_resources(SEND_CANWAIT);
		}
		return grp_pass_request(defer_member) ;
	}

	/* Fall through, we do not know anything */
	return STD_NOSPACE ;	
    }

    *cpp= new ;
    return STD_OK;
}

/* A routine that defers to a brother/sister for an existing file. Returns
 * BULLET_LAST_ERR if request has been deferred, even when that forwarding
 * has not succeeded. This is because we lost control over the reply anyhow.
 * Anything else is an error that allows further handling and a putrep.
 */
errstat
bs_grp_defer(ino,ip,restricted)
	Inodenum	ino;
	Inode		*ip;
	int		restricted;	/* Is only request for info */
{
	int		member ;

	member= find_fellow(ino,ip,restricted) ;
	if ( member<0 ) return STD_NOTNOW ;

	return grp_pass_request(member) ;
}

/* A routine that passes a single request to a designated server
 * at the private port.
 */

errstat
bs_grp_single(member,hdr,buf,cnt)
	int	member ;
	header	*hdr;
	bufptr	buf;
	int	cnt;
{
	header		copy_hdr ;
	header		result_hdr ;
	bufsize		n;
	int		his_status;

	copy_hdr= *hdr ;

	if ( member>=Superblock.s_maxmember || member<0 ) {
		return STD_ARGBAD ;
	}
	super_lock();
	his_status=Superblock.s_memstatus[member].ms_status ;
	if ( his_status==MS_NONE ) {
		super_free();
		return STD_ARGBAD ;
	}
	if ( !grp_member_present(member) ) {
		super_free();
		return STD_NOTFOUND ;
	}

	copy_hdr.h_port = Superblock.s_memstatus[member].ms_cap.cap_port ;
	copy_hdr.h_priv = Superblock.s_memstatus[member].ms_cap.cap_priv;
	super_free();
	n = trans(&copy_hdr, buf, (bufsize)cnt, &result_hdr, NILBUF, 0);
	dbmessage(67,("single trans for %d returns %d, status %d",
				member,n,result_hdr.h_status)) ;
	if (ERR_STATUS(n)) {
		return ERR_CONVERT(n) ;
	} else {
	   	return ERR_CONVERT(result_hdr.h_status);
	}
}


/* A routine that sends a specific hdr, buffer to all active peers.
 * All of them are always called, if any of them returns an error
 * one of the errors is returned, otherwise STD_OK.
 * Do not send anything to yourself.
 * In theory this could be optimized to run these trans'es simultaneously.
 * We do not bother for the moment.
 */
errstat
bs_grp_all(hdr,buf,cnt)
	header	*hdr;
	bufptr	buf;
	int	cnt;
{
	errstat		last_err= STD_OK ;
	int		i ;
	header		copy_hdr ;
	header		result_hdr ;
	bufsize		n;
	errstat		err;
	int		his_status;

	copy_hdr= *hdr ;

	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
		super_lock();
		his_status=Superblock.s_memstatus[i].ms_status ;
		if ( i!=Superblock.s_member &&
		     (his_status==MS_ACTIVE || his_status==MS_PASSIVE) &&
		     grp_member_present(i) ) {
	copy_hdr.h_port = Superblock.s_memstatus[i].ms_cap.cap_port ;
	copy_hdr.h_priv = Superblock.s_memstatus[i].ms_cap.cap_priv;
			super_free();
			n = trans(&copy_hdr, buf, (bufsize)cnt,
					&result_hdr, NILBUF, 0);
			dbmessage(67,("multi trans for %d returns %d, status %d",
				i,n,result_hdr.h_status)) ;
			if (ERR_STATUS(n)) {
				err= ERR_CONVERT(n) ;
			} else {
	    			err= ERR_CONVERT(result_hdr.h_status);
			}
			if ( err!=STD_OK ) last_err=err ;
		} else super_free();
	}
	return last_err ;
}
/* A routine returning the number of missing members.
 */
int
bs_grp_missing()
{
	int	i;
	int	missing;
	int	his_status;

	/* Must make sure that all active/passive members are present */
	super_lock(); missing=0 ;
	for ( i=0 ; i<Superblock.s_maxmember ; i++ ) {
	   his_status= Superblock.s_memstatus[i].ms_status ;
	   if ( (his_status==MS_ACTIVE || his_status==MS_PASSIVE) &&
		!grp_member_present(i) ) {
		missing++ ;
	   }
	}
	super_free();
	return missing;
}

/* A routine returning whether we have a majority.
 */
int
bs_has_majority()
{
	int	i;
	int	active;
	int	existing ;
	int	his_status;

	super_lock(); active=0 ; existing=0 ;
	for ( i=0 ; i<Superblock.s_maxmember ; i++ ) {
	   his_status= Superblock.s_memstatus[i].ms_status ;
	   if ( (his_status==MS_ACTIVE || his_status==MS_PASSIVE) ) {
		existing++ ;
		if ( grp_member_present(i) ) active++ ;
	   }
	}
	super_free();
	return 2*active>existing ;
}

/* Resource maintenance */

/* The routine that sends resource information to all members.
   Resource information contains:
    hdr:
	h_command	BS_RESOURCE
	h_extra		# of sender member
	h_size		# bytes in message 

    buf:
	# free blocks	string, decimal
	# free inodes	string, decimal
	# inodes in use string, decimal
 */
void
bs_send_resources(mode)
	int		mode;
{
	header		hdr ;
	char		msgbuf[100] ;
	char		*p, *end ;

	hdr.h_command= BS_RESOURCES ;
	hdr.h_extra= Superblock.s_member ;

	end= msgbuf + sizeof msgbuf ; p= msgbuf ;
	p= bprintf(p, end, "%ld %ld %ld",
		(long)a_nfree(A_DISKBLOCKS),
		(long)bs_local_inodes_free,
		(long)Superblock.s_numblocks) ;

	hdr.h_size= p+1-msgbuf ;
	dbmessage(5,("Sending resource %s",msgbuf));

	bs_grpreq(&hdr, (bufptr)msgbuf, (int)hdr.h_size, mode ) ;
}

static void
recv_resources(hdr,msgbuf,size)
	header		*hdr ;
	char		*msgbuf ;
{
	int		member ;
	long		blocks_free ;
	long		inodes_free ;
	long		his_blocks ;
	char		*p, *next_p ;

	member= hdr->h_extra ;

	dbmessage(15,("resources from %d: `%s`",member, size, msgbuf)) ;
	p= msgbuf ;

	if ( msgbuf[size-1] ) {
		bwarn("internal error; illegal resources format");
		return ;
	}
	blocks_free= strtol(p,&next_p, 10) ;
	if ( p!=next_p ) { /* Success */
		p= next_p ;
		inodes_free= strtol(p,&next_p, 10) ;
	}
	if ( p!=next_p ) { /* Success */
		p= next_p ;
		his_blocks= strtol(p,&next_p, 10) ;
	}
	if ( p==next_p ) { /* Failure */
		bwarn("internal error; illegal resources format");
		return ;
	}

	mu_lock(&res_lock) ;
	bs_member_resources[member].bl_free= blocks_free ;
	bs_member_resources[member].in_free= inodes_free ;
	bs_member_resources[member].total_blocks=  his_blocks ;
	mu_unlock(&res_lock) ;

}

Inodenum bs_grp_ino_free(member)
	int member ;
{
	Inodenum retval ;

	mu_lock(&res_lock) ;
	retval= bs_member_resources[member].in_free ;
	mu_unlock(&res_lock) ;
	return retval ;
}

disk_addr bs_grp_blocks(member)
	int member ;
{
	disk_addr retval ;

	mu_lock(&res_lock) ;
	retval= bs_member_resources[member].total_blocks ;
	mu_unlock(&res_lock) ;
	return retval ;
}

disk_addr bs_grp_bl_free(member)
	int member ;
{
	disk_addr retval ;

	mu_lock(&res_lock) ;
	retval= bs_member_resources[member].bl_free ;
	mu_unlock(&res_lock) ;
	return retval ;
}

/* And the sweeper routine that once in a while sends information out */
void
bs_grp_sweep() {
	long		epsilon ;
	register long	oldvalue ;
	long		difference ;
	int		doit ;
	static int	elapsed ;

	/* age information is sent out before inode information! */
	gs_age_sweep() ;
	bs_publist(SEND_CANWAIT) ;

	doit=0 ;

	mu_lock(&res_lock) ;
	oldvalue= bs_member_resources[Superblock.s_member].bl_free ;
	/* Do not send this unless there has been a significant change or
	   after some time elapsed */
	
	elapsed++ ;
	doit= elapsed>=RES_SWEEPS ;
	if ( !doit ) {
		difference= oldvalue - a_nfree(A_DISKBLOCKS) ;
		if ( difference<0 ) difference = -difference ;
		epsilon= oldvalue/20 ;

		if ( difference>epsilon ) doit=1 ;
	}
	if ( !doit ) {
		oldvalue= bs_member_resources[Superblock.s_member].in_free ;
		difference= oldvalue - bs_local_inodes_free ;
		if ( difference<0 ) difference = -difference ;
		epsilon= oldvalue/20 ;
	
		if ( difference>epsilon ) doit=1 ;
	}
	mu_unlock(&res_lock) ;
	if ( doit ) {
		bs_send_resources(SEND_CANWAIT);
		elapsed=0 ;
	}
}

/* A second sweeper running functions that might take long */
/* None of these functions need to be called when sync'ing */
#if defined(USER_LEVEL)
/*ARGSUSED*/
void
bullet_back_sweeper(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
bullet_back_sweeper()
#endif
{
    mutex			blocker;
    static int			elapsed ;
    char			retry_msgs[80] ;

    mu_init(&blocker);
    mu_lock(&blocker);

    for (;;)
    {
	/* Sleep a while! (since we are already locked we always time out) */
	(void) mu_trylock(&blocker, (interval) BS_SWEEP_TIME);
	bs_check_state();

	/* Take group actions first */

	gs_sweeprep();
	bs_shuffle_free() ;

	elapsed++ ;
	if ( elapsed>=RES_SWEEPS ) {
		elapsed=0 ;
		if ( grp_member_present((int)Superblock.s_member ) ) {
			bs_grp_depart();
		}
	}
	if ( bs_ls_sick ) {
	    /* logging was disabled, try restarting */
	    bs_ls_sick= 0 ;
	    strcpy(retry_msgs,"restarting logging\n") ;
	    bs_log_send(retry_msgs,strlen(retry_msgs),sizeof retry_msgs) ;
	}

    }
}

static errstat
b_detach(svrcap)
capability *svrcap;
{
    header  hdr;
    errstat err;
    bufsize siz;
 
 
    hdr.h_port = svrcap->cap_port;
    hdr.h_priv = svrcap->cap_priv;
    hdr.h_command = BS_STATE;
    hdr.h_size= Superblock.s_member;
    hdr.h_extra= BS_CAIN|BS_STATE_FORCE;
    hdr.h_offset= -1 ;
 
    siz = trans(&hdr, (bufptr) NULL, (bufsize)0, &hdr, (bufptr) NULL, (bufsize) 0);
    if (ERR_STATUS(siz)) {
	err = ERR_CONVERT(siz);
    } else {
	err = ERR_CONVERT(hdr.h_status);
    }
 
    return err;
}
 

/* Called to check whether storage servers not currently present
 * are active. Kill all storage servers between yourself and the next
 * lower storage server that is present.
 */
static void
bs_grp_depart() {
	int		other ;
	errstat 	err ;
	int		his_status ;
	capability	his_cap ;

	other= Superblock.s_member ;
	while ( other>0 ) {
		other-- ;
		super_lock();
		his_status= Superblock.s_memstatus[other].ms_status ;
		his_cap= Superblock.s_memstatus[other].ms_cap ;
		super_free();
		switch ( his_status ) {
		case MS_ACTIVE :
		case MS_PASSIVE :
			if ( grp_member_present(other) ) return ;
			/* Try to kill */
			dbmessage(16,("trying to kill %d",other));
			err=b_detach(&his_cap);
			if ( err!=STD_NOTFOUND ) {
			    dbmessage(16,("murder attempt on %d returned %s",
				other,err_why(err)));
			}
			break ;
		}
	}
}

/* Increase version number. This can be a routine because it is
 * not called often. Only when ownership changes or an inode is destroyed.
 * Assumption: Inode is locked and will be written later.
 */

void
bs_version_incr(ip)
	Inode *	ip;	/* inode pointer */
{
	int32			version_stub ;

	version_stub= ip->i_version ;
	DEC_VERSION_BE(&version_stub) ;
	if ( version_stub==UNKNOWN_VERSION ) {
		bwarn("attempt to increase an unknown version number for inode %ld",ip-Inode_table) ;
		return ;
	}
	version_stub++ ;
	if ( version_stub<0 || version_stub>=MAX_VERSION ) version_stub=1 ;
	ENC_VERSION_BE(&version_stub) ;
	ip->i_version = version_stub ;
}

/* Compare two version numbers */
int
bs_version_cmp(vers1,vers2)
	uint32	vers1;
	uint32	vers2;
{
	int32	vers_diff ;

	if ( vers1==vers2 ) return VERS_EQUAL ;
	if ( vers1==UNKNOWN_VERSION ) {
		/* If two would be zero too, we would not be here ... */
		return VERS_TWO ;
	}
	if ( vers2==UNKNOWN_VERSION ) {
		/* If one would be zero too. we would not be here... */
		return VERS_ONE ;
	}
	if ( vers1<= VERSION_WINDOW ) vers1 += MAX_VERSION ;
	if ( vers2<= VERSION_WINDOW ) vers2 += MAX_VERSION ;
	vers_diff= (int32)vers1-(int32)vers2 ;
	if ( vers_diff>0 && vers_diff<=VERSION_WINDOW ) return VERS_ONE ;
	if ( vers_diff<0 && vers_diff>=-VERSION_WINDOW ) return VERS_TWO ;
	return VERS_INCOMP ;
}


/* Miscellaneous */ 
#ifdef __STDC__
#include <stdarg.h>
#define va_dostart(ap, format)  va_start(ap, format)
#else
#include <varargs.h>
#define va_dostart(ap, format)  va_start(ap)
#endif

static mutex Grp_print_mu;

void bs_print_lock() {
    mu_lock(&Grp_print_mu);
}

int bs_print_trylock(delay)
	interval	delay ;
{
    return mu_trylock(&Grp_print_mu,delay);
}

void bs_print_unlock() {
    mu_unlock(&Grp_print_mu);
}

#ifdef __STDC__
void message(char *format, ...)
#else
/*VARARGS1*/
void message(format, va_alist) char *format; va_dcl
#endif
{
    int id;
    va_list ap;
    char	buf[MAX_LOG_BUF] ;
    char	*p ;
    int		count ;


    va_dostart(ap, format);
    id = grp_static_id();
    if (id >= 0) {
	p= bprintf(buf,&buf[MAX_LOG_BUF],"GBULLET %d: ", id);
    } else {
	p= bprintf(buf,&buf[MAX_LOG_BUF],"GBULLET <unknown>: ");
    }
    count= p-buf ;
    count+= vsprintf(&buf[count], format, ap);
    buf[count++]= '\n' ; buf[count]= 0 ;
    va_end(ap);

    if ( bs_print_msgs ) {
	mu_lock(&Grp_print_mu);
	printf("%s",buf);
	mu_unlock(&Grp_print_mu);
    }
    if ( bs_log_msgs ) {
	bs_log_send(buf,count,MAX_LOG_BUF) ;
    }
}

#ifndef AMOEBA
#ifdef USER_LEVEL
#include <sys/time.h>
/* UNIX only */
long getmilli() {
	static long	ft_sec ;
	struct timeval	tv ;

	gettimeofday(&tv, (struct timezone *)0 ) ;
	if ( !ft_sec ) {
		/* First time */
		ft_sec= tv.tv_sec ;
		return 0 ;
	}
	return (tv.tv_sec-ft_sec)*1000 + tv.tv_usec/1000 ;
}
#endif
#endif
