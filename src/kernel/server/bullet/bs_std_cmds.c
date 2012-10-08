/*	@(#)bs_std_cmds.c	1.7	96/02/27 14:12:03 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BS_STD_CMDS.C
 *
 *	This file contains the routines for all the standard commands
 *	implemented by the bullet server
 *
 *  Author:
 *	Greg Sharp, Jan 1989
 *  Modified:
 *	Greg Sharp, Jan 1992 - finer locking granularity
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "stats.h"
#include "bs_protos.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "module/buffers.h"
#include "module/strmisc.h"

extern	Superblk	Superblock;	/* in-core superblock */
extern	Inode *		Inode_table;	/* pointer to in-core inode table */
extern	Statistics	Stats;		/* list of statistics */
extern	b_fsize		Blksizemin1;	/* used by NUMBLKS */


/*
 * STD_AGE
 *
 *	This routine goes through every inode (a potentially SLOW operation)
 *	and decrements its time to live.  If the time to live reaches 0
 *	the file is deleted.
 *	NB:  This does not alter the i_lifetime in the cache since the version
 *	     of i_lifetime in the cache is NOT used except to touch the disk
 *	     copy after the inode has been used.  This "inconsistency" is not
 *	     a problem as long as nobody tries to use the cached lifetime for
 *	     anything else.
 */

errstat
bs_std_age(hdr)
header *	hdr;
{
    register Inodenum	i;
    register Inode *	ip;
    register errstat	err;

    if ((err = bs_supercap(&hdr->h_priv, BS_RGT_ALL)) == STD_OK)
    {
	for (ip = Inode_table + 1, i = 1; i < Superblock.s_numinodes; ip++, i++)
	{
#ifndef USER_LEVEL
	    /*
	     * Because we might run for a long time we should give the
	     * interrupt queue a chance to drain by rescheduling ourselves
	     * after each 512 inodes
 	     */
	    if (!(i & 511))
		threadswitch();
#endif

	    /*
	     * Most inodes are free so we peak to see if it is free before
	     * spending a lot of time locking it.  We do have to check that
	     * is still in use after we get the lock though!
	     */
	    if (ip->i_lifetime != 0)
	    {
		lock_inode(i, L_WRITE);
		if (ip->i_lifetime != 0 && --ip->i_lifetime == 0)
		{
		    Stats.i_aged_files++;
		    destroy_inode(i); /* unlocks inode */
		}
		else
		    unlock_inode(i);
	    }
	}

	Stats.std_age++; /* Only count successful completion of this! */
    }
    return err;
}


/*
 * REMOTE_SIZE
 *
 *	Gets the size of the remote file specified by cap.
 */

static errstat
remote_size(cap, size)
capability *	cap;
b_fsize *	size;
{
    header	hdr;
    bufsize	n;

    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_command = BS_SIZE;
    if ((n = trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0)) != 0)
	return ERR_CONVERT(n);
    if (hdr.h_status != STD_OK)
	return ERR_CONVERT(hdr.h_status);
    *size = (b_fsize) hdr.h_offset;
    return STD_OK;
}


/*
 * REMOTE_READ
 *
 *	This routine reads the first "size" bytes of the file specified
 *	by "cap" into the buffer pointed to by "buf".  It does this via
 *	transactions with the file server managing "cap".
 */

static errstat
remote_read(cap, buf, size)
capability *	cap;		/* capability for the file to be read */
bufptr		buf;		/* buffer to read the file into */
b_fsize		size;		/* size of file (and buffer) */
{
    header      hdr;
    bufsize     n; 
     
    hdr.h_port = cap->cap_port;
    hdr.h_priv = cap->cap_priv;
    hdr.h_offset = 0;
    /* Until we get big transactions we put a loop around everything */
    do
    {
	hdr.h_command = BS_READ;
	hdr.h_size = size > BS_REQBUFSZ ? BS_REQBUFSZ : (short) size;
	n = trans(&hdr, NILBUF, 0, &hdr, buf+hdr.h_offset, hdr.h_size);
	if (ERR_STATUS(n))
	    return ERR_CONVERT(n);
	if (hdr.h_status != STD_OK)
	    return ERR_CONVERT(hdr.h_status);
	hdr.h_offset += (short) n;
	size -= (short) n;
    } while (size > 0);
    return STD_OK;
}


