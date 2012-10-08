/*	@(#)grp_adm.c	1.1	96/02/27 14:07:13 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * GRP_ADM.C
 *
 *	This file contains the implementation of the group elements.
 *
 * Author:
 *	Ed Keizer & Kees Verstoep
 */

#include <stdlib.h>
#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdcom.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "module/strmisc.h"
#include "module/buffers.h"

#include "module/disk.h"
#include "group.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "alloc.h"
#include "stats.h"
#include "cache.h"
#include "bs_protos.h"
#include "preempt.h"
#include "assert.h"
#include "grp_defs.h"
#include "grp_comm.h"
#include "grp_bullet.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif

extern	Superblk	Superblock;	/* in-core copy of superblock */
extern	Statistics	Stats;

static char *memstat[] = { "none", "new", "exists", "passive" } ;

extern errstat gs_be_welcome();
extern errstat gs_help_member();

/*
 *	A new member of the server has announced itself.
 *
 */

void
gs_welcome(hdr, buf, end)
header *hdr;
bufptr  buf;
bufptr  end;
{
    int			my_index;
    errstat		error;
    static mutex	being_welcomed ;

    my_index = hdr->h_size;

    /* First make sure we are alone. This is needed because the
     * server might have crashed leaving the superblock with state NEW.
     * The administrator has to replay the b_attach, but we do not
     * want to have multiple of these running.
     */

    mu_lock(&being_welcomed) ;
    if (my_index < 0 || my_index >= S_MAXMEMBER) {
	bwarn("gs_welcome: wrong member id (%d)", my_index);
	error=STD_ARGBAD;
    } else {
	error= gs_be_welcome(my_index, buf, end);
    }
    mu_unlock(&being_welcomed) ;
    hdr->h_status= error ; hdr->h_size=0 ;
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, NILBUF, hdr->h_size);
#else
    putrep(hdr, NILBUF, hdr->h_size);
#endif /* USE_AM6_RPC */
    if ( error==STD_OK ) {
	/* Join the group.
	 */
	if (!bs_build_group()) {
	    bpanic("gs_welcome: cannot join group");
	}
	bs_start_service() ;

    }
    return  ;
}

errstat
gs_attach(hdr, buf, end, peer)
header * hdr;
bufptr   buf;
bufptr   end;
int      peer;
{
    capability member_priv;
    errstat	err ;

    Stats.g_attach++;
    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,peer)) != STD_OK) {
	return err;
    }

    /* the argument required is the private cap of the joining member */
    if (buf_get_cap(buf, end, &member_priv) == NULL) {
	return STD_ARGBAD;
    }

    if (bs_grp_missing()) return STD_NOTNOW ;

    return gs_help_member(&member_priv);
}


/*
 *	A member is changing state.
 *
 */

errstat
bs_state(hdr)
header *	hdr;
{
    errstat	err ;

    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL,BS_PEER)) != STD_OK) {
	return err;
    }

    return bs_do_state(hdr,BS_PEER) ;
}


/*
 *	A member is changing state.
 *
 */

errstat
bs_do_state(hdr,peer)
header *	hdr;
int		peer;
{
    errstat	err ;
    int		action ;
    int		member ;
    int		need_all ;
    int		my_eyes_only;

    Stats.g_state++;

    member= hdr->h_offset ;
    need_all= !(hdr->h_extra&BS_STATE_FORCE) ;
    action= hdr->h_extra & BS_STATE_MASK ;

    dbmessage(14,("action %d, member %d",action,member));

    if ( member< -1 || member>=S_MAXMEMBER ) return STD_ARGBAD ;

    /* A simple test whose results are used later */
    my_eyes_only= (member== -1 || member==Superblock.s_member) ; 

    /* If the force flag is off, we check whether everybody is there.
     */
    if ( action!=BS_EXIT && (need_all && bs_grp_missing()!=0) ) {
	return STD_NOTNOW ;
    }

    switch ( action ) {
    case BS_EXIT:
	if ( !my_eyes_only ) return STD_ARGBAD ;
	bs_create_mode(0) ;
	if ( need_all ) {
	    /* Force flag on means "wait for stability" on exit */
	    err=bs_retreat(0);
	    if ( err!=STD_OK ) return err ;
	}
	/* We return the RPC status before we panic */
	hdr->h_size=0 ;
	hdr->h_status= STD_OK ;
#ifdef USE_AM6_RPC
	rpc_putrep(hdr, NILBUF, 0);
#else
	putrep(hdr, NILBUF, 0);
#endif /* USE_AM6_RPC */
	message("Exiting by user request") ;
#ifndef USER_LEVEL
	/* We are in the kernel */
	bs_set_state(BS_DEAD);
#else
	exit(0);
#endif
    case BS_PASSIVE:
	err=bs_change_status(member,MS_PASSIVE,(capability *)0,need_all) ;
	break ;
    case BS_DETACH:
	err=bs_change_status(member,MS_NONE,(capability *)0,need_all) ;
    case BS_CAIN:
	/* A peer member thinks I am not in his group. If we
	 * really are not and we are functioning, perform suicide.
	 */
	if ( grp_member_present(Superblock.s_member) &&
	     !grp_member_present((int)hdr->h_size) ) {
		hdr->h_size=0 ;
		hdr->h_status= STD_OK ;
#ifdef USE_AM6_RPC
		rpc_putrep(hdr, NILBUF, 0);
#else
		putrep(hdr, NILBUF, 0);
#endif /* USE_AM6_RPC */
		message("Exiting by peer request") ;
#ifndef USER_LEVEL
		/* We are in the kernel */
		bs_set_state(BS_WAITING);
#else
		exit(0);
#endif
	} 
	err=STD_NOTNOW ;
	break ;
    case BS_TESTSTABLE:
	if ( !my_eyes_only ) return STD_ARGBAD ;
	err= bs_is_stable();
	break ;
    default:
	err= STD_ARGBAD ;
    }
    return err ;
}

