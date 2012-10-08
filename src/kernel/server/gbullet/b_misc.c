/*	@(#)b_misc.c	1.2	96/02/28 11:45:58 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  B_MISC.C
 *
 *	Miscellaneous routines for the bullet server.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/strmisc.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "group.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_defs.h"
#include "grp_bullet.h"

#ifdef __STDC__
#include "stdarg.h"
#define VA_LIST			va_list
#define VA_ALIST
#define VA_DCL
#define VA_START( ap, last )	va_start( ap, last )
#define VA_END			va_end
#define VA_ARG			va_arg
#else
#include "varargs.h"
#define VA_LIST			va_list
#define VA_ALIST		, va_alist
#define VA_DCL			va_dcl
#define VA_START( ap, last )	va_start( ap )
#define VA_END			va_end
#define VA_ARG			va_arg
#endif  /* __STDC__ */

extern	Superblk	Superblock;
extern	Inode *		Inode_table;	/* pointer to start of inode table */

int	bs_print_msgs ;			/* print messages on stdout */
int	bs_log_msgs ;			/* log messages on server */
int	bs_ls_sick ;			/* set if log server is sick */

/*
 * BPANIC
 *
 *	Bullet Server panic.  This prints the string "Bullet Server"
 *	followed by the parameters.  Then it quietly, but effectively
 *	brings down the world around it.
 *	For a user mode version it must happen differently!!
 */

/*VARARGS1*/
#ifdef __STDC__
void
bpanic(char * s, ...)

#else /* __STDC__ */

void
bpanic(s VA_ALIST)
char *	s;
VA_DCL

#endif /* __STDC__ */
{
    VA_LIST	args;
    char	buf[MAX_LOG_BUF] ;
    char	*p ;
    int		count ;

    VA_START(args, s);
    p=bprintf(buf,&buf[MAX_LOG_BUF],"Panic - Bullet Server: ");
    count= vsprintf(p,s, args) + ( p-buf ) ;
    buf[count++]= '\n' ; buf[count]= 0 ;
    VA_END(args);

    bs_print_trylock((interval)1) ;
    printf("%s",buf);
    if ( bs_log_msgs ) {
	bs_log_send(buf,count,MAX_LOG_BUF) ;
    }

    panic("");
    /*NOTREACHED*/
}


/*
 * BWARN
 *
 *	Print a bullet server warning message
 */

/*VARARGS1*/
#ifdef __STDC__
void
bwarn(char * s, ...)

#else /* __STDC__ */

void
bwarn(s VA_ALIST)
char * s;
VA_DCL

#endif /* __STDC__ */
{
    VA_LIST	args;
    char	buf[MAX_LOG_BUF] ;
    char	*p ;
    int		count ;

    VA_START(args, s);
    p=bprintf(buf,&buf[MAX_LOG_BUF],"Bullet Server WARNING: ");
    count= vsprintf(p,s, args) + ( p-buf ) ;
    buf[count++]= '\n' ; buf[count]= 0 ;
    VA_END(args);

    bs_print_lock() ;
    printf("%s",buf) ;
    bs_print_unlock() ;
    if ( bs_log_msgs ) {
	bs_log_send(buf,count,MAX_LOG_BUF) ;
    }
}

void
bs_log_send(buf, count, maxcount)
	char	*buf ;
	int	count ;
	int	maxcount ;
{
	header		log_hdr ;
	static port	nilport ;
	char		msg_header[30] ;
	char		*last ;
	int		hdr_count ;
	errstat		err ;

	/* If we have problems with the server */
	if ( bs_ls_sick ) return ;
	/* No port filled in ... */
	if ( PORTCMP(&Superblock.s_logcap.cap_port,&nilport) ) return ;

	/* Insert header in front ... */
	last= bprintf(msg_header,msg_header+sizeof msg_header,
		"From: bullet server %d\n\n",grp_static_id());
	hdr_count= last-msg_header ;
	memmove(buf+hdr_count,buf,
		(maxcount-hdr_count>=count?count:maxcount-hdr_count)) ;
	memcpy(buf,msg_header,hdr_count) ;

	log_hdr.h_port= Superblock.s_logcap.cap_port ;
	log_hdr.h_priv= Superblock.s_logcap.cap_priv ;
	log_hdr.h_command= BS_READ ; /* FAke, not a STD.... */
	log_hdr.h_size= count+hdr_count ;
	log_hdr.h_offset= 0 ;
	err= trans(&log_hdr,buf,count+hdr_count,&log_hdr,NILBUF,0) ;
	if ( err!= STD_OK ) bs_ls_sick= 1 ;
}

/*
 * BS_SUPERCAP
 *
 *	Returns STD_OK if the capability is for object 0 and has the required
 *	rights.  Otherwise it returns STD_CAPBAD if the capability isn't
 *	correct or STD_DENIED if the rights were insufficient.
 */