/*
 * STD_COPY
 *
 *	This routine makes a copy of a file specified by the capability given
 *	in the parameter.  The file will normally be on another file server.
 *	It returns the capability for the newly created file.
 *	The value of the function is the error status of the create operation.
 *	The statistics differentiate between a local and remote copy to check
 *	that remote copies are indeed more frequent.
 */

errstat
bs_std_copy(hdr, buf, bufend)
header *	hdr;
bufptr		buf;
bufptr		bufend;
{
    capability	fcap;		/* capability of object to read */
    Cache_ent *	cp;		/* pointer to cache slot of new local file */
    Inodenum	dummy;		/* holds extra parameter for validate_cap */
    errstat	n;		/* holds return status of remote ops */
    b_fsize	size;		/* size of remote file */

    /*
     * Must check that the server capability given is valid and has create
     * permission
     */
    n = bs_supercap(&hdr->h_priv, BS_RGT_CREATE);
    if (n == STD_CAPBAD)
	n = validate_cap(&hdr->h_priv, BS_RGT_CREATE, &dummy, L_NONE);
    if (n != STD_OK)
	return n;

    /* Get the capability out of the buffer */
    if (buf_get_cap(buf, bufend, &fcap) == 0)
	return STD_ARGBAD;

    /* See if it is a local or a remote copy */
    if (PORTCMP(&fcap.cap_port, &Superblock.s_supercap.cap_port))
    {
	Stats.std_localcopy++;
	n = make_cache_entry(&fcap.cap_priv, BS_RGT_READ, &cp, L_WRITE);
	if (n != STD_OK)
	    return n;
	if (cp->c_flags & NOTCOMMITTED)
	{
	    unlock_inode(cp->c_innum);
	    return STD_CAPBAD;
	}
	n = clone_file(&cp, cp->c_inode.i_size);
	/* Now cp points to the new uncommitted clone */
	if (n != STD_OK)
	{
	    dispose_file(cp);
	    return n;
	}
	assert(inode_locked_by_me(cp->c_innum) == 2);
    }
    else
    {
	Stats.std_remotecopy++;
	/* We still have no locks - get file size */
	n = remote_size(&fcap, &size);
	if (n != STD_OK)
	    return n;
	/* Allocate an inode and cache entry for the new file */
	if ((cp = new_file()) == 0)
	    return STD_NOSPACE;

	/* Allocate memory from buffer cache */
	if (alloc_cache_mem(cp, size) != STD_OK)
	{
	    n = STD_NOMEM;
	}
	else
	{
	    if ((cp->c_inode.i_size = size) > 0)
	    {
		/* Do a read transaction with remote bullet server */
		n = remote_read(&fcap, cp->c_addr, size);
	    }
	    else
		n = STD_OK;
	}
	if (n != STD_OK)
	{
	    dispose_file(cp);
	    return n;
	}
    }

    /* Create capability for new file */
    if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL,
						    &cp->c_inode.i_random) < 0)
    {
	dispose_file(cp);
	return STD_CAPBAD;
    }

    /*
     * Now allocate disk blocks and write the file to disk with safety.
     * The inode is also unlocked in the process.
     */
    return f_commit(cp, BS_SAFETY, &hdr->h_priv);
}


/*
 * STD_DESTROY
 *
 *	This removes a bullet server file completely and frees all the
 *	resources it was using.
 */

errstat
bs_std_destroy(hdr)
header *	hdr;
{
    errstat	err;		/* status of capability check */
    Inodenum	inode;		/* inode to destroy */

    Stats.std_destroy++;
    err = validate_cap(&hdr->h_priv, BS_RGT_DESTROY, &inode, L_WRITE);
    if (err == STD_OK)
	destroy_inode(inode);
    return err;
}


/*
 * STD_INFO
 *
 *	Returns standard string describing the specified bullet server object.
 */

#define	BS_INFOSTRLEN	32