errstat
bs_retreat(hang_in) {
    static mutex exiting;

    /* Announce that touches must come immediatly */
    gs_age_exit();

    /* Only one thread may hang in this... */
    if ( hang_in ) {
	mu_lock(&exiting) ;
    } else {
	if ( mu_trylock(&exiting,(interval)0)<0 ) return STD_EXISTS ;
    }

    while ( bs_is_stable()!=STD_OK ) {
	mu_trylock(&exiting,(interval)4000);
    }
    /* Disconnect yourself */
    bs_leave_group();
    return STD_OK ;
    /* The caller is obliged to do an exit soon after this return ... */
}

/* A test whether there is enough in the superblock to get started.
 */

char *
bs_can_start_service()
{
    /* Tests whether superblock is good enough to start a service */
    int		me ;

    me= Superblock.s_member ;
    if ( me==S_UNKNOWN_ID ) return "Fresh slave server" ;
    if ( me<0 || me>=S_MAXMEMBER || me>=Superblock.s_maxmember )
	return "illegal member id" ;
    /* No super_lock needed, we are alone ... */
    switch( Superblock.s_memstatus[me].ms_status ) {
    case MS_NONE:
	return "no status!!!!!" ;
    case MS_NEW:
	dbmessage(3,("server is still fresh")); ;
    case MS_PASSIVE:
	bs_create_mode(0) ;
    case MS_ACTIVE:
	/* OK */
	break ;
    default:
	return "unknown state" ;
    }
    return (char *)0 ;

}

/* Status messages called by std_status handlers */

char *
bs_grp_ms(status)
	uint16	status;
{
	static char	mbuf[20] ;

	if ( status >= (sizeof memstat)/sizeof (char *) ) {
		bprintf(mbuf,mbuf+sizeof mbuf,"?%d",status) ;
		return mbuf ;
	} else {
		return memstat[status] ;
	}
}
	
/* Put the status of one particular member in the buffer */
char *
member_status(begin,end)
	char	*begin;
	char	*end;
{
	char	*buf;

	buf=begin;
	if ( Superblock.s_member==S_UNKNOWN_ID) {
		return bprintf(buf,end,"Bullet Server Member, server number unknown\n\n") ;
	}
	super_lock();
	buf = bprintf(buf, end, "Storage server %3d, %s",
		Superblock.s_member,
		bs_grp_ms(Superblock.s_memstatus[Superblock.s_member].ms_status)) ;
	buf=bprintf(buf,end,", superblock seqno %d\n",
		Superblock.s_seqno) ;
	super_free();
	return buf ;
	
}

char *
grp_status(begin,end)

	char		*begin;
	char		*end;
{
	char		*buf;
	int		i;
	Inodenum	inodes_free ;
	disk_addr	blocks_free ;
	disk_addr	blocks ;
	Inodenum	total_inodes_free ;
	disk_addr	total_blocks_free ;
	disk_addr	total_blocks ;

	buf=begin;

	if (Superblock.s_maxmember==0)
		return bprintf(buf,end,"no member information present\n") ;

	total_inodes_free=0 ; total_blocks_free=0 ; total_blocks=0;
	buf=bprintf(buf,end,"member   state     activity  inodes free  blocks free      used\n");
	super_lock() ;
	for ( i=0 ; i<Superblock.s_maxmember ; i++ ) 
	if ( Superblock.s_memstatus[i].ms_status != MS_NONE ) {
	    inodes_free= bs_grp_ino_free(i);
	    blocks_free= bs_grp_bl_free(i) ;
	    blocks= bs_grp_blocks(i) ;
	    total_inodes_free += inodes_free ;
	    total_blocks_free += blocks_free ;
	    total_blocks+= blocks ;
	    buf=bprintf(buf,end,"%3d%1s:  %7s  %11s       %6ld    %9ld %9ld\n",i,
			(i==Superblock.s_member?"*":" "),
			bs_grp_ms(Superblock.s_memstatus[i].ms_status),
			grp_mem_status(i),
			inodes_free, blocks_free,blocks-blocks_free
		);
	}
	super_free();
	buf=bprintf(buf,end,"total: %ld out of %ld inodes free\n",
		total_inodes_free,Superblock.s_numinodes) ;
	buf=bprintf(buf,end,"       %ld out of %ld blocks free\n",
		total_blocks_free,total_blocks) ;


	return buf ;
}
