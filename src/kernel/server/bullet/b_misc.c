/*	@(#)b_misc.c	1.6	96/02/27 14:11:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  B_MISC.C
 *
 *	Miscellaneous routines for the bullet server.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "cache.h"
#include "bs_protos.h"

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

    VA_START(args, s);
    printf("Panic - Bullet Server: ");
    (void) vprintf(s, args);
    printf("\n");
    VA_END(args);

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

    VA_START(args, s);
    printf("Bullet Server WARNING: ");
    (void) vprintf(s, args);
    printf("\n");
    VA_END(args);
}


/*
 * BS_SUPERCAP
 *
 *	Returns STD_OK if the capability is for object 0 and has the required
 *	rights.  Otherwise it returns STD_CAPBAD if the capability isn't
 *	correct or STD_DENIED if the rights were insufficient.
 */

#define	SUPER_RANDOM	Superblock.s_supercap.cap_priv.prv_random

errstat
bs_supercap(priv, required_rights)
private *	priv;
rights_bits	required_rights;
{
    rights_bits	rights;

    if (prv_number(priv) != 0 || prv_decode(priv, &rights, &SUPER_RANDOM) != 0)
	return STD_CAPBAD;
    if ((rights & required_rights) != required_rights)
	return STD_DENIED;
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
 */

errstat
validate_cap(priv, min_rights, inodep, locktype)
private *	priv;		/* capability to be checked */
rights_bits	min_rights;	/* minimum rights for valid capability */
Inodenum *	inodep;		/* number of the inode */
int		locktype;	/* read or write lock? or none? */
{
    register Inode *	ip;	/* pointer to the inode */
    rights_bits		rights;	/* access permissions in the file capability */
    Inodenum		inode;	/* number of the inode */
    int			release;

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
	if (bs_supercap(priv, min_rights) != STD_CAPBAD)
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
    lock_inode(inode, locktype);
    ip = Inode_table + inode;
    if ((!(ip->i_flags & I_ALLOCED)) ||
				prv_decode(priv, &rights, &ip->i_random) < 0)
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
     * A valid capability, so touch the file since someone has accessed it.
     * NB: we don't touch files not yet written to disk since they don't have a
     * valid inode - either on disk or in core.
     * A file is on disk if its lifetime is non-zero.
     */
    if (ip->i_lifetime != 0 && ip->i_lifetime != MAX_LIFETIME)
	ip->i_lifetime = MAX_LIFETIME;
    
    if (release)
	unlock_inode(inode);
    *inodep = inode;
    return STD_OK;
}