void
bs_std_info(hdr)
header *	hdr;
{
    char 	info_str[BS_INFOSTRLEN];
    char *	end;
    errstat	err;
    Inodenum	inode;
    Inode *	ip;
    b_fsize	size;

    Stats.std_info++;

    end = info_str + BS_INFOSTRLEN;

    err = validate_cap(&hdr->h_priv, BS_RGT_READ, &inode, L_READ);
    switch (err)
    {
    case STD_OK:
	ip = Inode_table + inode;
	if (ip->i_lifetime == 0) /* File is uncommitted */
	{
	    unlock_inode(inode);
	    hdr->h_size = 0;
	    err = STD_CAPBAD;
	}
	else
	{
	    /* Otherwise copy the info string and add the file size */
	    size = ip->i_size;
	    unlock_inode(inode);
	    DEC_FILE_SIZE_BE(&size);
	    end = bprintf(info_str, end, "- %8ld bytes", size);
	}
	break;
    
    case STD_DENIED:
	break;

    default:
	if ((err = bs_supercap(&hdr->h_priv, BS_RGT_READ)) == STD_OK)
	    end = bprintf(info_str, end, "bullet server");
	break;
    }

    if ((hdr->h_status = err) != STD_OK)
	hdr->h_size = 0;
    else
    {
	hdr->h_size = end - info_str;
	*end = '\0';
    }
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, info_str, hdr->h_size);
#else
    putrep(hdr, info_str, hdr->h_size);
#endif /* USE_AM6_RPC */
}


/*
 * STD_RESTRICT
 *
 *	This function implements the STD_RESTRICT function for the
 *	bullet server.  It takes a capability and builds a new one
 *	with fewer rights.
 */

errstat
bs_std_restrict(priv, rights_mask)
private *	priv;
rights_bits	rights_mask;
{
    Inode *	ip;
    Inodenum	inode;
    rights_bits	rights;
    errstat	err;

    Stats.std_restrict++;

    /*
     * We handle the restricting of the super capability separately since we
     * it has no inode.
     */
#define SUPER_RANDOM    Superblock.s_supercap.cap_priv.prv_random
    if (prv_number(priv) == 0) /* a super capability! */
    {
        if (prv_decode(priv, &rights, &SUPER_RANDOM) == 0 &&
	    prv_encode(priv, (objnum) 0, rights & rights_mask, &SUPER_RANDOM)
									  == 0)
	{
	    return STD_OK;
	}
	return STD_CAPBAD;
    }
#undef SUPER_RANDOM

    /*
     * We got a file capability.  Validate capability, get the current rights
     * field and make a restricted capability.
     */
    if ((err = validate_cap(priv, (rights_bits) 0, &inode, L_READ)) == STD_OK)
    {
	ip = Inode_table + inode;
	if (prv_decode(priv, &rights, &ip->i_random) != 0 ||
	    prv_encode(priv, inode, rights & rights_mask, &ip->i_random) != 0)
	{
	    err = STD_CAPBAD;
	}
	unlock_inode(inode);
    }
    return err;
}


/*
 * STD_TOUCH
 *
 *	This stops garbage collection of a file by resetting the i_lifetime
 *	of the file.  This is a side-effect of validate_cap().
 *	No rights are required to touch an object.
 */

errstat
bs_std_touch(priv)
private *	priv;
{
    Inodenum	inode;

    Stats.std_touch++;
    return validate_cap(priv, (rights_bits) 0, &inode, L_NONE);
}


/*
 * STD_NTOUCH
 *
 *	This stops garbage collection of a file by resetting the i_lifetime
 *	of the n files corresponding to the array of private parts.  This is
 *	a side-effect of validate_cap().
 *	No rights are required to touch an object.
 *	The routine assumes the array priv really points to sufficient data
 *	so that we don't stomp someone else's memory.
 */

#if defined(__STDC__)
errstat
bs_std_ntouch(bufsize n, private * priv, bufsize * done)
#else
errstat
bs_std_ntouch(n, priv, done)
bufsize		n;	/* # entries in priv array */
private *	priv;	/* array of caps to touch */
bufsize *	done;	/* returns the # successfull touches */
#endif /* __STDC__ */
{
    Inodenum	inode;
    errstat	ret;
    errstat	err;
    bufsize	count;

    ret = STD_OK;
    count = 0;
    Stats.std_touch += n;
    if (n != 0)
    {
	do
	{
	    if (prv_number(priv) == 0)
		err= STD_OK; /* Skip the super cap */
	    else
		err = validate_cap(priv, (rights_bits) 0, &inode, L_NONE);
	    if (err != STD_OK)
		ret = err;
	    else
		count++;
	    priv++;
	} while (--n != 0);
    }
    *done = count;
    return ret;
}
