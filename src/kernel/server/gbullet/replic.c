/*	@(#)replic.c	1.1	96/02/27 14:07:36 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * GRP_BULLET.C
 *
 *	This file contains the code used to replicate files.
 *
 * Author: Ed Keizer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "amoeba.h"
#include "group.h"
#include "cmdreg.h"
#include "stderr.h"
#ifdef USER_LEVEL
#include <thread.h>
#else
#include <sys/proto.h>
#include "sys/kthread.h"
#endif
#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/prv.h"
#include "module/buffers.h"
#include "module/mutex.h"
#include "byteorder.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_defs.h"
#include "grp_comm.h"
#include "grp_bullet.h"
#include "event.h"
#include "alloc.h"
#include "assert.h"
#include "stats.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

/* Size of info buffer used in replication */
#define	INFOBUF_SIZE	(sizeof(objnum)+3*sizeof(int32)+sizeof(port))


/* Return values for select_servers */
#define	REP_NOSPACE	-2	/* Enough servers, but no space */
#define	REP_NOSERVERS	-1	/* Not enough servers, or server failure */
#define REP_DONE	0	/* Replication OK */
#define REP_WORK	1	/* More work to do */

extern  Superblk Superblock;     /* in-core copy of superblock */
extern	Inode *		Inode_table;
extern	peer_bits *	Inode_reptab;
extern	b_fsize		Blksizemin1;
extern	Statistics	Stats;
extern  Inodenum        bs_local_inodes_free;


/* This offers:
	gs_startrep(cp)
		Replicate an Inode now!
	gs_replicate(header,buffer,size)
		Get a copy of the inode.
	bs_do_repchk()
		Do a replication check
	gs_sweeprep()
		Do a `sweeper' replication check
	gs_lostrep()
		Cache admin lost a file to be replicated.
	gs_flagnew(vec)
		New member(s).
*/

/* Inode replication can be done in several ways:
   1. Sending the group a replicate request, indicating which servers
      should make a replica. Leave it up to the servers to acquire the
      contents from the owner.
   2. Send the contents of the file to the group, indicating which servers
      should make a replica.
   3. Send the contents of the file to each server you think needs a
      replica.
   4. Send each server a request to make a replica.

   A disadvantage of method 1 and 2 is that the owner has to get
   acknowledgments from the replica server before in can return
   from the replication routine.
   What to do when one has waited for such an acknowledgement for
   too long is difficult. Retry?
   A disadvantage of method 2 and 3 is that for large files the receiving
   server has to keep internal state to be able to find out whether it
   has received the whole file.
   A rough estimate of the number of messages needed for each method
   follows, R is the number of replica's, X the number of 8K chunks in the
   file, N the number of servers. (RPC's can do 32K chunks, grp_send
   8K chunks.)

	1.	2 + 2*R + 2*X*R/4
	2.	2 + 2*R + 2*X
	3.	2*R*(X/4)
	4.	2*R + 2*R*(X/4)

   Method 1 has less messages method 2 when there less then four replica's.
   Method 3 always has less messages than method 4.
   Method 3 has less messages than method 1 when the number of replica's
   is bigger than four.

   We have chosen a combination of 3 and 4. The message to the recipient
   contains the first chunk of the file. If the file is longer the recipient
   fetches it, before reurning from the initial RPC.
 */

/* Definition of enum describing replication status kept during
 * replication of a file.
 */

typedef enum {
	RS_BLANK,	/* Nothing known */
	RS_NOT_OK,	/* Not available */
	RS_DONE,	/* Has a copy */
	RS_NOSPACE,	/* Not enough room */
	RS_TODO,	/* Working at it */
	RS_NOTNOW	/* We prefer others */
} rs_status ;

/* Exclusive lock for replication */
static mutex	Single_rep ;

/* Administration for replication check */
static mutex	Admin_mutex ;
static int	nr_servers_never_seen ;
static int	need_sweep ;

/* Forward definitions */

static	int	select_servers() ;
static	errstat	repl_file() ;
static	errstat	docheck() ;
static	errstat	single_rep() ;
static	int	passives_seen() ;

void
gs_rep_init() {
	nr_servers_never_seen= S_MAXMEMBER  ;
}

errstat
gs_startrep(cp)
	Cache_ent *	cp;
{
	int	dummy ;

	return single_rep(cp,&dummy);
}

/* Replicate an inode.
 * The routine assumes that the file is locked upon entrance,
 * but can unlock the file during its operation.
 * The routine returns with a locked inode.
 * cp remains valid for as long as REPLICATE_BUSY is in the flags.
 * Unless the inode is destroyed, we have to check for that after
 * locking the file.
 */
