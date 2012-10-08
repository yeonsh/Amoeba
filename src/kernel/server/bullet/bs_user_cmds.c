/*	@(#)bs_user_cmds.c	1.7	96/02/27 14:12:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * BS_USER_CMDS.C
 *
 *	This file contains the implementation of all the bullet server
 *	specific user interface commands.
 *
 * Author:
 *	Greg Sharp, Jan 1989
 * Modified:
 *	Greg Sharp, Jan 1992 - finer locking granularity
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"
#include "string.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "cache.h"
#include "stats.h"
#include "alloc.h"
#include "bs_protos.h"
#include "module/buffers.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */


extern	Superblk	Superblock;
extern	Statistics	Stats;
extern	Inode *		Inode_table;

/*
 * BS_CREATE
 *
 *	This routine implements the first transaction from a B_CREATE
 *	request.  It accepts a buffer containing data plus some parameters
 *	describing whether the file is complete and should be committed.
 *	If it is not committed then b_modify, b_insert and/or b_delete
 *	commands are expected within the timeout period, so a timer is
 *	started.  If the timer expires the uncommitted file is destroyed.
 */

errstat
bs_create(hdr, buf, size)
register header *	hdr;
bufptr		buf;	/* file data */
b_fsize		size;	/* #bytes of data in buf */
{
    register int		commit;	   /* commit and safety flags */
    register Cache_ent *	cp;
    int				comparing; /* true if we must do a compare */
    errstat			err;
    Inodenum			inode;

    Stats.b_create++;
    assert(size <= BS_REQBUFSZ);

    /*
     * Must check that the capability given is valid and has create permission.
     * It must be committed if it is a file cap so that we can do a compare.
     */
    comparing = 0;
    err = validate_cap(&hdr->h_priv, BS_RGT_CREATE, &inode, L_READ);
    if (err == STD_OK)
    {
        if ((Inode_table + inode)->i_lifetime != 0) /* it is committed */
	    comparing = 1;
	unlock_inode(inode);
    }
    else
    {
	if (err == STD_COMBAD)
	    err = bs_supercap(&hdr->h_priv, BS_RGT_CREATE);
	if (err != STD_OK)
	    return err;
    }

    /* Get an inode with new random number plus cache slot */
    if ((cp = new_file()) == 0)
	return STD_NOSPACE;

    if (comparing) /* save capability of original file */
    {
	cp->c_flags |= COMPARE;
	cp->c_cmpprv = hdr->h_priv;
    }

    /* Make the capability for the new file and fill in the inode */
    if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL, &cp->c_inode.i_random) < 0)
    {
	dispose_file(cp);
	return STD_CAPBAD;
    }

    /*
     * Get memory in the cache for the new file.
     * We get enough for the entire request buffer since we may need it later
     * if the file is not yet complete.  (It reduces the number of realloc's.)
     * Note that alloc_cache_mem() sets cp->c_addr and cp->c_bufsize.
     */
    if (alloc_cache_mem(cp, (b_fsize) BS_REQBUFSZ) != STD_OK)
    {
	dispose_file(cp);
	return STD_NOMEM;
    }

    /* Move the data to the newly allocated cache buffer */
    (void) memmove((_VOIDSTAR) cp->c_addr, (_VOIDSTAR) buf, (size_t) size);

    cp->c_inode.i_size = size;

    commit = hdr->h_extra;
    if (commit & BS_COMMIT)
	return f_commit(cp, commit & BS_SAFETY, &hdr->h_priv);

    unlock_inode(cp->c_innum);
    return STD_OK;
}


/*
 * BS_DELETE
 *
 *	This routine deletes a piece of a file.  If the file is committed
 *	it first makes an uncomitted copy of the file.  It then deletes
 *	"size" bytes starting at position offset.  
 *	The file descreases in size by at most "size" bytes.
 */

#ifdef __STDC__
errstat
bs_delete(
	header * hdr,
	bufptr reqbuf,
	bufsize bufsz)

#else /* __STDC__ */

errstat
bs_delete(hdr, reqbuf, bufsz)
register header *	hdr;
bufptr			reqbuf;
bufsize			bufsz;

