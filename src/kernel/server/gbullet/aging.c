/*	@(#)aging.c	1.1	96/02/27 14:06:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * AGING.C
 *
 *	This file contains the routines for the aging commands
 *	implemented by the bullet server
 *
 *  Author:
 *	Ed Keizer, 1995
 */

#include "amoeba.h"
#include "group.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "semaphore.h"
#ifdef USER_LEVEL
#include <thread.h>
#else
#include "sys/proto.h"
#include "sys/kthread.h"
#endif
#include "module/prv.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/buffers.h"
#include "grp_comm.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"
#include "assert.h"
#include "grp_defs.h"
#include "event.h"
#include "aging.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */

extern	Superblk	Superblock;	/* in-core superblock */
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	Statistics	Stats;		/* list of statistics */
extern	b_fsize		Blksizemin1;	/* used by NUMBLKS */

/* A general principle is that age information is sent out before
 * inode information. This principle is something violated due to race
 * conditions. If the storage server that should send out the age
 * information crashes the age information wil never be sent and
 * the global age will remain set to unknown until a new server
 * with that inode comes up or the inode is touched.
 * The race especially exists in bs_grp_sweep between age_sweep and publist
 * and while replicating inodes.
 */

/* Locking of ttl structures:
	The global structure has no lock. The values consist of bytes.
	We assume that writes into bytes are atomic. 
	Only a single thread can change the global structure, thus race
	conditions can not occur.

	The values in the local structure belong to the inodes.
	Acquiring an inode lock means that the local ttl value is locked.

	The base value is accessed by both local and global locks.
	It is only changed by the single aging thread. We assume
	that changes into that single object are atomic and
	no locking is necessary.
 */

/* The current base value for the lifetime counters */

int		bs_ttl_base ;	/* initialized to 0 */

#ifndef AGE_IN_INODE
/* Global tables */
unsigned char	*Inode_local_ttl ;
unsigned char	*Inode_global_ttl ;
#endif

/* Local send buffers and their locks */
static	unsigned char	ttl_updates[BS_GRP_REQSIZE] ;
static	unsigned char	*p_ttl_updates ;
#define	END_UPDATES	((char *)(ttl_updates+BS_GRP_REQSIZE))
static  mutex	update_mutex ;
static	int	init_done ;

/* Local delaying of age commands */
static semaphore	can_age ;

/* Accept_age and awaiting accept are protected by age_info */
static	int	refuse_age ;	/* Must refuse ages */
static	int	refusing_member ;
static	int	ages_waiting ;	/* # of downs of awaiting accept */
static	mutex	age_info ;

static	semaphore awaiting_accept ; /* Age waiting for refuse_age */

/* Value of refuse-age when my age came in. */
static	int	age_refused ;
/* Value of refuse-age when my stop_age came in and it's lock */
static	int	stop_age_refused ;
static	mutex	can_stop_age ;

/* Threshold value for local aging, set to HARD_THRESHOLD during normal
   operation. Set to 0 when preparing for exit.
 */
static	int	delay_threshold ;

/* Local routines */
static	void	flush_ttls _ARGS((void)) ;
static	void	check_age _ARGS((void)) ;
static	void	send_check_ttl _ARGS((Inodenum, int)) ;
static	void	ttl_announce _ARGS((Inodenum,int)) ;
static	void	dec_ttl_buffer _ARGS((void)) ;

/* GS_INIT_TTL
 * Initialize the ttl structures
 * Do not lock anything, assume it is not necessary.
 */

void
gs_init_ttl()
{
    register Inodenum	i;
#ifndef AGE_IN_INODE
    Res_size		numbytes;

    numbytes = Superblock.s_numinodes;
    CEILING_BLKSZ(numbytes);
    Inode_local_ttl = (unsigned char *) a_alloc(A_CACHE_MEM, numbytes);
    Inode_global_ttl = (unsigned char *) a_alloc(A_CACHE_MEM, numbytes);
    if (Inode_local_ttl == 0 || Inode_global_ttl == 0)
        bpanic("Insufficient memory for ttl structures.");
#endif

    if ( init_done ) return ;

    dbmessage(28,("Init_ttl starts..."));

    p_ttl_updates = ttl_updates ;
    mu_init(&update_mutex) ;
    sema_init(&can_age,0) ;
    sema_init(&awaiting_accept,0);

    refuse_age=1 ; refusing_member= -1 ; delay_threshold= HARD_THRESHOLD ;

    /* Initially set all lifetimes to unknown */
    for (i = 1; i < Superblock.s_numinodes; i++)
    {
	    INIT_LOCAL_TTL(i) ; INIT_GLOBAL_TTL(i) ;
    }
    init_done= 1;
    dbmessage(28,("Init_ttl finishes"));
}