errstat
bs_supercap(priv, required_rights, peer)
private *	priv;
rights_bits	required_rights;
{
    rights_bits	rights;
    port	*p_random;

    switch ( peer ) {
    case BS_PEER:
	p_random= &Superblock.s_membercap.cap_priv.prv_random ;
	break ;
    case BS_PUBLIC:
	p_random= &Superblock.s_supercap.cap_priv.prv_random ;
	break ;
    default:
	return STD_CAPBAD ;
    }


    if (prv_number(priv) != 0 || prv_decode(priv, &rights, p_random) != 0)
	return STD_CAPBAD;
    if ((rights & required_rights) != required_rights) {
	return STD_DENIED;
    }
    return STD_OK;
}


/*
 * VALIDATE_CAP
 *
 *	Find out if the capability in priv is valid and return in *inodep
 *	the inode number it refers to if all is well.  If "locktype" is
 *	not L_NONE and no errors were encountered then the inode will be
 *	locked on return with the type of lock specified.  Otherwise a
 *	read lock will be used to ascertain if the cap is valid and the
 *	inode will be unlocked before returning.
 *	In case the inode is unknown and owned by a different member
 *	an attempt is done to get the inode information.
 */

errstat
validate_cap(priv, min_rights, inodep, locktype, peer)
private *	priv;		/* capability to be checked */
rights_bits	min_rights;	/* minimum rights for valid capability */
Inodenum *	inodep;		/* number of the inode */
int		locktype;	/* read or write lock? or none? */
int		peer;		/* BS_PEER if peer request */
{
    register Inode *	ip;	/* pointer to the inode */
    rights_bits		rights;	/* access permissions in the file capability */
    Inodenum		inode;	/* number of the inode */
    int			release;
    int			owner ;
    errstat		err ;

    /*
     * Get the inode number.  Make sure it is a valid number.
     * If not we should also check if the capability is the super capability
     * and if so admit that it is for a different kind of object which does
     * not accept the commands for which this routine is used.
     */
    if (locktype == L_NONE)
    {
	release = 1;
	locktype = L_READ;
    }
    else
	release = 0;

    inode = prv_number(priv);
    if (inode <= 0 || inode >= Superblock.s_numinodes)
    {
	if (bs_supercap(priv, min_rights,peer) != STD_CAPBAD)
	    	return STD_COMBAD;
	return STD_CAPBAD;
    }

    /*
     * Check random part and rights using disk version of inode.
     * WARNING.  Assumes port (i_random) does not need decoding since it is a
     * character array.  If this ceases to be true then copy and unpack it
     * before call to prv_decode.  Note, that if an inode is not allocated
     * then we musn't accept the capability!
     */
    ip = Inode_table + inode;

    for(;;) {
	lock_inode(inode, locktype);
	if ( ip->i_flags & I_DESTROY ) {
	    unlock_inode(inode);
	    if ( grp_member_present((int)ip->i_owner)){
	        err=bs_inq_inode(inode) ;
	        if ( err==STD_OK ) {
		    bwarn("STD_OK from bs_inq_inode for %ld",inode);
		    err=STD_SYSERR ;
	        }
	    } else {
		/* If the other member is not present it can not have
		 * re-allocated the inode.
		 */
		err= STD_CAPBAD ;
	    }
	    return err ;
	}
	if ( !(ip->i_flags & I_ALLOCED) ) {
	    owner=ip->i_owner ;
	    unlock_inode(inode);
	    if ( owner==Superblock.s_member ) {
		/* The inode is ours and we know nothing about it */
		return STD_CAPBAD;
	    }
	    err=bs_inq_inode(inode) ;
	    if ( err==STD_OK ) {
		bwarn("STD_OK from bs_inq_inode for %ld",inode);
		err=STD_SYSERR ;
	    }
	    return err;
	}
	if ( prv_decode(priv, &rights, &ip->i_random) < 0)
	{
	    unlock_inode(inode);
	    return STD_CAPBAD;
	}
    
	if ((rights & min_rights) != min_rights)
	{
	    unlock_inode(inode);
	    return STD_DENIED;
	}
    
	/*
	 * A valid capability, so touch the file since someone has accessed
	 * it. gs_reset_ttl can free the inode and return after the inode
	 * has been destroyed.
	 * Requests over the peer port do not spank the lifetime.
	 */
	if ( peer!=BS_PEER && ip->i_flags&I_INUSE ) 
	    switch ( gs_reset_ttl(inode) ) {
	    case STD_OK :
		    break ;
	    case STD_INTR :
		    /* The inode still exists, but was unlocked, lock and
		       redo all checks ...
		     */
		    continue ;
	    case STD_CAPBAD :
		    /* The inode has exhausted its lifetime and was destroyed */
		    return STD_CAPBAD ;
	    }
    
	
	if (release)
	    unlock_inode(inode);
	*inodep = inode;
	return STD_OK;
    }
}

/* Test for bit-for-bit equality between capabilities */
int bs_cap_eq(a,b)
	capability *a, *b ;
{
    return memcmp((_VOIDSTAR) a, (_VOIDSTAR) b, sizeof(capability)) == 0 ;
}