#endif /* __STDC__ */
{
    register Cache_ent *	cp;
    Cache_ent *	tmpcp;
    errstat	err;
    b_fsize	size;		/* #bytes data in reqbuf */
    b_fsize	offset;		/* position at which to make changes */
    int		commit;		/* commit and safety flags */
#ifndef NDEBUG
    b_fsize	expected_size;
#endif

    Stats.b_delete++;

    /* Get the cache slot for the file */
    err = make_cache_entry(&hdr->h_priv, BS_RGT_READ | BS_RGT_MODIFY,
							    &tmpcp, L_WRITE);
    if (err != STD_OK)
	return err;
    
    /* Unpack parameters */
    offset = hdr->h_offset;
    commit = (short) hdr->h_extra;
    cp = tmpcp;
    if (buf_get_bfsize(reqbuf, reqbuf+bufsz, &size) == 0 || size <= 0 ||
				offset > cp->c_inode.i_size || offset < 0)
    {
	unlock_inode(cp->c_innum);
	return STD_ARGBAD;
    }

    /* Prevent timeout */
    cp->c_timeout = BS_CACHE_TIMEOUT;
    
    /* Mustn't delete past the end of file */
    if (offset + size > cp->c_inode.i_size)
	size = cp->c_inode.i_size - offset;

#ifndef NDEBUG
    expected_size = cp->c_inode.i_size - size;
#endif

    /* If the file is committed then we had better make a copy */
    if (!(cp->c_flags & NOTCOMMITTED))
    {
	/* Clone file unlocks the original */
	if ((err = clone_file(&tmpcp, cp->c_inode.i_size)) != STD_OK)
	    return err;
	cp = tmpcp;
	assert(inode_locked_by_me(cp->c_innum) == 2);
	/* Generate user's capability for new file */
	if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL,
						    &cp->c_inode.i_random) < 0)
	{
	    dispose_file(cp);
	    return STD_CAPBAD;
	}
    }

    /* Move top part down */
    (void) memmove((_VOIDSTAR) (cp->c_addr + offset),
			(_VOIDSTAR) (cp->c_addr + offset + size),
			    (size_t) (cp->c_inode.i_size - offset - size));
    cp->c_inode.i_size -= size;

    assert(cp->c_inode.i_size == expected_size);
    /*
     * We don't free up any excess cache memory now.  We leave that until the
     * commit.
     */
    if (commit & BS_COMMIT)
	return f_commit(cp, commit & BS_SAFETY, &hdr->h_priv);
    unlock_inode(cp->c_innum);
    return STD_OK;
}


/*
 * BS_INSERT
 *
 *	This routine takes a capability for a file.  If the file is committed
 *	it first makes an uncomitted copy of the file.  It then inserts the
 *	"size" bytes in the request buffer into the file at position offset.  
 *	The file grows by "size" bytes.
 */

errstat
bs_insert(hdr, reqbuf, size)
header *	hdr;
bufptr		reqbuf;		/* buffer containing replacement data */
b_fsize		size;		/* #bytes data in reqbuf */
{
    register Cache_ent *	cp;
    Cache_ent *	tmpcp;
    b_fsize	newsize;
    errstat	err;
    int		commit;
    b_fsize	offset;
#ifndef NDEBUG
    b_fsize	expected_size;
#endif

    Stats.b_insert++;

    /* Get the cache slot for the file */
    err = make_cache_entry(&hdr->h_priv, BS_RGT_READ | BS_RGT_MODIFY,
							    &tmpcp, L_WRITE);
    if (err != STD_OK)
	return err;

    cp = tmpcp;
    /* Unpack parameters in header */
    commit = (short) hdr->h_extra;
    offset = hdr->h_offset;

    if (size <= 0 || offset > cp->c_inode.i_size || offset < 0)
    {
	unlock_inode(cp->c_innum);
	return STD_ARGBAD;
    }

    /* Prevent timeout */
    cp->c_timeout = BS_CACHE_TIMEOUT;
    
    newsize = cp->c_inode.i_size + size;
#ifndef NDEBUG
    expected_size = newsize;
#endif

    /* If the file is committed then we had better make a copy */
    if (!(cp->c_flags & NOTCOMMITTED))
    {
	if ((err = clone_file(&tmpcp, newsize)) != STD_OK)
	    return err;
	cp = tmpcp;
	assert(inode_locked_by_me(cp->c_innum) == 2);
	/* Generate user's capability for new file */
	if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL,
						    &cp->c_inode.i_random) < 0)
	{
	    dispose_file(cp);
	    return STD_CAPBAD;
	}
    }
    else	/* file will grow so make space */
    {
	if (newsize > cp->c_bufsize && (err = growbuf(cp, newsize)) != STD_OK)
	{
	    unlock_inode(cp->c_innum);
	    return err;
	}
	cp->c_inode.i_size = newsize;
    }

    /* Move top piece up.  Note that i_size has been increased already. */
    (void) memmove((_VOIDSTAR) (cp->c_addr + offset + size),
			(_VOIDSTAR) (cp->c_addr + offset),
			    (size_t) (cp->c_inode.i_size - size - offset));

    /* Insert new data */
    (void) memmove((_VOIDSTAR) (cp->c_addr + offset), (_VOIDSTAR) reqbuf,
								(size_t) size);

    assert(cp->c_inode.i_size == expected_size);

    if (commit & BS_COMMIT)
	return f_commit(cp, commit & BS_SAFETY, &hdr->h_priv);
    unlock_inode(cp->c_innum);
    return STD_OK;
}