static errstat
single_rep(cp,repeat_signal)
	Cache_ent *	cp;	/* in: cache entry to be replicated */
	int		*repeat_signal ;
{
	Inodenum	inode ;
	int		num_replicas ;
	rs_status	repservers[S_MAXMEMBER] ;
	int		i ;
	b_fsize		blocks_needed ;
	int		failures ;
	int		file_locked ;
	peer_bits	*inode_repl ;
	peer_bits	replicas ;
	errstat		err ;
	errstat		last_err ;	/* To be returned */
	int		copy_owner;
	Superblk	CopySuper;

	super_lock();
	CopySuper=Superblock;
	super_free();

	num_replicas= CopySuper.s_def_repl ;

	/* Check the incore BUSY flag. If it is set, this is a fresh
	 * replication. Thus we may assume that any servers never
	 * seen do not have a replica.
	 * If the flag is not set, this is from docheck, thus any missing
	 * servers are assumed to have a copy.
	 */
	if ( !(cp->c_inode.i_flags&I_BUSY) ) {
		mu_lock(&Admin_mutex) ;
		num_replicas -= nr_servers_never_seen ;
		mu_unlock(&Admin_mutex) ;
		if ( num_replicas<=0 ) return STD_NOTNOW ;
	}

	inode= cp->c_innum ;

	dbmessage(15,("replicating %ld, %d replicas needed",
				inode,num_replicas));
	/* We set the replicating busy flag here. The caller should have
	 * made sure it is not set yet.
	 */
	assert( !(cp->c_flags&REPLICATE_BUSY) );
	cp->c_flags|= REPLICATE_BUSY ;
        /* Initialize statuses */
	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
	    repservers[i]=
	      (CopySuper.s_memstatus[i].ms_status==MS_ACTIVE) ?
		RS_BLANK : RS_NOT_OK ;
	}
	file_locked=1 ;

	/* Take note of the replica's already present. */
	copy_owner= -1 ;
	inode_repl= Inode_reptab + inode ;
	replicas= *inode_repl ;
	for ( i=0 ; replicas ; replicas>>= 1, i++ ) {
		if ( replicas&1 ) {
			/* We have a replica */
			if ( repservers[i]==RS_BLANK) repservers[i]=RS_DONE ;
			if ( copy_owner<0 ) copy_owner=i ;
		}
	}
	if ( repservers[CopySuper.s_member]==RS_DONE ) {
		copy_owner=CopySuper.s_member ;
	}
	if ( copy_owner<0 ) {
	    dbmessage(4,("Could not find replica of inode %ld",inode));
	    return STD_NOTNOW ;
	}

	blocks_needed = NUMBLKS(cp->c_inode.i_size) ;

	last_err= STD_NOTNOW ;
	for (;;) {
	    /* Keep at it until satisfied */
	    i= select_servers(repservers,num_replicas,blocks_needed) ;
	    dbmessage(29,("select_servers for %ld returns %d",inode,i)) ;
	    /* The following is a should not happen that I am not to
	     * sure about. It ensures that once we start replicating the
	     * global ttl is set.
	     */
	    if ( !file_locked ) {
		lock_inode(inode,L_READ);
		file_locked=1 ;
		/* Inode could have been destroyed by now */
		if ( cp->c_innum!=inode || !(cp->c_flags&REPLICATE_BUSY) ) {
		    return STD_NOTFOUND ;
		}
	    }
	    switch ( i ){
	    case REP_NOSPACE :
		/* Enough servers, but out of space.. */
		cp->c_flags &= ~REPLICATE_BUSY ;
		*repeat_signal=1 ;
		return STD_NOSPACE ;
	    case REP_NOSERVERS : /* Can not find enough servers, fail ... */
		cp->c_flags &= ~REPLICATE_BUSY ;
		*repeat_signal=1 ;
		return last_err ;
	    case REP_DONE :  /* It has all been done */
		cp->c_flags &= ~REPLICATE_BUSY ;
		return STD_OK ;
	    case REP_WORK : /* There is work to be done, do it */
		break ;
	    }
	    gs_force_ttl(inode) ;
	    failures=0 ;
	    for ( i=0 ; i<S_MAXMEMBER ; i++ ) if ( repservers[i]==RS_TODO ) {
		/* Send contents to peer */
		err= repl_file(cp,i,copy_owner,&file_locked);
		/* Check whether we still have the file .. */
		if ( !file_locked ) {
		    lock_inode(inode,L_WRITE);
		    file_locked=1 ;
		    /* Inode could have been destroyed by now */
		    if ( cp->c_innum!=inode || !(cp->c_flags&REPLICATE_BUSY) ) {
			return STD_NOTFOUND ;
		    }
		}
		switch( err ) {
		case STD_OK :
		case STD_EXISTS :
			*inode_repl |= ((peer_bits)1<<i) ;
			repservers[i]=RS_DONE ;
			break ;
		case STD_NOTFOUND :
			cp->c_flags &= ~REPLICATE_BUSY ;
			return STD_NOTFOUND ;
		case STD_NOSPACE:
			repservers[i]=RS_NOSPACE;
			last_err=STD_NOSPACE;
			failures++ ;
			break ;
		default:
			last_err= err;
			repservers[i]=RS_NOT_OK ;
			failures++ ;
			break ;
		}
	    }
	    if ( failures==0 ) {
		cp->c_flags &= ~REPLICATE_BUSY ;
		return STD_OK ;
	    }
	}
	/* NOTREACHED */
}

