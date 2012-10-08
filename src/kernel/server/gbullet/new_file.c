/*	@(#)new_file.c	1.1	96/02/27 14:07:31 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * NEW_FILE.C
 *
 *	This file contains routines related to making new files in the cache
 *	and saving them.
 *
 * Author:
 *	Greg Sharp, Jan 1989
 * Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include "amoeba.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "string.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "stats.h"
#include "bs_protos.h"
#include "grp_defs.h"

extern	Superblk	Superblock;
extern	Statistics	Stats;
extern	b_fsize		Blksizemin1;
extern	Inode *		Inode_table;
extern	peer_bits *	Inode_reptab;

/*
 * NEW_FILE
 *
 *	This routine allocates the necessary things to build a cache slot
 *	for a new file.  The cache slot returned is locked!
 */

Cache_ent *
new_file()
{
    Cache_ent *	cp;
    Inodenum	inode;

    /* Get an inode - it is locked when we get it. */
    if ((inode = alloc_inode()) < 0)
	return 0;

    /* Get a cache slot */
    if ((cp = alloc_cache_slot(inode)) == 0)
    {
	free_inode(inode,0,ANN_NEVER,SEND_CANWAIT); /* unlocks the inode */
	return 0;
    }

    cp->c_flags |= NOTCOMMITTED;
    cp->c_timeout = BS_CACHE_TIMEOUT;
    return cp;
}


/*
 * DISPOSE_FILE
 *
 *	Releases all of the resources allocated by new_file().
 *	Used when a subsequent part of the file creation fails and the
 *	resources of an uncommitted file must be released.
 */

void
dispose_file(cp)
Cache_ent *	cp;
{
    Inodenum	inode;

    inode = cp->c_innum;
    assert(inode_locked_by_me(inode) == 2);
    destroy_cache_entry(cp);
    free_inode(inode,0,ANN_NEVER,SEND_CANWAIT); /* unlocks inode */
}

/*
 * LOAD_CACHE
 * Try to get file information into cache.
 * If present, read from local disk. If that fails, and one can defer
 * try to read from another member.
 * If not present, can defer should be on, and try to read remote.
 */

static errstat
load_cache(orig_cp,can_defer,addr,size)
Cache_ent * orig_cp;
int	    can_defer;	/* Can read from other members */
bufptr	    addr;
b_fsize	    size;
{
    errstat	err;
    disk_addr   numblocks ;

    if ( orig_cp->c_inode.i_flags&I_PRESENT ) {
	Stats.i_readdata++;
	numblocks = NUMBLKS(size);
	err = bs_disk_read(orig_cp->c_inode.i_dskaddr, addr, numblocks);
	if (err!=STD_OK ) {
	    bwarn("read data error %s, blocks %ld..%ld",err_why(err),
		orig_cp->c_inode.i_dskaddr,
		orig_cp->c_inode.i_dskaddr+numblocks-1);
	    if ( can_defer ) {
		err = bs_member_read(orig_cp->c_innum, &orig_cp->c_inode,
				addr, size);
	    }
	}
    } else {
	/* This check prevents loops between servers */
	if ( !can_defer ) return STD_SYSERR ;
	err = bs_member_read(orig_cp->c_innum, &orig_cp->c_inode, addr, size) ;
    }
    return err;
}

/*
 * BS_LOADFILE
 *
 *	Internal routine for bullet server to load a file from disk
 *	into the buffer cache.
 */

errstat
bs_loadfile(cp,can_defer)
Cache_ent * cp;
int	    can_defer;	/* Can read from other members */
{
    errstat	err;
    disk_addr	numblocks;
    b_fsize	numbytes;

    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(cp->c_addr == 0);

    numblocks = NUMBLKS(cp->c_inode.i_size);
    numbytes = numblocks << Superblock.s_blksize;

    if (alloc_cache_mem(cp, numbytes) != STD_OK)
	return STD_NOMEM;

    err= load_cache(cp,can_defer,cp->c_addr,cp->c_inode.i_size) ;

    if (err != STD_OK)
    {
	cp->c_bufsize -= free_cache_mem(cp->c_addr, cp->c_bufsize);
	cp->c_addr = 0;
    }
    return err;
}


/*
 * CLONE_FILE
 *
 *	Makes a copy of the specified file and returns a pointer to it.
 *	The copy is uncommitted and has newsize bytes of cache allocated!
 *	Guarantees that the original inode is unlocked at completion.
 *	New inode is locked if all went well.
 */