/* GS_ALLOW_AGING
 * The age data is fully up-to-date allow local ages to proceed.
 */
void
gs_allow_aging() {
	sema_up(&can_age);
}

/* GS_FIRST_TTL
 * Initialize the lifetime of all existing files to max.
 */

void
gs_first_ttl()
{
    register Inodenum	i;
    register Inode *	ip;

    if ( !init_done ) {
	bwarn("awkward lifetime initialize");
    }
    for (i = 1, ip = Inode_table + 1; i < Superblock.s_numinodes; i++, ip++)
    {	/* Have a peek .... */
	if ( ip->i_flags&I_INUSE ) {
	    lock_inode(i,L_READ) ;
	    if ( ip->i_flags&I_INUSE ) {
		/* The file is still there, set its lifetime */
	    	SET_LOCAL_TTL(i,MAX_LIFETIME) ;
		SET_GLOBAL_TTL(i,MAX_LIFETIME) ;
	    }
	    unlock_inode(i);
	}
    }
}

/* GS_LATER_TTL
 * Initialize the lifetime of all `uninitialized' inodes.
 */

void
gs_later_ttl()
{
    register Inodenum	i;
    register Inode *	ip;

    for (i = 1, ip = Inode_table + 1; i < Superblock.s_numinodes; i++, ip++)
    {	/* Have a peek .... */
	if ( GLOBAL_TTL(i)==L_UNKNOWN && ip->i_flags&I_INUSE ) {
	    /* We have an unknown global ttl and a file. The file must
	     * have been unknown thus far.
	     */
	    lock_inode(i,L_READ) ;
	    if ( ip->i_flags&I_INUSE ) {
		/* The file is still there, set its lifetime */
	    	gs_set_ttl(i) ;
	    }
	    unlock_inode(i);
	}
    }
    gs_age_sweep() ;
}

/*
 * GS_DO_AGE
 *
 *	This routine increments the base value, thereby decrementing
 *	all time to tives in one blow.
 *	It then checks to see whether one of its own threads is waiting
 *	to go through all files and destroy what is necessary.
 */
void
gs_do_age(hdr)
header *	hdr;
{
    /* Do not lock age_info, the only thread changing refuse_age
     * is the thread calling us.
     */

    dbmessage(15,("Age from member %d, event %d",
		hdr->h_extra,hdr->h_size)) ;
    if ( !refuse_age ) {
	/* Increment the base */
	bs_ttl_base = L_VAL(1) ;
	/* Decreasing all ages... */
	mu_lock( &update_mutex ) ;
	dec_ttl_buffer() ;
	mu_unlock( &update_mutex ) ;
    }

    /* Wake up your slave .... */
    if ( hdr->h_extra==Superblock.s_member ) {
	/* It is one of ours */
	/* If age were blocked, signal that to receiver.
	   We can use a global variable here, because we have only one
	   age running at any one moment.
	 */
	age_refused= refuse_age ;
	event_wake(map_event((int)hdr->h_size)) ;
    }

}

/* GS_STD_AGE
 *	Start aging; broadcast request, wait for it to come in; then
 *	check all files that can be destroyed. Allow only one of these
 *	at one moment.
 */

errstat
gs_std_age(hdr)
	header	*hdr;
{
	b_event		*p_event ;
	int		num_event ;

	if ( !bs_has_majority() ) return STD_NOTNOW ;

	sema_down(&can_age) ;

	num_event=get_event(&p_event) ;
	hdr->h_extra= Superblock.s_member;
	hdr->h_size= num_event ;
	do {
		mu_lock(&age_info) ;
		while ( refuse_age ) {
			ages_waiting++ ;
			mu_unlock(&age_info) ;
			dbmessage(4,("Waiting for age to come through"));
			sema_down(&awaiting_accept);
			mu_lock(&age_info) ;
		}
		mu_unlock(&age_info) ;
		dbmessage(23,("Send age, eventnum %d", num_event)) ;
		event_incr(p_event);
		bs_grpreq(hdr, (bufptr)0, 0, SEND_CANWAIT) ;
		event_wait(p_event) ;
		dbmessage(23,("Woken up: eventnum %d",num_event));
	} while ( age_refused ) ;
	free_event(num_event) ;

	check_age() ;
	
	sema_up(&can_age) ;
	return STD_OK ;
}