/* Select the servers on which to replicate.
 * returns -1 on failure (nospace), 0 on more work, 1 on no more work.
 * The principal method is round-robin. Each servers attempts the next
 * highest. If this fails for some reason, it tackles the next ....
 * The first time around, the algorithm tries to avoid servers with
 * less then 100 blocks or 150% of the file size. The second time around
 * only the file size is the limit.
 * This routine is designed to be called again with an array with updated
 * status information.
 */

static int
select_servers(repservers,num_replicas,size) 
	rs_status	repservers[] ;
	int	num_replicas ;
	b_fsize	size ;

{
	int	serverno ;
	int	i ;
	int	done_count=0 ;
	int	blank_count=0 ;
	int	notnow_count=0 ;
	int	nospace_count=0 ; /* Useful for error return */
	int	todo_count ;
	int	svr_limit ;
	b_fsize	blocks_free ;

	/* First time around, count ... */
	for ( i=0, serverno=Superblock.s_member ;
	   i<S_MAXMEMBER ; i++, serverno++ ) {
	    	serverno %= S_MAXMEMBER ;
		switch ( repservers[serverno] ) {
		case RS_DONE: done_count++ ; break ;
		case RS_BLANK: blank_count++ ; break ;
		case RS_NOTNOW: notnow_count++ ; break ;
		case RS_NOSPACE: nospace_count++ ; break ;
		case RS_TODO:	 assert(RS_TODO) ;
		}
	}
	if ( done_count>=num_replicas ) return REP_DONE ;

	/* Even if there are not enough servers,
	 * try to make as many replicas a you can ....
	 */

	if ( blank_count==0 ) {
		/* We are out of eligible servers, try the ones we
		 * skipped first time.
		 */
		if ( notnow_count==0 ) {
			/* Really out of servers */
			dbmessage(99,("really out of servers"));
			return nospace_count?REP_NOSPACE:REP_NOSERVERS ;
		}
	}

	/* Calculate the number of servers we want to catch */
	svr_limit= num_replicas - done_count ;
	todo_count=0 ;

	/* Count the number of notnow's */
	notnow_count=0 ;
	for ( i=0, serverno=Superblock.s_member ;
	   i<S_MAXMEMBER ; i++, serverno++ ) {
	    	serverno %= S_MAXMEMBER ;
		switch ( repservers[serverno] ) {
		case RS_NOSPACE:
		case RS_NOT_OK:
		case RS_DONE: continue ;
		case RS_NOTNOW:
			if ( blank_count!=0 ) {
				notnow_count++ ;
				continue ;
			}
			break ;
		case RS_BLANK:
			if ( !grp_member_present(serverno) ) {
				repservers[serverno]=RS_NOT_OK ;
				continue ;
			}
			/* Ok, it exists, look at # free blocks */
			blocks_free=bs_grp_bl_free(serverno) ;
			if ( blocks_free<size ) {
				/* The server does not have enough */
				repservers[serverno]=RS_NOSPACE ;
				continue ;
			}
			if ( blocks_free<BS_REP_AVOID ||
			     2*blocks_free<3*size ) {
				/* We'd like to leave this one alone */
				repservers[serverno]=RS_NOTNOW ;
				notnow_count++ ;
				continue ;
			}
			/* Ok, use this one ... */
			break ;
		}
		/* We only get here if we can use the server */
		repservers[serverno]= RS_TODO ;
		todo_count++ ;
		if ( todo_count>=svr_limit ) return REP_WORK ;
	}
	/* We only get here when there were not enough servers available */
	if ( todo_count==0 ) {
		/* Nothing to do, check whether there are NOTNOWS left */
		if ( notnow_count ) {
			/* Go recursive, blank_count is zero now */
			return select_servers(repservers,num_replicas,size);
		}
		/* No chance at anything any more, forget it */
		dbmessage(99,("out of chance, no servers"));
		return nospace_count?REP_NOSPACE:REP_NOSERVERS ;
	}
	return 0 ;
}

/* Copy the contents of a file to a peer server */
/* Unlock the file if we expect requests back */