errstat
clone_file(cpp, newsize)
Cache_ent **	cpp;		/* addr of pointer to original file */
b_fsize		newsize;	/* size buffer cache to allocate for clone */
{
    register Cache_ent *	orig;
    register Cache_ent *	new;
    Cache_ent *			new_inmem;
    errstat	err;

    orig = *cpp;
    assert(inode_locked_by_me(orig->c_innum));
    assert(newsize >= orig->c_inode.i_size);

    /* Allocate new resources */
    err= bs_new_inode(&new_inmem) ;
    new=new_inmem ;
    if ( err!=STD_OK ) {
	*cpp = new;
	unlock_inode(orig->c_innum);
	return err ;
    }

    new->c_inode.i_size = newsize;

    /*
     * A quick cheap solution - if the file size won't change and the original
     * is in the cache, steal the cached copy from the original!
     * Don't steal it if it isn't written to disk!
     */
    if (!(orig->c_flags & (WRITEINODE | WRITEDATA)) &&
		newsize == orig->c_inode.i_size && orig->c_addr)
    {
	new->c_addr = orig->c_addr;
	new->c_bufsize = orig->c_bufsize;
	orig->c_addr = 0;
	orig->c_bufsize = 0;
    }
    else
    {
	/* Allocate memory from buffer cache */
	if (alloc_cache_mem(new, newsize) != STD_OK)
	{
	    err = STD_NOMEM;
	}
	else
	{
	    /*
	     * Now we restrict the amount we copy to be no more than
	     * originally existed.
	     */
	    if (newsize > orig->c_inode.i_size)
		newsize = orig->c_inode.i_size;

	    /* Read from disk or copy from memory */
	    if (newsize > 0)
	    {
		if (orig->c_addr) /* already in cache so copy from memory */
		{
		    (void) memmove((_VOIDSTAR) new->c_addr,
					(_VOIDSTAR) orig->c_addr,
					(size_t) newsize);
		}
		else
		{
		    /* Otherwise must read from disk or fellow member */
		    err= load_cache(orig,1,new->c_addr,newsize);
		}
	    }
	}
    }
    unlock_inode(orig->c_innum);
    if (err != STD_OK)
	dispose_file(new);
    else
	*cpp = new;
    return err;
}


/*
 * COMPARE_FILES
 *
 *	Compares files in the cache.  'new' must be in the cache already
 *	and uncommitted, but 'orig' may have to be loaded.
 *	Returns 0 if they are identical and 1 otherwise.
 */

static int
compare_files(new)
Cache_ent *	new;
{
    Cache_ent *	orig;
    Cache_ent * tmp;
    int		result;

    /* DEADLOCK RISK: we are locking two inodes in arbitrary order.
     *                however we do know that 'new' is uncommitted and nothing
     *		      else tries to lock an uncommitted file at the same time
     *		      as a committed file (we hope).
     */
    assert(inode_locked_by_me(new->c_innum));
    assert(new->c_flags & NOTCOMMITTED);

    /*
     * Make sure that the original file still exists and see if it is the same
     * size as the new one.
     */
    if (make_cache_entry(&new->c_cmpprv, BS_RGT_READ, &tmp, L_READ) != STD_OK)
	return 1;
    orig = tmp;
    if (new->c_inode.i_size != orig->c_inode.i_size)
    {
	result = 1;
    }
    else
    {
	b_fsize size;

	/* It doesn't make sense to compare 0 byte files! */
	if ((size = orig->c_inode.i_size) == 0)
	{
	    result = 0;
	}
	else
	{
	    /*
	     * Load the original file into the buffer cache if necessary
	     * and possible
	     */
	    if (orig->c_addr == 0 && bs_loadfile(orig,1) != STD_OK)
		result = 1;
	    else
		result = memcmp((_VOIDSTAR) new->c_addr,
				    (_VOIDSTAR) orig->c_addr, (size_t) size);
	}
    }
    unlock_inode(orig->c_innum);
    return result;
}

/*
 * F_TODISK
 *
 *	This routine marks it as being ready to be written
 *	to disk.  If the "safety" flag is set then it writes the file to
 *	disk immediately.  Otherwise the sweeper thread will write it at a
 *	later time.
 */