/*
 * BS_CHECK_AGE
 *	Goes through every inode (a potentially SLOW operation)
 *	and to check its time to live.  If the time to live dipped below 0
 *	the file must be destroyed.
 *	Global ttl's that are higher then local ttl's are stored into
 *	the local ttl's if the inode is unlocked or the local ttl
 *	is lower then or equal to the soft threshold.
 */

static void
check_age()
{
    register Inodenum	i;
    register Inode *	ip;
    register int	ttl ;
    register int	ttl_dif ;
    

    for (ip = Inode_table + 1, i = 1; i < Superblock.s_numinodes; ip++, i++)
    {
#ifndef USER_LEVEL
        /*
         * Because we might run for a long time we should give the
         * interrupt queue a chance to drain by rescheduling ourselves
         * after each 2048 inodes
          */
        if (!(i & 2047))
            threadswitch();
#endif
	ttl=GLOBAL_TTL(i) ;
	if ( ttl==L_UNKNOWN ) continue ;
	if ( ttl<=0 ) {
	    /* The inode has exhausted its lifetime, lets look at it */
            /*
             * Most inodes are free so we peak to see if it is free before
             * spending a lot of time locking it.  We do have to check that
             * is still in use after we get the lock though!
             */
            if (ip->i_flags&I_INUSE )
            {
                lock_inode(i, L_WRITE);
		/* If it is committed, ... */
                if ( (ip->i_flags&I_INUSE) )
                {
                    Stats.i_aged_files++;
                    destroy_inode(i,1,ANN_DELAY|ANN_ALL,SEND_CANWAIT);
                }
                unlock_inode(i);
            }
	    dbmessage(18,("Resetting ttl for %ld",i));
	    INIT_GLOBAL_TTL(i);
	} else {
	    /* We peek at the difference between local and global ttl */
	    ttl_dif=ttl-LOCAL_TTL(i);
	    if ( ttl_dif ) {
		if ( trylock_inode(i,0)==0 ) {
		    /* We have the lock, simply update the local ttl */
		    XS_LOCAL_LIFE(i)= XS_GLOBAL_LIFE(i) ;
		    unlock_inode(i);
		} else if ( LOCAL_TTL(i)<=SOFT_THRESHOLD ) {
		    /* We really need a lock now, to prevent local
		       ttl wraparound */
		    lock_inode(i,L_WRITE) ;
		    XS_LOCAL_LIFE(i)= XS_GLOBAL_LIFE(i) ;
		    unlock_inode(i);
		}
	    }
	}
    }

    Stats.std_age++; /* Only count successful completion of this! */
}

/*
 * GS_CHECKLIFE
 * An event came in that sets the lifetime of that inode to max
 * and restarts the thread waiting for that event.
 */

void
gs_checklife(hdr)
	header	*hdr ;
{
	dbmessage(25,("Checklife for %ld from member %d, event %d, my global ttl %d",
		hdr->h_offset,hdr->h_extra,hdr->h_size,GLOBAL_TTL(hdr->h_offset))) ;
	SET_GLOBAL_TTL(hdr->h_offset,MAX_LIFETIME) ;
	if ( hdr->h_extra==Superblock.s_member ) {
		/* It is one of ours */
		event_wake(map_event((int)hdr->h_size)) ;
	}
	dbmessage(25,("Checklife for %ld ends: %d",
		hdr->h_offset,GLOBAL_TTL(hdr->h_offset))) ;
}

/*
 * FORCE_TTL
 * Enforce that the global lifetime for an inode is not equal to unknown.
 * This happens when an inode is replicated before the ttl buffer is
 * flushed. Now do a flush_ttl. Earlier versions did a checklife. That was
 * not optimal.
 * We assume that the inode is locked.
 * Return values:
 *	STD_OK		- inode remained locked and lives
 *	STD_INTR	- inode was temporarely unlocked has to be rechecked
 */

void
gs_force_ttl(inode)
	Inodenum	inode;
{
	int		glob_ttl ;

	glob_ttl= GLOBAL_TTL(inode) ;
	if ( glob_ttl == L_UNKNOWN ) {
		gs_age_sweep();
	} 
	return ;
}

/* GS_AGE_EXIT.
   Stop caching of ages.
 */
void
gs_age_exit() {
	delay_threshold=0 ;
	flush_ttls();
}