/*
 * BS_MODIFY
 *
 *	This routine takes a capability for a file.  If the file is committed
 *	it first makes an uncomitted copy of the file.  It then replaces
 *	The section of code, beginning at "offset" with the "size" bytes in the
 *	request buffer.  If "offset"+"size" > file size then the file grows.
 */

errstat
bs_modify(hdr, reqbuf, size)
header *	hdr;
bufptr		reqbuf;		/* buffer containing replacement data */
b_fsize		size;		/* #bytes data in reqbuf */
{
    register Cache_ent *	cp;
    Cache_ent *	tmpcp;
    int		commit;		/* commit and safety flags */
    b_fsize	newsize;
    b_fsize	offset;		/* position at which to make changes */
    errstat	err;
#ifndef NDEBUG
    b_fsize	expected_size;
#endif

    Stats.b_modify++;

    /* Get the cache slot for the file */
    err = make_cache_entry(&hdr->h_priv, BS_RGT_READ | BS_RGT_MODIFY,
							    &tmpcp, L_WRITE);
    if (err != STD_OK)
	return err;
    cp = tmpcp;
    commit = (short) hdr->h_extra; /*AMOEBA4*/
    offset = hdr->h_offset;

    /*
     * Offset must lie within the file size and the size of piece to be
     * modified must be >=0
     */
    if (size < 0 || offset > cp->c_inode.i_size || offset < 0)
    {
	unlock_inode(cp->c_innum);
	return STD_ARGBAD;
    }

    /*
     * See if file is going to grow.  If we have to clone an uncommitted
     * file we can pre-allocate enough space for the resultant file.
     */
    if (offset + size > cp->c_inode.i_size)
	newsize = offset + size;
    else
	newsize = cp->c_inode.i_size;

#ifndef NDEBUG
    expected_size = newsize;
#endif

    /*
     * If the file is committed then we make a copy.
     * NB: clone_file modifies cp to point to the clone's cache entry.
     */
    if (!(cp->c_flags & NOTCOMMITTED))
    {
	if ((err = clone_file(&tmpcp, newsize)) != STD_OK)
	    return err;
	cp = tmpcp;
	assert(inode_locked_by_me(cp->c_innum) == 2);
	/* Generate capability for new file */
	if (prv_encode(&hdr->h_priv, cp->c_innum, BS_RGT_ALL,
						&cp->c_inode.i_random) < 0)
	{
	    dispose_file(cp);
	    return STD_CAPBAD;
	}
    }

    /* Prevent uncommitted file timeout */
    cp->c_timeout = BS_CACHE_TIMEOUT;

    /*
     * IF size == 0 THEN only commit and return.  If size == 0 and commit
     * isn't set we should probably flag an error but we just ignore it
     * since it causes no harm.
     * ELSE do modify first.
     */
    if (size != 0)
    {
	if (cp->c_addr == 0 && cp->c_inode.i_size != 0)
	    bpanic("b_modify: uncommitted file not in cache!");

	/*
	 * If file will grow, make space (this won't happen if we just
	 * cloned!)
	 */
	if (newsize > cp->c_bufsize && (err = growbuf(cp, newsize)) != STD_OK)
	{
	    unlock_inode(cp->c_innum);
	    return err;
	}

	/* Copy the data to the right place in the cache */
	(void) memmove((_VOIDSTAR) (cp->c_addr + offset), (_VOIDSTAR) reqbuf,
								(size_t) size);

	cp->c_inode.i_size = newsize;
    }

    assert(cp->c_inode.i_size == expected_size);

    if (commit & BS_COMMIT)
	return f_commit(cp, commit & BS_SAFETY, &hdr->h_priv);

    unlock_inode(cp->c_innum);
    return STD_OK;
}