errstat
f_todisk(cp, safety)
Cache_ent *	cp;	/* in: cache entry to be written */
int		safety; /* in: whether to write with safety or not */
{
    register b_fsize	size;
    register disk_addr	nblks;	/* # blocks needed to store file data */

    assert(inode_locked_by_me(cp->c_innum) == 2);

    /*
     * Free any unused part of cache memory.
     */
    size = cp->c_inode.i_size;
    CEILING_BLKSZ(size);
    cp->c_bufsize -= free_cache_mem(cp->c_addr+size, cp->c_bufsize-size);
    if (cp->c_bufsize == 0)
    {
        assert(cp->c_inode.i_size == 0);
        cp->c_addr = 0;
    }

    /* Allocate the disk blocks if the file needs any */
    if (cp->c_inode.i_size > 0)
    {
	nblks = NUMBLKS(cp->c_inode.i_size);
	cp->c_inode.i_dskaddr = (disk_addr) a_alloc(A_DISKBLOCKS,
    						    (Res_size) nblks);
	if (cp->c_inode.i_dskaddr == 0)
	{
	    return STD_NOSPACE;
	}
    }

    /*
     *  Mark file as committed and requiring writing.
     *  We can use cp as long as the inode remains locked.
     */
    cp->c_flags &= ~NOTCOMMITTED;
    cp->c_flags |= (WRITEDATA | WRITEINODE);
    cp->c_inode.i_flags |= (I_PRESENT|I_INUSE) ;

    if (safety)
    {
	writefile(cp);
	 /* flush inode to in-core copy and write to disk */
	writecache(cp,SEND_CANWAIT);
    }
    else
    {
	/*
	 * In core inode must reflect the true state for b_size, etc
	 * to work properly
	 */
	Inode * ip;

	ip = Inode_table + cp->c_innum;
	/* Lock the block containing the inode temporarely */
	lock_ino_cache(cp->c_innum) ;
	/* Do not touch ownership, ttl's, version, random */
	ip->i_flags= cp->c_inode.i_flags ;
	ip->i_dskaddr= cp->c_inode.i_dskaddr ;
	ip->i_size= cp->c_inode.i_size ;
	/* The file is not yet present on disk */
	/* reboot must check this flag */
	ip->i_flags |= I_INTENT ;
	ENC_DISK_ADDR_BE(&ip->i_dskaddr);
	ENC_FILE_SIZE_BE(&ip->i_size);
	unlock_ino_cache(cp->c_innum) ;
    }

    Inode_reptab[cp->c_innum] = ((peer_bits)1)<< Superblock.s_member ;

    return STD_OK;
}

/*
 * F_COMMIT
 *
 *	This routine commits a file and marks it as being ready to be written
 *	to disk.  If the "safety" flag is set then it writes the file to
 *	disk immediately.  Otherwise the sweeper thread will write it at a
 *	later time.
 *	If safety is on and replication needed, we could first write the
 *	inode to disk and then do replication and disk write in parallel.
 *	We did not implement that.
 *	If the COMPARE flag is set then it must first compare the file with
 *	the one specified in the cache table entry and if they are the same
 *	then return the capability for the original and destroy the new one.
 *	If they are different then 'priv' already contains the new capability.
 *	Must guarantee that cp->c_innum is unlocked on return.
 *	The meaning of the safety flags depends on the superblock.
 *	If S_SAFE_REP is set in the superblock, we always do a replication
 *	before returning.
 */

errstat
f_commit(cp, safety, priv)
Cache_ent *	cp;	/* in: cache entry to be committed */
int		safety; /* in: whether to commit with safety or not */
private *	priv;	/* out: capability for the file committed */
{
    errstat	err ;
    Inodenum	inode;

    inode= cp->c_innum ;
    assert(inode_locked_by_me(inode) == 2);

    if (cp->c_flags & NOTCOMMITTED)
    {
	/*
	 * See if we must do a compare, and if they are the same return the
	 * original file capability
	 */
	if ((cp->c_flags & COMPARE) && compare_files(cp) == 0)
	{
	    *priv = cp->c_cmpprv;
	    dispose_file(cp);
	    return STD_OK;
	}

	/*
	 * If we got to here then either there was no compare or it failed.
	 * Send the file to disk.
	 */
	err= f_todisk(cp, safety) ;
	if ( err!=STD_OK ) {
	    dispose_file(cp);
	    dbmessage(9,("f_todisk returned error %s",err_why(err)));
	    return err;
	}

	/* Set lifetime to maximum */
	gs_set_ttl(inode) ;

	if ( Superblock.s_def_repl>1 ) {
	    if ( safety && Superblock.s_flags&S_SAFE_REP ) {
		/* If the server somehow crashes during replication, the
		 * capability will have been passed to the requestor.
		 * Thus there is no need to replicate the file when
		 * the server comes back up.
		 */
		err=gs_startrep(cp) ;
		/* NOTFOUND can only occur if inode was destroyed, but
		 * that could not have happend, since we still have not
		 * passed the capability to the requestor.
		 */
		if ( err==STD_NOTFOUND ) {
		    bwarn("inode disappeared during `safety` replication");
		    err=STD_SYSERR ;
		} else if ( err!=STD_OK ) {
		    dbmessage(2,("gs_startrep on %ld returns %s", inode,
					err_why(err)));
		    destroy_inode(inode,0,ANN_DELAY|ANN_MINE,SEND_CANWAIT);
		    return err;
		}
	    } else { /* background replication */
		/* Zero sized files get replicated automagically */
		if ( cp->c_inode.i_size) cp->c_flags |= REPLICATE ;
	    }
	}
	unlock_inode(inode);
	return STD_OK;
    }
    bpanic("f_commit: trying to commit a committed file");
    /*NOTREACHED*/
}