static errstat
repl_file(cp,peer,copy_owner,file_locked_p)
	Cache_ent *	cp;	/* in: cache entry to be replicated */
	int		peer;
	int		copy_owner;
	int		*file_locked_p ;
{
	Inodenum	inode ;
	header		hdr;
	bufsize		n;
	errstat		err;
	int		failures;
	char		*buf ;
	char		infobuf[INFOBUF_SIZE] ;
	char		*p_info ; /* Point just after last address */
	int		one_go ; /* Flag */
	int32		file_size ;

     
	inode= cp->c_innum ;
	dbmessage(22,("Attempt to replicate %ld on %d",inode,peer));
	assert(*file_locked_p);

	hdr.h_command= BS_REPLICATE ;
	/* Send it to the peer's port */
	super_lock();
	hdr.h_port = Superblock.s_memstatus[peer].ms_cap.cap_port ;
	hdr.h_priv = Superblock.s_memstatus[peer].ms_cap.cap_priv;
	super_free();

	file_size= cp->c_inode.i_size ;

	/* Fill the information buffer */
	/* It contains
		4 byte		sending member
		4 byte?		inode number
		4 byte		file size
		portsize	random number
		4 byte		version number
	*/
	p_info= infobuf ;
	p_info= buf_put_int32(p_info,
			infobuf + sizeof(infobuf),
			(int32)copy_owner) ;
	p_info= buf_put_objnum(p_info,
			infobuf + sizeof(infobuf),
			inode) ;
	p_info= buf_put_int32(p_info,
			infobuf + sizeof(infobuf),
			file_size) ;
	p_info= buf_put_port(p_info,
			infobuf + sizeof(infobuf),
			&cp->c_inode.i_random );
	p_info= buf_put_int32(p_info,
			infobuf + sizeof(infobuf),
			(int32)cp->c_inode.i_version );

	if ( !p_info ) bpanic("infobuffer in repl_file too small");

	/* Put the address into a separate variable. There is a small
	 * chance that the inode will get destroyed during this process.
	 * In that case the trans can still finish, but will send garbage
	 * file contents. This is not a problem, since the file will
	 * get destroyed anyhow.
	 * There is still a small race. If the owner destroys the file and
	 * crashes before it can announce this to the peers, the peers
	 * will have garbage data until the owner comes back up.
	 * Hmm, this is a security hole too.
	 * Implementing separate read and write locks could solve this
	 * problem.
	 */
	/* If there is room to tack the info to the end of the
	 * file buffer and send it all in one go do it.
	 * Otherwise just send the information and let the other side
	 * get it all.
	 * One trick: zero sized files have one_go set to 0. The other
	 * side notices that there is nothing to get.
	 * Besides, this is only used for 0 sized files with SAFETY.
	 */
	one_go=0 ;
	if ( cp->c_addr && file_size>0 && file_size+sizeof(infobuf)<= BS_REQBUFSZ ) {
		/* Check whether info fits at end of buffer */
		if ( file_size+sizeof(infobuf)>cp->c_bufsize) {
			err=growbuf(cp,file_size+sizeof(infobuf));
			dbmessage(7,("did not fit, growbuf returned %d",err));
			if ( err==STD_OK ) {
				one_go=1 ;
			}
		} else {
			one_go=1 ;
		}
	}
	dbmessage(17,("onego = %d",one_go));
	if ( one_go ) {
		buf= cp->c_addr ;
		p_info= buf+file_size ;
		hdr.h_size = file_size+sizeof(infobuf);
		memcpy((_VOIDSTAR)p_info,(_VOIDSTAR)infobuf,sizeof(infobuf)) ;
	} else {
		p_info= buf= infobuf ;
		hdr.h_size = sizeof(infobuf);
		/* We possibly expect request back for the rest of the file,
		 * unlock the inode.
		 */
		unlock_inode(inode) ;
		*file_locked_p=0 ;
	}

	failures=0 ;
	for(;;) {
		/* repeat until we lose RPC_FAILURE */
		n = trans(&hdr, buf, hdr.h_size, &hdr, NILBUF, 0);
		dbmessage(17,("gs_replicate trans returns %d, status %d",n,hdr.h_status)) ;
		if (ERR_STATUS(n)) {
			err= ERR_CONVERT(n) ;
			if ( err==RPC_FAILURE && failures++<BS_RPC_RETRY) {
				continue ;
			}
			break ;
		}
	    	err= ERR_CONVERT(hdr.h_status);
		break;
	}
	return err ;
}

/* The routine called by the owner to place a replica on this server
 * This message can come in two formats:
 *	1. The contents of the whole file followed by inode information,
 *	2. Only inode information.
 */