/*
 * RESET_TTL
 * This routine updates the ttl (time to live) when the object is touched.
 * It only updates the local ttl. When the difference between local and
 * global ttl exceed the hard threshold, the routine unlocks the inode,
 * broadcasts the update to all members and waits until that the update
 * has been processed by its storage server. It then checks again.
 * If the global ttl is zero or lower the inode has been `aged' and
 * this routine assumes that it does not exist.
 * If the difference between local and global ttl is higher or equal
 * to the soft threshold, the routine fills a buffer with that information.
 * That buffer is broadcast when it threatens to overflow and at regular
 * intervals by the sweeper.
 * Return values:
 *	STD_OK		- inode remained locked and lives
 *	STD_INTR	- inode was temporarely unlocked has to be rechecked
 *	STD_CAPBAD	- inode was at end of life
 */

errstat
gs_reset_ttl(inode)
	Inodenum	inode;
{
	int		glob_ttl ;
	b_event		*p_event ;
	int		num_event ;

	SET_LOCAL_TTL(inode,MAX_LIFETIME) ;
	/* There is a race condition in computing the difference when
         * the base changes between to two accesses.
	 * They are done in such an order that the race will result
	 * in a value that is too high. Thus the server remains
	 * consistent at the cost of a little bit of extra work once in
	 * a while.
	 */
	glob_ttl= GLOBAL_TTL(inode) ;
	/* ttl can be unknow at startup, in that case check ....
	 * if unknown when functioning, the age has not reached the
	 * others yet.
	 */
	if ( ( glob_ttl== L_UNKNOWN &&
			!grp_member_present((int)Superblock.s_member) ) 
             || MAX_LIFETIME-glob_ttl>=delay_threshold ) {
		if ( glob_ttl<=0 ) {
			unlock_inode(inode) ;
			return STD_CAPBAD ;
		}
		/* Get an event to wait for the grp_broadcast
		 * to get back to you.
		 */
		num_event=get_event(&p_event) ;
		event_incr(p_event);
		unlock_inode(inode) ;
		send_check_ttl(inode,num_event) ;
		event_wait(p_event) ;
		dbmessage(13,("Woken up: eventnum %d",num_event));
		free_event(num_event) ;
		return STD_INTR ;
	} else if ( MAX_LIFETIME-glob_ttl>=SOFT_THRESHOLD ) {
		ttl_announce(inode,MAX_LIFETIME) ;
	}
	return STD_OK ;
}

/* dec_ttl_buffer
 * This is a hack. We did get a std_age. We should now decrease all
 * aging info. Including the information already in the buffer.
 * This routines asumes that the update_lock is already set.
 */
static void
dec_ttl_buffer()
{
	unsigned char	*p ; /* cache for p_ttl_updates */

	for (p=ttl_updates+sizeof(int32) ; 
	     p< p_ttl_updates ;
	     p+= sizeof(int32)+1 ) {
		/* We assume that this never decreases below 0 */
		if ( *p!=L_UNKNOWN) --*p ;
	}
}

/* ttl_announce
 * Fill a buffer with ttl update information.
 * The buffer is broadcast when it threatens to overflow and at regular
 * intervals by the sweeper.
 */

static void
ttl_announce(inode,ttl)
	Inodenum	inode;
	int		ttl;
{
	char	*p ; /* cache for p_ttl_updates */

	/* Fill the buffer with the information */
	mu_lock( &update_mutex ) ;
	for (;;) {
		p= buf_put_int32((char *)p_ttl_updates,
			END_UPDATES,(int32)inode) ;
		if ( !p || p+1>=END_UPDATES ) {
			/* Buffer overflow, send and retry */
			flush_ttls() ;
			continue ;
		}
		/* Enough room */
		*p++ = ttl ;
		p_ttl_updates= (unsigned char *)p ;
		break ;
	}
	mu_unlock( &update_mutex ) ;
	dbmessage(15,("Inode %ld in ttl update buffer",inode)) ;
}

/*
 * SET_TTL
 * This routine sets the ttl (time to live) when the object is created.
 * It updates the local ttl and broadcasts (with delay) the update to all
 * members the routine fills a buffer with that information.
 */

void
gs_set_ttl(inode)
	Inodenum	inode;
{
	SET_LOCAL_TTL(inode,MAX_LIFETIME) ;
	ttl_announce(inode,MAX_LIFETIME) ;
}

/*
 * CLEAR_TTL
 * This routine sets the ttl (time to live) to unknown. This is done when
 * the object is destroyed.
 */