/*
 * BS_READ
 *
 *	bs_read() reads "size" bytes from the file specified by "hdr->h_priv"
 *	starting "offset" bytes from the start of the file.
 *	This routine must do its own putrep!
 *	NB: A file must be committed before it can be read.
 */

void
bs_read(hdr)
header *	hdr;		/* parameters for read are in here */
{
    Cache_ent *	cp;	/* pointer to cache slot */
    bufptr	eof;	/* pointer to end of file */
    errstat	err;
    bufptr	replybuf;

    Stats.b_read++;

    /* If size is negative then error */
    if ((short) hdr->h_size < 0)
    {
	hdr->h_size = 0;
	hdr->h_status = STD_ARGBAD;
	goto CLEANUP;
    }

    /* Get a cache slot for the file if it exists & make sure it is committed */
    err = make_cache_entry(&hdr->h_priv, BS_RGT_READ, &cp, L_READ);
    if (err != STD_OK)
    {
	hdr->h_size = 0;
	hdr->h_status = err;
	goto CLEANUP;
    }
    if (cp->c_flags & NOTCOMMITTED)
    {
	unlock_inode(cp->c_innum);
	hdr->h_size = 0;
	hdr->h_status = STD_CAPBAD;
	goto CLEANUP;
    }

    /* If size == 0 nothing to do */
    if (hdr->h_size == 0)
    {
	unlock_inode(cp->c_innum);
	hdr->h_status = STD_OK;
	goto CLEANUP; /* It isn't really a failure, but ... */
    }

    /* Is the offset within the file? */
    if (hdr->h_offset > cp->c_inode.i_size || hdr->h_offset < 0)
    {
	unlock_inode(cp->c_innum);
	hdr->h_size = 0;
	hdr->h_status = STD_ARGBAD;
	goto CLEANUP;
    }

    /* Read the file from disk if it isn't in core */
    if (cp->c_addr == 0 && cp->c_inode.i_size > 0 &&
					    (err = bs_loadfile(cp)) != STD_OK)
    {
	unlock_inode(cp->c_innum);
	hdr->h_size = 0;
	hdr->h_status = err;
	goto CLEANUP;
    }

    /* Make sure we don't read more of the file than actually exists */
    replybuf = cp->c_addr + hdr->h_offset;
    eof = cp->c_addr + cp->c_inode.i_size;
    if (replybuf + hdr->h_size >= eof)
	hdr->h_size = eof - replybuf;

    hdr->h_status = STD_OK;
#ifdef USE_AM6_RPC
    rpc_putrep(hdr, replybuf, hdr->h_size);
#else
    putrep(hdr, replybuf, hdr->h_size);
#endif /* USE_AM6_RPC */
    unlock_inode(cp->c_innum);
    return;

CLEANUP:

#ifdef USE_AM6_RPC
    rpc_putrep(hdr, NILBUF, hdr->h_size);
#else
    putrep(hdr, NILBUF, hdr->h_size);
#endif /* USE_AM6_RPC */
    return;
}


/*
 * BS_SIZE
 *
 *	This function returns the size of the file as recorded in the inode.
 *	Uncommitted files cannot be sized.  This avoids using the cache and
 *	thus risking throwing something out for no purpose.
 */

errstat
bs_size(hdr)
header *        hdr;
{
    errstat	err;
    Inodenum	inode;
    Inode *	ip;

    Stats.b_size++;
    err = validate_cap(&hdr->h_priv, BS_RGT_READ, &inode, L_READ);
    if (err == STD_OK)
    {
	ip = Inode_table + inode;
	if (ip->i_lifetime == 0) /* File is uncommitted */
	    err = STD_CAPBAD;
	else /* decode size and return it */
	{
	    b_fsize	size;

	    size = ip->i_size;
	    DEC_FILE_SIZE_BE(&size);
	    hdr->h_offset = (long) size;
	}
	unlock_inode(inode);
    }
    return err;
}