errstat
gs_replicate(hdr, buf, size)
header *	hdr;
bufptr		buf;	/* file data */
b_fsize		size;	/* #bytes of data in buf */
{
	register Cache_ent *	cp;
	errstat			err;
	Inodenum		inode;
    	register Inode *	ip;	/* pointer to the inode */
	char		*p_info ; /* Point just after last address */
	objnum		object_number ;
	int32		file_size ;
	int32		sending_member ;
	port		remote_random ;
	b_fsize		buffer_size ;
	uint32		remote_version ;
	uint32		current_version ;
	int		Changed ;

    	if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER)) != STD_OK)
		return err;

	Stats.b_replicate++;
	assert(size <= BS_REQBUFSZ);

	/* get inode information */
	if ( size<INFOBUF_SIZE ) return STD_ARGBAD ;
	buffer_size= size-INFOBUF_SIZE ;

	p_info= buf + size - INFOBUF_SIZE ;
	
	p_info= buf_get_int32(p_info,
			buf+size,
			&sending_member) ;
	p_info= buf_get_objnum(p_info,
			buf+size,
			&object_number) ;
	p_info= buf_get_int32(p_info,
			buf+size,
			&file_size) ;
	p_info= buf_get_port(p_info,
			buf+size,
			&remote_random );
	p_info= buf_get_int32(p_info,
			buf+size,
			(long *)&remote_version) ;

	if ( !p_info ) return STD_ARGBAD;

	inode= object_number ;

	dbmessage(11,("Replicating %ld, size %ld, bufsize %ld from %ld",
		inode, file_size, buffer_size, sending_member));
	
	/* Check the information */
	/* First member presence */
	if ( sending_member<0 || sending_member>=S_MAXMEMBER ) {
		return STD_ARGBAD ;
	}
	if ( !grp_member_present((int)sending_member) ) {
		dbmessage(2,("Receiving gs_rep from member that is not present"));
		return STD_SYSERR ;
	}

	/* Then inode information */
	if (inode <= 0 || inode >= Superblock.s_numinodes) {
		return STD_ARGBAD;
	}

	lock_inode(inode, L_WRITE);
	Changed=0 ;

	err= STD_OK ;
	ip = Inode_table + inode;
	if ( ip->i_owner==Superblock.s_member ) {
	    /* The inode is ours and we know nothing about it */
		unlock_inode(inode) ;
		return STD_CAPBAD ;
	}
	current_version=ip->i_version ;
	DEC_VERSION_BE(&current_version);
	if ( current_version!=remote_version ) {
	    int result;
	    /* Actions to take when versions don't match */
	    result= bs_version_cmp(current_version,remote_version) ;
	    dbmessage(26,("version_cmp in replicate for %ld returns %d",inode,result));
	    switch (result) {
		/* The other is owner, scream if my version seems better,
		 * ignore everything else.
		 */
	    case VERS_INCOMP:
		bwarn("Owner of %ld has strange version number",inode);
	    case VERS_TWO:
		break ;
	    case VERS_ONE:
		dbmessage(11,("Have better version for %ld than other(%d)",
						inode,sending_member));
		break ;
	    }
	    if (ip->i_flags & I_INUSE) {
		    dbmessage(15,("getting rid of contents of %ld",inode));
		    /* We have something here, get rid of it */
		    destroy_inode(inode,1,ANN_NEVER,SEND_CANWAIT) ;
		    /* This might upgrade the version number, but
		     * we will overwrite that later.
		     */
	    }
    
	    /* We will change the inode, so lock its ino cache block */
	    lock_ino_cache(inode) ;
	    /* We now have a free inode, update version number last and
	     * leave rest of processing to calling routine.
	     */
	    ip->i_version= remote_version ;
	    ENC_VERSION_BE(&ip->i_version);
	    ip->i_flags= 0;
	    unlock_ino_cache(inode) ;
	}
	if ( ip->i_flags & I_DESTROY ) {
	    err= STD_CAPBAD ;
	} else
	if ( ip->i_flags & I_ALLOCED ) {
		if ( ip->i_flags&I_INUSE ) {
			/* Inode exists, check random # */
			if ( PORTCMP(&ip->i_random,&remote_random) ) {
				/* Random # ok, check presence of data */
				if ( ip->i_flags&I_PRESENT ) {
					err= STD_EXISTS ;
				}
			} else {
				err= STD_CAPBAD ;
			}
		} else err= STD_CAPBAD ;
	}
	if ( err!= STD_OK ) {
		unlock_inode(inode) ;
		return err ;
	}

	/* Ok, we can fill in the inode now */
	if ( !(ip->i_flags&I_INUSE) ) {
		Changed=1 ;
		lock_ino_cache(inode) ;
		ip->i_size = file_size;
		ip->i_random = remote_random;
		ENC_FILE_SIZE_BE(&ip->i_size);
		ip->i_flags = I_INUSE|I_ALLOCED ;
		/* Do not set reptab, sender may have cache copy only */
    	}

	/* Nothing to do for 0 sized files */
	if ( file_size == 0 ) {
		/* Empty files are always present */
		ip->i_flags |= I_PRESENT ;
		Inode_reptab[inode] |= (1<<Superblock.s_member) ;
		if ( Changed ) writeinode(ip,ANN_ALL,SEND_CANWAIT);
		unlock_inode(inode);
		return STD_OK;
	} else {
		if ( Changed ) unlock_ino_cache(inode) ;
	}
	/* We just unlocked the inode cache. If an error develops
	 * somehow we do not force the inode to disk. It is not
	 * necessary and would cost extra code.
	 */
	
	/* Try to get cache memory */
	if ((cp = alloc_cache_slot(inode)) == 0) {
		unlock_inode(inode);
		return STD_NOSPACE;
	}
	/*
	 * Get memory in the cache for the replica.
	 */
	if (alloc_cache_mem(cp, (b_fsize) file_size) != STD_OK)
	{
		unlock_inode(inode);
		return STD_NOSPACE;
	}

	/* Move the data to the newly allocated cache buffer */
	if (buffer_size ) (void) memmove((_VOIDSTAR) cp->c_addr,
					(_VOIDSTAR) buf,
					(size_t) buffer_size);

	if ( buffer_size!=file_size ) {
		/* We did not get all of the file, get the rest */
		capability	file_cap ;
		b_fsize		dummy ;
		dbmessage(31,("Reading from member %d",sending_member));
	
		/* Build the capability */
		super_lock();
		file_cap.cap_port= Superblock.s_memstatus[sending_member].ms_cap.cap_port ;
		super_free();
		if (prv_encode(&file_cap.cap_priv, inode, BS_RGT_ALL, &ip->i_random) < 0)
		{
			destroy_cache_entry(cp) ;
			unlock_inode(inode) ;
	    		return STD_SYSERR;
		}
		/* Read the file */
		err= b_read(&file_cap,
			(b_fsize)buffer_size,
			(char *)cp->c_addr + buffer_size,
			file_size-buffer_size,
			&dummy);
		if ( err!=STD_OK ) {
			destroy_cache_entry(cp) ;
			unlock_inode(inode) ;
			return err ;
		}
	}

	/* Ahhh, we are finally there.
	 * Always wait for the disk writes to finish before returning.
	 * If the replica was for a file with safety on this is essential.
	 * Files without safety are replicated in the background and
	 * a little wait there is not essential.
	 */
	err= f_todisk(cp,1);
	if ( err!=STD_OK ) {
	    destroy_cache_entry(cp) ;
	}
	dbmessage(17,("Finished replicating, ip_size %lx, cp_size %lx",
		ip->i_size, cp->c_inode.i_size));
	unlock_inode(inode);
	bs_publish_inodes(inode,(Inodenum)1,SEND_CANWAIT);
	return err;
}