void
gs_clear_ttl(inode)
	Inodenum	inode;
{
	dbmessage(25,("Clearing ttl for %ld",inode));
	INIT_LOCAL_TTL(inode) ; INIT_GLOBAL_TTL(inode) ;
	/* ttl_announce(inode,L_UNKNOWN) ; */
}

/* GS_MEMB_CRASH
 * A member crashed. Increase all ages > MAX_LIFETIME-HARD_THRESHOLD
 * to MAX_LIFETIME.
 */
void
gs_memb_crash() {
    register Inodenum	i;
    register int	ttl ;
    

    dbmessage(11,("Resetting ages>MAX-HARD_THRESHOLD"));
    for ( i = 1; i < Superblock.s_numinodes; i++)
    {
#ifndef USER_LEVEL
        /*
         * Because we might run for a long time we should give the
         * interrupt queue a chance to drain by rescheduling ourselves
         * after each 2048 inodes
          */
        if (!(i & 2047))
            threadswitch();
#endif
	ttl=GLOBAL_TTL(i) ;
	if ( ttl==L_UNKNOWN ) continue ;
	if ( ttl>MAX_LIFETIME-HARD_THRESHOLD ) {
	    SET_GLOBAL_TTL(i,MAX_LIFETIME) ;
	}
    }

    if ( refuse_age && refusing_member!= -1  ) {
	if ( !grp_member_present(refusing_member) ) {
	    dbmessage(9,("Crash of member stopping ages, enable ages"));
	    bs_allow_ages() ;
	}
    }
}

/* GS_AGE_SWEEP
 * Flush the ttl update buffer.
 */

void
gs_age_sweep() {
	mu_lock( &update_mutex ) ;
	flush_ttls();
	mu_unlock( &update_mutex ) ;
}

/* Flush the ttl update buffer, assume the buffer is locked */
static void
flush_ttls() {
	header	hdr ;

	/* Prepare the header */
	hdr.h_command= BS_SETLIFES;
	hdr.h_extra= Superblock.s_member;
	hdr.h_size= p_ttl_updates-ttl_updates ;

	if ( hdr.h_size==0 ) return;
	dbmessage(25,("broadcasting ttl update"));
	bs_grpreq(&hdr, (char *)ttl_updates, (int)hdr.h_size,SEND_CANWAIT) ;
	p_ttl_updates= ttl_updates ;
}

/* Send out a command to stop aging */
static void
stop_aging() 
{
	int		num_event ;
	b_event		*p_event ;
	header		hdr;

	mu_lock(&can_stop_age) ;
	num_event=get_event(&p_event) ;
	hdr.h_extra= Superblock.s_member;
	hdr.h_size= num_event ;
	hdr.h_offset= 0 ;
	hdr.h_command= BS_AGE_DELAY ;
	do {
		mu_lock(&age_info) ;
		while ( refuse_age ) {
			ages_waiting++ ;
			mu_unlock(&age_info) ;
			dbmessage(24,("Waiting for start_age to come through"));
			sema_down(&awaiting_accept);
			mu_lock(&age_info) ;
		}
		mu_unlock(&age_info);
		dbmessage(23,("Send stop age, eventnum %d", num_event)) ;
		event_incr(p_event);
		bs_grpreq(&hdr, (bufptr)0, 0, SEND_CANWAIT) ;
		event_wait(p_event) ;
		dbmessage(23,("Woken up: eventnum %d",num_event));
	} while ( stop_age_refused ) ;
	free_event(num_event) ;
	mu_unlock(&can_stop_age);
}

/* Send out a command to stop/restart aging */
static void
restart_aging()
{
	header		hdr;
	b_event		*p_event ;
	int		num_event ;

	num_event=get_event(&p_event) ;
	hdr.h_extra= Superblock.s_member;
	hdr.h_size= num_event ;
	hdr.h_offset= 1 ;
	hdr.h_command= BS_AGE_DELAY ;
	dbmessage(3,("Send restart aging, eventnum %d", num_event)) ;
	event_incr(p_event);
	bs_grpreq(&hdr, (bufptr)0, 0, SEND_CANWAIT) ;
	event_wait(p_event) ;
	dbmessage(3,("Woken up age delay: eventnum %d",num_event));
	free_event(num_event) ;
}

/*
 * GS_DO_STOP_AGE
 *
 * Refuse all further ages, until enough restarts come in.
 */