#if L_READ!=L_WRITE
/* Upgrade a lock from READ to WRITE */
/* We have to release the lock and then re-acquire it.
 * Others might get in between and change the file....
 * returns 0 for success, negative for failure
 */
static int
upgrade_inode_lock(inode,ip)
	Inodenum	inode ;
	Inode		*ip;
{
	uint32	read_version ;

	read_version=ip->i_version ;
	unlock_inode(i);
	lock_inode(i,L_WRITE);
	if( read_version!=ip->i_version  ){
		/* version changed, file changed, forget it... */
		unlock_inode(i) ;
		return -1 ;
	}
	return 0 ;
}
#endif

/* TRY_REPL.
 * Try replication of a file from check_rep.
 */
static errstat
try_repl(inode,ip,repeat_signal)
	Inodenum	inode;
	Inode		*ip;
	int		*repeat_signal;
{
    register Cache_ent *cp;	/* pointer to cache entry in linked list */

    /* See if the inode is in the cache */
    cp = find_cache_entry(inode);

    if (cp == 0)
    {
	/* Not in cache. 
	 * Add new entry in hash table at front of linked list.
	 */
	if ((cp = alloc_cache_slot(inode)) == 0) {
	    *repeat_signal=1 ;
	    return STD_NOSPACE;
	}
    } else {
	/* Check whether already replicating. If so, act as if
	 * we have success.
	 * Because there is always only one docheck running, we can
	 * assume that if REPLICATE is off, REPLICATE_BUSY is too.
	 */
	if (cp->c_flags & REPLICATE ) return STD_OK ;
    }
    return single_rep(cp,repeat_signal) ;
}

/* Find next highest activated member, starting from a given member */
static unsigned char
next_active(member)
	unsigned char member ;
{
	int		next_member ;
	uint16		memstatus ;
	int		i ;

	next_member= member ;
	super_lock();
	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
		/* We try to take the next highest */
		next_member= next_member+1;
		if ( next_member>=S_MAXMEMBER ) next_member=0 ;
		memstatus= Superblock.s_memstatus[next_member].ms_status ;
		if ( memstatus==MS_ACTIVE ) {
			super_free();
			return next_member ;
		}
	}
	super_free();
	/* Fall through, we do not know anything */
	return member ;	
}

/* Check replication.
 * First check ownership. If you find an inode that is not owned by anybody
 * and for which you are the next highest active member, grab it.
 * We assume that the reptab is accurate.
 * simply count the number of missing members and make the appropiate number
 * of replicas.
 * Do this only for inodes belonging to you.
 * Wait_flag is set if we want to wait for the lock coming free.
 * *repeat_signal is set to 1 if it might be usefull to repeat the
 * same action, later on, even if the number of avaiable members does not
 * change. Set to 0 otherwise.
 */
static errstat
docheck(wait_flag,repeat_signal)
	int 	wait_flag;
	int	*repeat_signal;
{
    register 	Inodenum	i;
    register 	Inode *	ip;
    errstat  	err ;
    int	     	n_replicas ;
    peer_bits	rep_bits ;	
    peer_bits	tmpmask ;	
    peer_bits	my_mask ;
    peer_bits	active_mask ;
    peer_bits	active_bits ;
    int		n_active_replicas ;
    unsigned char	owner ;
    unsigned char	me ;
    int		nr_done_since_switch ;
    int		cache_never_seen ;
    Superblk	CopySuper ;
    int		his_status ;

    *repeat_signal=0 ; nr_done_since_switch=0 ;

    super_lock();
    me= Superblock.s_member ;
    CopySuper=Superblock;
    super_free();
    his_status=CopySuper.s_memstatus[me].ms_status;
    if ( !(his_status==MS_ACTIVE || his_status==MS_PASSIVE) ) {
	*repeat_signal=1 ;
	return STD_DENIED;
    }

    /* Is is not useful to have more than one of these running at
     * any moment.
     */

    if ( wait_flag ) {
	mu_lock(&Single_rep) ;
    } else if ( mu_trylock(&Single_rep,(interval)0)<0 ) {
	/* Mutex failed, return to caller */
	*repeat_signal=1;
	return STD_NOTNOW ;
    }

    my_mask= 1<<me ;

    mu_lock(&Admin_mutex) ;
    cache_never_seen=nr_servers_never_seen ;
    mu_unlock(&Admin_mutex) ;

    /* The active_mask contains all activated members, this is used
     * to throw passive and dying members from the reptab entries.
     *
     */
    active_mask=0 ;
    for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
	if ( CopySuper.s_memstatus[i].ms_status==MS_ACTIVE ) {
	    active_mask |= (1<<i) ;
	}
    }

    err=STD_OK ;
    /* Start the real work */
    for (i = 1, ip = Inode_table + 1; i < CopySuper.s_numinodes; i++, ip++)
    {	
	if ( nr_done_since_switch++>500 ) {
	    nr_done_since_switch=0 ;
	    threadswitch();
	}
	dbmessage(98,("Checking inode %ld",i));
	/* If we are all there, have a peek at ownership */
	owner=ip->i_owner ;
	if ( cache_never_seen==0 && owner!=S_UNKNOWN_ID &&
	     (owner>=S_MAXMEMBER ||
	      CopySuper.s_memstatus[owner].ms_status<MS_ACTIVE) ) {
	    /* Something is wrong, check again */
	    dbmessage(98,("Strange owner %d for %ld",owner,i));
	    lock_inode(i,L_WRITE) ;
	    owner=ip->i_owner ;
	    if ( owner>=S_MAXMEMBER ||
	         CopySuper.s_memstatus[owner].ms_status<MS_ACTIVE ) {
	        /* Something is definitely wrong, change owner? */
		dbmessage(98,("Stranger owner %d for %ld",owner,i));
	    	if ( next_active(ip->i_owner)== me ) {
		    /* OK, we are the next highest activated member, take
		     * over.
		     */
		    /* All servers need to be present, otherwise one server
		     * could have more up-to-date information.
		     * Example: file is still noted as free in some servers
		     * but one other(inactive) disk has it noted as in use.
		     * Now, if we would grab it, and use it, and then the
		     * other server would come up, we will have conflicting
		     * information.
		     */
		    lock_ino_cache(i) ;
		    ip->i_owner=me ;
		    writeinode(ip,ANN_DELAY|ANN_MINE,SEND_CANWAIT);
		    if ( !(ip->i_flags & I_ALLOCED) ) {
			bs_local_inodes_free++ ;
		    }
		}
	    }
	    unlock_inode(i);
	}
	/* Have a peek .... */
	if ( ip->i_flags&I_INUSE ) {
	    /* We have a valid file. Lock it and see again.
	     */
	    lock_inode(i,L_READ) ;
	    if ( ip->i_flags&I_INUSE && ip->i_owner==me ) {
		if ( ip->i_size==0 ) {
		    /* This is dirty, we assume that an encoded zero is
		     * zero.
		     */
		    /* Zero sized files do not need replication */
		    unlock_inode(i) ;
		    continue ;
		}
		n_replicas=0 ;rep_bits= Inode_reptab[i] ;
		n_active_replicas=0 ; active_bits= rep_bits&active_mask ;
		dbmessage(29,("Inode %ld: rep 0x%x",i,rep_bits));
		if ( rep_bits&my_mask ) {
		    if ( !(ip->i_flags&I_PRESENT) ) {
			bwarn("Inode %ld incorrectly has a replica") ;
			rep_bits &= ~my_mask ;
		    }
		} else {
		    if ( ip->i_flags&I_PRESENT ) {
			bwarn("Inode %ld, my replica missing in reptab",i);
			rep_bits |= my_mask ;
		    }
		}
		for ( tmpmask=rep_bits ; tmpmask ; tmpmask >>= 1 ) {
		    if ( tmpmask&1 ) {
			n_replicas++ ;
			if ( active_bits&1 ) n_active_replicas++ ;
		    }
		    active_bits>>= 1 ;
		}
		if ( cache_never_seen==0 &&
		     n_active_replicas>CopySuper.s_def_repl ) {
			message("Inode %ld: %d replicas too many",
				i,n_active_replicas-CopySuper.s_def_repl) ;
		}
		if ( n_active_replicas==n_replicas ) {
		    dbmessage(15,("Inode %ld: %d replicas",i,n_replicas)) ;
		} else {
		    dbmessage(13,("Inode %ld: %d replicas (%d active)",
				i,n_replicas,n_active_replicas));
		}
		if ( n_replicas==0 && cache_never_seen==0 &&
		     passives_seen(&CopySuper) ) {
#if L_READ!=L_WRITE
		    upgrade_inode_lock(i,ip);
#endif
			
		    bwarn("Inode %ld: no replicas, being destroyed");
		    destroy_inode(i,0,ANN_DELAY|ANN_MINE,SEND_CANWAIT) ;
		    continue ;
	        }
		if ( CopySuper.s_def_repl-n_replicas>cache_never_seen) {
		    /* Number of missing replicas exceeds number of missing
		     * servers.
		     * Try replication.
		     */
#if L_READ!=L_WRITE
		    upgrade_inode_lock(i,ip);
#endif
		    err=try_repl(i,ip,repeat_signal) ;
		
		}
	    }
	    unlock_inode(i);
	}
	/* Quit if anything important went wrong. */
	switch (err) {
	case STD_NOTFOUND :
	case STD_OK :
	    break ;
	default:
	    mu_unlock(&Single_rep) ;
	    return err ;
	}
    }
    mu_unlock(&Single_rep) ;
    return err ;
}