void
gs_delay_age(hdr)
header *	hdr;
{
    /* Do not always lock age_info, the only thread changing refuse_age
     * is the thread calling us.
     */

    dbmessage(25,("Stop age %d from member %d, event %d",
		hdr->h_offset,hdr->h_extra,hdr->h_size)) ;
    if ( hdr->h_offset==0 ) {
	if ( hdr->h_extra==Superblock.s_member ) {
		stop_age_refused= refuse_age ;
	}
	if ( !refuse_age ) {
	    /* About to change the value, lock it. */
	    mu_lock(&age_info) ;
	    refuse_age=1 ;
	    refusing_member= hdr->h_extra ;
	    mu_unlock(&age_info) ;
	}
    } else {
	bs_allow_ages();
    }

    /* Wake up your slave .... */
    if ( hdr->h_extra==Superblock.s_member ) {
	/* It is one of ours */
	/* If age were blocked, signal that to receiver.
	   We can use a global variable here, because we have only one
	   age running at any one moment.
	 */
	age_refused= refuse_age ;
	event_wake(map_event((int)hdr->h_size)) ;
    }

}

void
bs_allow_ages() {
	/* Only one of these can come in. */
	if ( !refuse_age ) bwarn("Start aging inconsistency");
	mu_lock(&age_info) ;
	refuse_age=0 ;
	sema_mup(&awaiting_accept,ages_waiting) ;
	ages_waiting=0 ;
	mu_unlock(&age_info) ;
}

/* Send out all lifetimes known to you */
void
gs_send_ttls()
{
	char	*p ; /* cache for p_ttl_updates */
	Inodenum	i;
	int		glob_ttl ;

	dbmessage(25,("Broadcasting all aging info"));
	stop_aging() ;
	mu_lock( &update_mutex ) ;
        for (i = 1; i < Superblock.s_numinodes; i++)
        {
	    glob_ttl= GLOBAL_TTL(i) ;
	    if ( glob_ttl==L_UNKNOWN ) continue ;
	    /* We could call ttl_announce here, but consider this an
	     * inline expansion.
	     */
	    for (;;) {
	    	p= buf_put_int32((char *)p_ttl_updates,
				END_UPDATES,(int32)i) ;
	    	if ( !p || p+1>=END_UPDATES ) {
		    /* Buffer overflow, send and retry */
		    flush_ttls() ;
		    threadswitch();
		    continue ;
	        }
	        /* Enough room */
	        *p++ = glob_ttl ;
	        p_ttl_updates= (unsigned char *)p ;
		break;
	    }
        }
	flush_ttls();
	mu_unlock( &update_mutex ) ;
	restart_aging() ;
	return ;
}

/* Send a check_ttl to all parties concerned */
static void
send_check_ttl(inode,eventnum)
	Inodenum	inode;
	int		eventnum;
{
	header	hdr ;

	/* Prepare the header */
	hdr.h_command= BS_CHECKLIFE;
	hdr.h_extra= Superblock.s_member;
	hdr.h_size= eventnum ;
	hdr.h_offset= inode ;

	dbmessage(13,("Send check_ttl for %ld, eventnum %d",
			inode, eventnum)) ;
	bs_grpreq(&hdr, (bufptr)0, 0, SEND_CANWAIT) ;
}

/* GS_SETLIFES
 * A buffer came in with new lifetimes, update the global lifetimes.
 */

void
gs_setlifes(hdr,buf,size)
	header	*hdr ;
	char	*buf ;
	int	size ;
{
	Inodenum		inode ;
	register unsigned char	ttl ;
	register unsigned char	my_ttl ;
	register char		*p_buf, *e_buf ;

	p_buf= buf ; e_buf = buf+size ;

	dbmessage(25,("Received ttl buffer from %d",hdr->h_extra));
	while ( p_buf<e_buf ) {
		p_buf= buf_get_int32(p_buf, e_buf, &inode ) ;
		if ( !p_buf || p_buf>=e_buf ) {
			bwarn("Inconsistent setlifes buffer received") ;
			return ;
		}
		ttl= *p_buf++ ;
		my_ttl= GLOBAL_TTL(inode);
		dbmessage(35,("Setlife %ld to %d from %d",inode,ttl,my_ttl));
		if ( my_ttl==L_UNKNOWN || my_ttl<ttl ) {
			SET_GLOBAL_TTL(inode,ttl) ;
		}
		dbmessage(25,("Setlife ends at %ld",GLOBAL_TTL(inode)));
	}
}