/* Return 1 if and only if all passive members have been seen.
 */
static int
passives_seen(Super_p)
Superblk	*Super_p;
{
	int		i ;

	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
		if ( Super_p->s_memstatus[i].ms_status==MS_PASSIVE ) {
			if ( !(bs_members_seen & (1<<i) ) ) {
				return 0 ;
			}
		}
	}
	return 1 ;
}


/* External interfaces */

/* Member(s) never seen during the current invocation re-entered the arena.
 */
void
gs_flagnew(vec)
int	vec;
{
	/* No locking, called from only one thread */
	int		never_seen ;
	int		i ;
	register int	new_vec ;

	new_vec= bs_members_seen|vec ;
	if ( new_vec == bs_members_seen ) {
		dbmessage(15,("no new replica information"));
		return ;
	}
	bs_members_seen= new_vec ;
	never_seen=0 ;
	super_lock();
	for ( i=0 ; i<S_MAXMEMBER ; i++ ) {
		if ( Superblock.s_memstatus[i].ms_status==MS_ACTIVE ) {
			if ( !(bs_members_seen & (1<<i) ) ) {
				never_seen++ ;
			}
		}
	}
	super_free();
	mu_lock(&Admin_mutex) ;
	nr_servers_never_seen=never_seen ;
	need_sweep=1 ;
	mu_unlock(&Admin_mutex) ;
	dbmessage(5,("There are %d servers we have never seen",never_seen));
}

/* The cache administration decided to throw somebody out. Start
 * checking replication.
 */

void
gs_lostrep()
{
	mu_lock(&Admin_mutex) ;
	need_sweep=1 ;
	mu_unlock(&Admin_mutex);

}
	
/* Called from outside. An administrator wants to be sure. */
errstat
bs_do_repchk()
{
	int	dummy;

	return docheck(1,&dummy) ;
}

void
gs_sweeprep()
{
	int	cache_need_sweep ;
	int	cache_never_seen ;
	int	try_again ;
	errstat	err ;

	mu_lock(&Admin_mutex) ;
	/* Reset the need sweep flag, so that anything happening will
	 * be able to set need_sweep again, even during docheck.
	 * We have to be careful to set need_sweep ourselves if
	 * docheck indicates that that is useful.
	 */
	cache_need_sweep=need_sweep ;
	need_sweep=0 ;
	cache_never_seen=nr_servers_never_seen ;
	mu_unlock(&Admin_mutex);

	if ( cache_need_sweep ) {
		dbmessage(13,("starting sweeper replication check"));
		err=docheck(0,&try_again) ;
		if ( try_again || ( err!=STD_OK && cache_never_seen==0 ) ) {
			mu_lock(&Admin_mutex) ;
			need_sweep=1 ;
			mu_unlock(&Admin_mutex) ;
			dbmessage(3,("replication check needs to be done again"));
		} else {
			dbmessage(3,("replication check ok for the moment"));
		}
	}
}
