/*	@(#)cache.c	1.1	96/02/27 14:07:06 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * CACHE.C
 *
 *	This file contains the routines for managing the buffer cache.
 *	The cache caches inodes and the file.  The inodes in Inode_table are
 *	encoded in the disk format (big-endian) and the cache keeps a decoded
 *	copy.  Changes to an inode are written through to Inode_table and the
 *	disk .  Changes to the file itself are only written on a commit of the
 *	file.
 *
 *  Author:
 *	Greg Sharp 09-01-89
 *  Modified:
 *	Greg Sharp 13-11-90	Added routine to let disk compaction flush
 *				unwritten committed files to disk.
 *	Greg Sharp 17-01-92	Modified lock granularity to be on inodes.
 *				The locks also imply that any corresponding
 *				cache slot is locked.
 *      Ed Keizer, 1995 - converted to group bullet server
 */

#include "amoeba.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/prv.h"
#include "module/mutex.h"
#include "stdlib.h"
#include "string.h"
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
#include "cache.h"
#include "alloc.h"
#include "stats.h"
#include "bs_protos.h"
#include "preempt.h"
#include "grp_defs.h"


extern	Inode *		Inode_table;
extern	Superblk	Superblock;

extern	int		Initialised;

extern	Statistics	Stats;

/* size of the biggest file the server can buffer */
extern	b_fsize		Largest_file_size;

/* the size of a block in bytes */
extern	b_fsize		Blksizemin1;

/*
 *	The cache is implemented using a hash table with the inode number as
 *	hash key.  It is implemented as an array of pointers to linked lists.
 *	The hash key selects a linked list which either contains the cache
 *	entry or the inode is not in the cache at all.
 *	NB. Htabsize is the size of the hashtable and must be a power of 2.
 *	It is deduced from the amount of available cache.
 *	For aging of cache entries there is an LRU list through the pool of
 *	cache slots.
 */
static Cache_ent **	Hashtab;
static long		Htabsize;
/* Mutex for protecting pathogenic cache interactions */
static mutex		Cache_sema;

#define	HASH(x)		((x) & (Htabsize - 1))

static Cache_ent *	Youngest;		/* head of LRU list */
static Cache_ent *	Oldest;			/* tail of LRU list */

static Cache_ent *	Cachetab;		/* array of cache slots */
static long		Num_cache_slots;	/* size of array */


/* Definitions indication the type of victim to select */
#define NEED_BUFFERS	0
#define NEED_SLOTS	1

/*
 * CACHE_INIT
 *
 *	Allocate some cache slots near the beginning of the buffer cache
 *	according to how big the buffer cache is.
 *	Then initialise the LRU list.
 *	Then initialise hash table.
 */

void
cache_init()
{
    Cache_ent *	cp;			/* pointer to cache slot pool */
    long	i;			/* loop counter */
    Res_size	cache_bytes;		/* size of cache buffer required */

    mu_init(&Cache_sema);

    /* We
    /* Allocate memory for cache slots from buffer cache memory */
    i = a_notused(A_CACHE_MEM);
    if (i < BS_MIN_MEMORY)
    {
	message("Only 0x%lx bytes cache available.  Minimum needed is 0x%lx",
							    i, BS_MIN_MEMORY);
	bpanic("Buffer cache is too small for serious bullet server.");
    }

    /*
     * The number of cache slots is twice what we would be necessary if all
     * files were of average size.  This is partly because lots of files take
     * up a cache slot even when the file is not in core because for many
     * operations only the inode is required.
     */
    if (AVERAGE_FILE_SIZE == 0)
	bpanic("Configuration error in AVERAGE_FILE_SIZE");
    Num_cache_slots = i / (AVERAGE_FILE_SIZE / 2);
    if (Num_cache_slots < BS_MIN_CACHE_SLOTS)
	Num_cache_slots = BS_MIN_CACHE_SLOTS;

    /*
     * We make it so that the average linked list length is no more than
     * 4 entries.  Then we round Htabsize up to a power of two.
     */
    Htabsize = Num_cache_slots / 4;
    for (i = 1; i < Htabsize; i <<= 1)
	/* do nothing */;
    Htabsize = i;
    Hashtab = (struct Cache_ent **)
			calloc((size_t) Htabsize, sizeof (struct Cache_ent *));
    if (Hashtab == 0)
	bpanic("Cannot allocate hash table for cache.");

    /* Round up size to nearest disk block size */
    cache_bytes = Num_cache_slots * sizeof (Cache_ent);
    CEILING_BLKSZ(cache_bytes);

    /* Allocate the memory */
    cp = (Cache_ent *) a_alloc(A_CACHE_MEM, cache_bytes);
    if (cp == 0)
	bpanic("Cannot allocate cache table slots.");
    Cachetab = cp;

    /* Initialise LRU list & a few other fields */
    Youngest = cp;
    for (i = 0; i < Num_cache_slots; cp++, i++)
    {
	cp->c_junior = cp - 1;
	cp->c_senior = cp + 1;
	cp->c_innum = 0;	/* not in use */
    }

    /* Correct the end pointers of the list */
    Youngest->c_junior = 0;
    Oldest = cp - 1;
    Oldest->c_senior = 0;

    /* Initialise hash table to empty */
    for (i = 0; i < Htabsize; i++)
	Hashtab[i] = 0;
}


/*
 * FIND_CACHE_ENTRY
 *
 *	Search the cache for the slot corresponding to the given inode
 *	Return pointer to slot or NULL if not present
 *	Note, we don't try to lock any inodes during the search since no
 *	one can modify the cache while we are busy.
 */

Cache_ent *
find_cache_entry(i)
Inodenum	i;
{
    register Cache_ent * cp;

    assert(inode_locked_by_me(i));
    mu_lock(&Cache_sema); /* Mustn't traverse pointers without the lock */
    for (cp = Hashtab[HASH(i)]; cp != 0 && cp->c_innum != i; cp = cp->c_hnext)
	/* do nothing */;
    mu_unlock(&Cache_sema);
    return cp;
}


/*
 * HEAD_LRU
 *
 *	Moves the specified cache entry to the head of the LRU list.
 *	It assumes that the cache entry is already in the list!
 */

static void
head_lru(cp)
Cache_ent *	cp;	/* pointer to slot to move to front of list */
{
    /* assert: Cache_sema is locked */assert(Cache_sema != 0);
    /* First remove cp from the list if necessary */
    if (cp->c_junior)			/* then not at front of list! */
	cp->c_junior->c_senior = cp->c_senior;
    else
	return;				/* already at front of list */

    if (cp->c_senior)			/*then not at end of list */
	cp->c_senior->c_junior = cp->c_junior;
    else
	Oldest = Oldest->c_junior;	/* remove from end of list */

    /* Now put it on the front of the list */
    cp->c_junior = 0;
    cp->c_senior = Youngest;
    Youngest->c_junior = cp;
    Youngest = cp;
}


/*
 * TAIL_LRU
 *
 *	Moves the specified cache entry to the tail of the LRU list.
 *	It assumes that the cache entry is already in the list!
 */

static void
tail_lru(cp)
Cache_ent *	cp;	/* pointer to slot to move to end of list */
{
    /* assert: Cache_sema is locked */assert(Cache_sema != 0);
    /* First remove cp from the list if necessary */
    if (cp->c_senior)			/* then not at end of list */
	cp->c_senior->c_junior = cp->c_junior;
    else
	return;				/* already at end of list */

    if (cp->c_junior)			/* then not at front of list! */
	cp->c_junior->c_senior = cp->c_senior;
    else
	Youngest = Youngest->c_senior;	/* remove from front of list */

    /* Now put it on the end of the list */
    cp->c_senior = 0;
    cp->c_junior = Oldest;
    Oldest->c_senior = cp;
    Oldest = cp;
}


/*
 * FREE_CACHE_SLOT
 *
 *	This function marks a cache slot as being no longer in use, puts it
 *	on the end of the LRU list and takes it out of the hash table.
 *	Note that cp is not necessarily locked at this point!
 */

static void
free_cache_slot(cp)
Cache_ent *	cp;	/* pointer to cache slot to be freed */
{
    /* assert: Cache_sema is locked */assert(Cache_sema != 0);
    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(cp->c_addr == 0);
    assert(cp->c_bufsize == 0);

    tail_lru(cp);

    /* Take it out of the hash table */
    if (cp->c_hnext)			/* not at end of linked list */
	cp->c_hnext->c_hprev = cp->c_hprev;
    if (cp->c_hprev)
	cp->c_hprev->c_hnext = cp->c_hnext;
    else				/* at front of linked list */
	Hashtab[HASH(cp->c_innum)] = cp->c_hnext;
    cp->c_hprev = 0;
    cp->c_hnext = 0;
    cp->c_innum = 0;
    cp->c_flags = 0;
}

/* Select a victim to evict from the cache.
 * This routine is entered with Cache_sema locked and returns with
 * Cache_sema unlocked. If it returns a non-zero cp, the inode
 * belonging to that entry is locked and `clean'. No further disk
 * writes will be neccesary.
 */

static Cache_ent *
select_victim(start_cp,junior_cpp,needs,mode)
	Cache_ent	*start_cp;
	Cache_ent	**junior_cpp;
	int		needs;
	int		mode;
{
	register Cache_ent	*cp;
	Cache_ent *		r_cp;	/* Slot needing replication */
	Cache_ent *		rb_cp;	/* Slot written, but needing replication */
	Inodenum		victim;
	/*
	 * Look for a committed, unlocked file to evict from the cache.
	 * Files busy replicating are considered to be locked.
	 * The following criteria are used for candidates to evict:
	 * 1 - first committed, replicated files
	 * 2 - then files, written to disk, for which replication failed
	 * 3 - then files, not written to disk, needing replication.
	 */
	r_cp= rb_cp= (Cache_ent *)0 ;
	DISABLE_PREEMPTION;
	for ( cp=start_cp ; cp != (Cache_ent *) 0 ; cp = cp->c_junior ) {
	    if ( cp->c_innum==0 ||
		 (cp->c_flags & (NOTCOMMITTED|REPLICATE_BUSY)) ) {
		continue ;
	    }
	    if ( inode_locked(cp->c_innum) ) continue ;
	    if ( needs==NEED_BUFFERS && cp->c_bufsize==0 ) continue ;
	    if ( cp->c_flags & REPLICATE ) {
		if ( cp->c_flags & (WRITEINODE|WRITEDATA) ) {
		    if ( !rb_cp ) rb_cp=cp ;
		} else {
		    if ( !r_cp ) r_cp=cp ;
		}
	    } else {
		break;
	    }
	}

	if ( cp == (Cache_ent *) 0 ) {
	    /* Nothing of first choice, take second or third */
	    if ( r_cp ) cp=r_cp ; else if ( rb_cp ) cp=rb_cp ; else
	    {
	    	/* Eeek! No uncommitted slot */
	        ENABLE_PREEMPTION;
	        mu_unlock(&Cache_sema);
		if ( needs==NEED_BUFFERS ) {
		    bwarn("No free cache buffers");
		} else {
	            bwarn("No free cache slot");
		}
	        return 0;
	    }
	}
	/* evict cp from the cache */
	victim = cp->c_innum;
	if ( junior_cpp ) *junior_cpp= cp->c_junior ;

	/* We have to lock it exclusively while we trash the cache entry. */
	/* Note that the victim is guaranteed not to be locked */
	mu_unlock(&Cache_sema);
	lock_inode(victim, L_WRITE);
	ENABLE_PREEMPTION;

	if (cp->c_flags & REPLICATE && !(cp->c_inode.i_flags&I_BUSY) ) {
	    /* Mark the inode in core as still needing
	     * replication.
	     */
	    cp->c_inode.i_flags |= I_BUSY ;
	    cp->c_flags |= WRITEINODE ;
	    gs_lostrep();
	}
	if (cp->c_flags & WRITEDATA)
	    writefile(cp);
	if (cp->c_flags & WRITEINODE)
	    writecache(cp,mode);
	assert(!(cp->c_flags & (WRITEINODE | WRITEDATA)));
	return cp ;
}

/*
 * ALLOC_CACHE_SLOT
 *
 *	This function allocates a cache slot by taking the least recently
 *	used slot from the list.  If the slot is still in use then it frees it
 *	before proceeding.  It caches the inode and returns a pointer to the
 *	allocated cache slot.  Returns null pointer if no slot could be freed.
 */

Cache_ent *
alloc_cache_slot(inode)
Inodenum	inode;
{
    Cache_ent *		cp;	/* Usable slot */
    Cache_ent **	hp;	/* pointer to hash table entry */

    assert(inode_locked_by_me(inode));
    /* assert : there is no cache slot for this inode already */

    /* DEADLOCK RISK: locking two inodes, which is avoided by
     * making sure that we never lock the 2nd inode without knowing
     * that it is free
     */
    mu_lock(&Cache_sema);

    cp = Oldest;

    if (cp->c_innum != 0)
    {
	Inodenum    deadi ;
	cp= select_victim(cp,(Cache_ent **)0,NEED_SLOTS,SEND_CANWAIT) ;
	if ( !cp ) return 0 ;
	if ( cp->c_bufsize) {
	    cp->c_bufsize -= free_cache_mem(cp->c_addr, cp->c_bufsize);
	}
	assert(cp->c_bufsize == 0);
	cp->c_addr = 0;
	deadi= cp->c_innum ;
	mu_lock(&Cache_sema);
	free_cache_slot(cp);
	unlock_inode(deadi);
    }
    /* assert:Cache_sema locked */assert(Cache_sema != 0);
    cp->c_addr = 0;
    cp->c_bufsize = 0;
    cp->c_innum = inode;
    cp->c_inode = *(struct Inode_cache *)(Inode_table + inode);
    cp->c_flags |= USEDONCE ;

    /* Unpack inode */
    DEC_DISK_ADDR_BE(&cp->c_inode.i_dskaddr);
    DEC_FILE_SIZE_BE(&cp->c_inode.i_size);
    DEC_VERSION_BE(&cp->c_inode.i_version);

    head_lru(cp);

    /* Put into hash table at front of linked list */
    hp = &Hashtab[HASH(inode)];
    cp->c_hprev = 0;
    cp->c_hnext = *hp;
    if (*hp)
	(*hp)->c_hprev = cp;
    *hp = cp;
    mu_unlock(&Cache_sema);

    return cp;
}


/*
 * SCROUNGE_MEMORY
 *
 *	This attempts to recover enough memory to allocate a buffer by evicting
 *	cache entries of committed files in lru order.  Just to be sure it
 *	chucks out 2*size bytes of files if it can.
 *
 * XXX: This is somewhat naive since releasing N files of size 'size'*2/N will
 *      not necessarily give us a single chunk that we can allocate.
 *	It is also risky since freeing the Cache_sema while we write to disk
 *	may allow someone else to try to scrounge memory.
 */

static void
scrounge_memory(size,mode)
register b_fsize	size;
int			mode;
{
    register Cache_ent *	cp;	/* current cache entry */
    register Cache_ent *	nextcp;
    Cache_ent *			juniorcp;

    size <<= 1;
    if (size > Largest_file_size)
	size = Largest_file_size;

    mu_lock(&Cache_sema);

    /* Start at least recently used slot */
    nextcp = Oldest;
    do
    {
	cp= select_victim(nextcp,&juniorcp,NEED_BUFFERS,mode) ;
	if ( !cp ) return ;

	mu_lock(&Cache_sema);
	nextcp = juniorcp;
	if ( cp->c_bufsize ) {
	    /* Free the memory associated with this cache slot */
	    size -= cp->c_bufsize;
	    cp->c_bufsize -= free_cache_mem(cp->c_addr, cp->c_bufsize);
	    assert(cp->c_bufsize == 0);
	    cp->c_addr = 0;
	}
	unlock_inode(cp->c_innum);
	/*
	 * Someone might have restructured the lru while we were
	 * playing. Therefore we go back to "Oldest".  In theory this
	 * can lead to an infinite loop but almost certainly won't in
	 * practice.
	 */
	nextcp = Oldest;
	/* At end of list, cp == 0 == Youngest.c_junior */
    } while (nextcp != 0 && size > 0);
    mu_unlock(&Cache_sema);
}


/*
 * CACHE_COMPACT
 *
 *	This routine serves two important purposes.  The first is that it
 *	attempts to assimilate all the free cache memory into a single chunk
 *	so that the chances of a large allocation request being satisfied are
 *	increased.  The second is that it reduces the number of holes in use
 *	by the allocator system which reduces the chance of it running out.
 *	It works by going through the list of holes in the cache and copying
 *	data down into holes in the cache until everything is as low in
 *	memory as possible.
 *	Alas it doesn't take cache entries in memory address order so it may
 *	take several calls to this routine to get good compaction.
 *	NB: It is assumes that the caller has not locked the cache!
 */

static void
cache_compact()
{
    register int		i;	/* hash table index */
    register Cache_ent *	cp;	/* pointer to cache_table entry */
    Res_addr			hp;	/* pointer to hole */
    Res_size			hsize;	/* size of hole */

    Stats.i_compact++;

    mu_lock(&Cache_sema);

    /*
     * For each cache entry see if we can find a lower memory address where it
     * can live. 
     * Stop looking if we find there are zero or one holes before we have moved
     * everyone down since compaction cannot be improved.
     */
    for (cp = Cachetab, i = 0; i < Num_cache_slots; cp++, i++)
    {
	if (cp->c_innum != 0) /* Cache slot is in use */
	{
	    DISABLE_PREEMPTION;
	    if (inode_locked(cp->c_innum))
	    {
		/* We mustn't move it & we can't wait for it to be free */
		ENABLE_PREEMPTION;
		continue;
	    }

	    lock_inode(cp->c_innum, L_WRITE);
	    if (cp->c_addr == 0)
	    {
		/* No buffer is allocated */
		unlock_inode(cp->c_innum);
		ENABLE_PREEMPTION;
		continue;
	    }

	    assert(cp->c_bufsize != 0);

	    /* If <= 1 holes left */
	    if (com_firsthole(A_CACHE_MEM, &hp, &hsize, (Res_addr) 0) <= 0)
	    {
		unlock_inode(cp->c_innum);
		mu_unlock(&Cache_sema);
		ENABLE_PREEMPTION;
		return;
	    }

	    do
	    {
		assert(hp != 0);
		assert(hsize != 0);

		/*
		 * If the free piece of memory is lower than the buffer and the
		 * free piece is bigger than the buffer or contiguous with the
		 * buffer then we move it. However com_first_hole & com_nexthole
		 * return addresses in ascending order so, as soon as we pass
		 * the cache_entry address we can stop looking.
		 */
		if ((char *) hp > cp->c_addr)
		    break;
		if (hsize >= cp->c_bufsize || (char *) hp + hsize == cp->c_addr)
		{
		    if (a_used(A_CACHE_MEM, hp, hsize) != 0)
			bpanic("cache_compact: inconsistent free list");
		    (void) memmove((_VOIDSTAR) hp, (_VOIDSTAR) cp->c_addr,
							(size_t) cp->c_bufsize);
		    /* If no overlap between old buffer and new buffer ... */
		    if ((long) hsize - cp->c_bufsize >= 0)
		    {
			(void) free_cache_mem((char *) hp+cp->c_bufsize,
						(b_fsize) hsize-cp->c_bufsize);
			(void) free_cache_mem(cp->c_addr, cp->c_bufsize);
		    }
		    else
			(void) free_cache_mem((char *) hp+cp->c_bufsize,
							    (b_fsize) hsize);
		    cp->c_addr = (char *) hp;
		    break; /* go to next cache entry */
		}
	    } while (com_nexthole(A_CACHE_MEM, &hp, &hsize) == 1);

	    ENABLE_PREEMPTION;

	    unlock_inode(cp->c_innum);
 
            /* Alas, compaction can take a long time - so let interrupts run */
            threadswitch();

	}
    }
    mu_unlock(&Cache_sema);
}

/*
 * MAKE_CACHE_ENTRY
 *
 *	Given a capability for a file and the minimum rights required for
 *	the intended operation, this routine validates the capability.
 *	If the capability is valid and has sufficient rights the file is
 *	touched (to prevent it from being garbage collected) and a pointer
 *	to the cache slot of the file is returned.  A cache slot is allocated
 *	and the inode cached if the file didn't have a slot already.
 *	If the capability is invalid or there are insufficient rights then
 *	a null pointer is returned.
 *	NB: If a read lock was requested but the file is not in core then
 *	    it must be converted to a write lock so that the file can be
 *	    loaded, if necessary.
 */

errstat
make_cache_entry(priv, min_rights, res_p, locktype)
private *	priv;		/* capability to be checked */
rights_bits	min_rights;	/* minimum rights for valid capability */
Cache_ent **	res_p;		/* pointer to cache entry made/found */
int		locktype;
{
    register Cache_ent *cp;	/* pointer to cache entry in linked list */
    Inodenum		inode;
    errstat		err;

    assert(locktype != L_NONE);

#if L_READ!=L_WRITE
RETRY:
#endif
    /* See if the capability is valid */
    if ((err = validate_cap(priv, min_rights, &inode, locktype, BS_PUBLIC)) != STD_OK)
	return err;

    /* See if the inode is in the cache */
    cp = find_cache_entry(inode);

    if (((Inode_table + inode)->i_flags&I_INUSE) == 0) /* uncommitted file */
	assert(cp != 0); /* uncommitted file must be in cache! */

    if (cp == 0)
    {
	/* Not in cache. Add new entry in hash table at front of linked list */
	if ((cp = alloc_cache_slot(inode)) == 0)
	    return STD_NOSPACE;
	Stats.i_cache_miss++;
    }
    else {
	Stats.i_cache_hit++;
	cp->c_flags &= ~USEDONCE ;
    }
    
    assert(cp != 0);


#if L_READ!=L_WRITE
    message("Old code has been automatically reinstated");
    if (cp->c_addr == 0 && locktype == L_READ)
    {
	/* Convert to a write lock in case file must be loaded */
	unlock_inode(inode);
	locktype = L_WRITE;
	goto RETRY;
    }
#endif

    *res_p = cp;
    return STD_OK;
}


/*
 * ALLOC_BUFFER
 *
 *	Called to allocate request buffers for threads and by alloc_cache_mem
 *	This code is really ugly but alas there is a design problem in here.
 */

bufptr
alloc_buffer(size,mode)
b_fsize	size;
int mode;
{
    Res_addr	result;
    int		i;

    if (size == 0 || size > Largest_file_size)
	return 0;

    /* Round up size to nearest disk block size */
    CEILING_BLKSZ(size);

    /* Allocate the memory */
    if ((result = a_alloc(A_CACHE_MEM, (Res_size) size)) == 0)
    {
	for (i = 0; i < 2 && result == 0; i++)
	{
	    scrounge_memory(size,mode);	/* try to get the memory */
	    if ((result = a_alloc(A_CACHE_MEM, (Res_size) size)) == 0)
	    {
		cache_compact();	/* getting desperate now */
		cache_compact();
		result = a_alloc(A_CACHE_MEM, (Res_size) size);
	    }
	}
    }
    return (bufptr) result;
}


/*
 * ALLOC_CACHE_MEM
 *
 *	Allocates "size" bytes of cache memory, rounded up to the nearest
 *	multiple of the disk block size.  It assigns the memory to the
 *	cache_slot "cp".  It returns the error status.
 *	c_bufsize must not be altered if nothing is allocated.
 */

errstat
alloc_cache_mem(cp, size)
register Cache_ent *	cp;	/* pointer to cache_slot to assign memory to */
register b_fsize	size;	/* # bytes to allocate */
{
    assert(inode_locked_by_me(cp->c_innum) == 2); /* Must have a write lock */

    /* Round up size to nearest disk block size */
    CEILING_BLKSZ(size);
    assert((size & ((unsigned long) Blksizemin1)) == 0);

    /* Allocate the memory */
    if (size == 0)
	cp->c_addr = 0;
    else
	if ((cp->c_addr = alloc_buffer(size,SEND_CANWAIT)) == 0)
	    return STD_NOMEM;
    cp->c_bufsize = size;
    assert(((long)(cp->c_addr) & ((unsigned long) Blksizemin1)) == 0);
    return STD_OK;
}


/*
 * FREE_CACHE_MEM
 *
 *	Restores to the free list some piece of previously allocated memory.
 *	It returns the number of bytes actually freed for callers who are
 *	releasing only a part of their allocation.  This is an hysterical
 *	raisin because the routine formerly accepted unaligned frees by
 *	rounding them to a the nearest block size.
 * NOT for the fainthearted:
 *	This routine has a rather tenuous grip on correctness.  It doesn't
 *	go wrong when someone tries to free a `blocksize' sized chunk of
 *	non-aligned memory because nobody does that.  This is because they
 *	all allocate `blocksize' multiples and they release `blocksize'
 *	multiples.  Keep it that way or the panic will bite you.
 */


b_fsize
free_cache_mem(ptr, size)
bufptr		ptr;	/* address from which freeable memory begins */
b_fsize		size;	/* size of freeable area */
{
    if (size == 0 || ptr == 0)
	return 0;			/* nothing to free */

    assert(size > 0);
    assert((size & ((unsigned long) Blksizemin1)) == 0);
    assert((((unsigned long) ptr) & ((unsigned long) Blksizemin1)) == 0);

    if (a_free(A_CACHE_MEM, (Res_addr) ptr, (Res_size) size) < 0)
	bpanic("free_cache_mem: a_free failed");
    return size;
}


/*
 * GROWBUF
 *
 *	This routine has to increase the size of the buffer available to a
 *	cache slot.
 */

errstat
growbuf(cp, newsize)
Cache_ent *	cp;
b_fsize		newsize;
{
    errstat	status;
    Cache_ent	tmp;
    b_fsize	double_buf;

    assert(inode_locked_by_me(cp->c_innum) == 2); /* Write locked */

    if (newsize < cp->c_bufsize)	/* buffer is shrinking! */
	return STD_ARGBAD;
    /*
     * To try to reduce copying, if someone tries to grow a buffer we try to
     * double it for them so that they won't change buffers so much.  If we
     * can't get double we just try for what they need.
     */
    double_buf = cp->c_bufsize << 1;
    if (double_buf < newsize) /* This should also catch -ve values! */
	double_buf = newsize;

    tmp.c_innum = cp->c_innum;
    /* Allocate a new buffer of the required size */
    if ((status = alloc_cache_mem(&tmp, double_buf)) != STD_OK &&
			(status = alloc_cache_mem(&tmp, newsize)) != STD_OK)
	return status;

    /* See if the new chunk is contiguous with the old - if so  join them! */
    if (cp->c_addr + cp->c_bufsize == tmp.c_addr)
	cp->c_bufsize += tmp.c_bufsize;
    else
    {
	/* Copy the original data to the new buffer */
	(void) memmove((_VOIDSTAR) tmp.c_addr, (_VOIDSTAR) cp->c_addr,
						(size_t) cp->c_inode.i_size);

	/* Free all of the old buffer and assign the new one to the slot */
	(void) free_cache_mem(cp->c_addr, cp->c_bufsize);
	cp->c_addr = tmp.c_addr;
	cp->c_bufsize = tmp.c_bufsize;
    }
    return STD_OK;
}


/*
 * DESTROY_CACHE_ENTRY
 *
 *	Free all the resources associated with the cache slot specified,
 *	including the cache slot itself.
 */

void
destroy_cache_entry(cp)
Cache_ent *	cp;
{
    assert(inode_locked_by_me(cp->c_innum) == 2);
    if (cp->c_addr)
    {
	cp->c_bufsize -= free_cache_mem(cp->c_addr, cp->c_bufsize);
	cp->c_addr = 0;
    }
    mu_lock(&Cache_sema);
    free_cache_slot(cp);
    mu_unlock(&Cache_sema);
}


/*
 *	CACHE_USAGE_STATS
 *
 *	This routine counts the number of bytes of cache memory used for
 *	a) committed files
 *	b) internal fragmentation in committed files
 *	c) committed files waiting to be replicated
 *	d) uncommitted files
 *	e) internal fragmentation in uncommitted files
 *	f) # free slots
 *	g) # slots waiting for replication
 *	It is used by the status command.  In theory it shouldn't look at
 *	things that are write-locked by others but the stats are just a
 *	rough guide.
 */

void
cache_usage_stats(committed, comm_frag, comm_rep, uncommitted, uncomm_frag,
			slots, slots_rep)
b_fsize *	committed;	/* #bytes cache in use for committed files */
b_fsize *	comm_frag;	/* #bytes fragmentation */
b_fsize *	comm_rep;	/* #bytes waiting for replication */
b_fsize *	uncommitted;	/* #bytes cache in use for uncommitted files */
b_fsize *	uncomm_frag;	/* #bytes fragmentation */
b_fsize *	slots;		/* #cache_slots free */
b_fsize *	slots_rep;	/* #cache_slots tied in replication */
{
    int		i;
    Cache_ent *	cp;
    b_fsize	com;
    b_fsize	comf;
    b_fsize	comr;
    b_fsize	uncom;
    b_fsize	uncomf;
    b_fsize	used_slots;
    b_fsize	r_slots;

    /*
     * For each cache entry we calculate # bytes used for committed bytes
     * the number of bytes used for uncommitted files and the internal
     * fragmentation of each.
     */
    uncom = 0;
    uncomf = 0;
    com = 0;
    comf = 0;
    used_slots = 0;
    comr=0;
    r_slots=0;
    mu_lock(&Cache_sema);
    for (i = 0; i < Htabsize; i++)
    {
	for (cp = Hashtab[i]; cp != 0; cp = cp->c_hnext)
	{
	    used_slots++;
	    if (cp->c_flags & REPLICATE) r_slots++ ;
	    /* In some cases the file is not in the cache, only the inode */
	    if (cp->c_addr != 0)
	    {
		if (cp->c_flags & NOTCOMMITTED)
		{
		    uncom += cp->c_bufsize;
		    uncomf += cp->c_bufsize - cp->c_inode.i_size;
		}
		else
		{
		    com += cp->c_bufsize; 
		    comf += cp->c_bufsize - cp->c_inode.i_size;
		    if (cp->c_flags & REPLICATE) comr += cp->c_bufsize;
		}
	    }
	}
    }
    mu_unlock(&Cache_sema);
    *committed = com;
    *comm_frag = comf;
    *uncommitted = uncom;
    *uncomm_frag = uncomf;
    *slots = Num_cache_slots - used_slots;
    *slots_rep = r_slots;
    *comm_rep = comr ;
}

/* Wait until there are no more uncommiteds or to be replicateds in cache
 */
errstat
bs_is_stable()
{
    int		i;
    Cache_ent *	cp;

    mu_lock(&Cache_sema);
    for (i = 0; i < Htabsize; i++)
    {
	for (cp = Hashtab[i]; cp != 0; cp = cp->c_hnext)
	{
	    if (cp->c_flags & (REPLICATE|NOTCOMMITTED)) {
		mu_unlock(&Cache_sema) ;
		return STD_NOTFOUND;
	    }
	}
    }
    mu_unlock(&Cache_sema);
    return STD_OK ;
}

/*
 * BS_FLUSH_COMMITTED
 *
 *	This routine is used by sync to flush files that
 *	were committed without safety.
 */

void
bs_flush_committed()
{
    register int		i;
    register Cache_ent *	cp;

    /*
     * Before we can modify the disk we must ensure that all committed
     * files are out there.
     */

    for (cp = Cachetab, i = 0; i < Num_cache_slots; cp++, i++)
    {
	if (cp->c_innum != 0)
	{
	    lock_inode(cp->c_innum, L_WRITE);
	    if (!(cp->c_flags & NOTCOMMITTED))
	    {
		if (cp->c_flags & REPLICATE && !(cp->c_inode.i_flags&I_BUSY) ) {
		    /* Mark the inode in core as still needing
		     * replication.
		     */
		    cp->c_inode.i_flags |= I_BUSY ;
		    cp->c_flags |= WRITEINODE ;
		    gs_lostrep();
		}
		if (cp->c_flags & WRITEDATA)
		    writefile(cp);
		if (cp->c_flags & WRITEINODE)
		    writecache(cp,SEND_CANWAIT);
	    }
	    unlock_inode(cp->c_innum);
	}
    }
}


/*
 * BULLET_SWEEPER THREAD
 *
 *	This thread wakes up once every BS_SWEEP_TIME milliseconds and goes
 *	through the cache looking for entries that need writing to disk and
 *	timing out uncommitted files.  The should only be one of these threads
 *	running in a bullet server!
 *	It then calculates a load average and if not much is happening or
 *	fragmentation is serious it tries to compact the cache.
 */

#if defined(USER_LEVEL)
/*ARGSUSED*/
void
bullet_sweeper(p, s)
char *  p;
int     s;
#else  /* Kernel version */
void
bullet_sweeper()
#endif
{
    register Cache_ent *	cp;
    register int		i;
    mutex			blocker;
    int				time;
    long			usage[BS_LOAD_BASE];
    errstat			err;

    mu_init(&blocker);
    mu_lock(&blocker);

    time = 0;
    for (;;)
    {
	/* Sleep a while! (since we are already locked we always time out) */
	(void) mu_trylock(&blocker, (interval) BS_SWEEP_TIME);
	bs_check_state();

	/* Take group actions first */
	bs_grp_sweep() ;

	/*
	 * Run through cache decrementing timeouts and looking for files
	 * to write
	 */
	for (cp = Cachetab, i = 0; i < Num_cache_slots; cp++, i++)
	{
	    register Inodenum	inode;

	    if ((inode = cp->c_innum) != 0)
	    {
		/* Cache slot has something in it! */
		lock_inode(inode, L_WRITE);
		/*
		 * Now we must check that the cache slot wasn't changed
		 * while we were waiting for the inode.
		 * If it was then we ignore this cache slot since it is
		 * too much hassle to chase it.  We'll get it next time
		 * if it is still there.
		 */
		if (cp->c_innum == inode) /* We got it */
		{
		    if (cp->c_flags & NOTCOMMITTED)
		    {
			if (--cp->c_timeout == 0)
			{
			    /* unlocks the inode */
			    destroy_inode(inode,1,ANN_NEVER,SEND_CANWAIT);
			    Stats.i_uncomm_tout++;
			}
		    }
		    else	/* see if file needs writing */
		    {
			if (cp->c_flags & REPLICATE ) {
			    /* Mark the inode in core as still needing
			     * replication.
			     */
			    cp->c_inode.i_flags |= I_BUSY ;
			}
			if (cp->c_flags & WRITEDATA) {
			    writefile(cp);
			}
			if (cp->c_flags & WRITEINODE) {
			    writecache(cp,SEND_CANWAIT);
			}
			if (cp->c_flags & REPLICATE ) {
			    err=gs_startrep(cp);
			    switch(err) {
			    case STD_NOTFOUND:
				/* The cache entry changed under our hands*/
				unlock_inode(inode) ;
				continue ;
			    case STD_OK:
				/* Mark the inode in core as not needing
			         * replication. (And in cache)
			         */
			        cp->c_inode.i_flags &= ~I_BUSY ;
				cp->c_flags &= ~REPLICATE ;
			        (Inode_table + cp->c_innum)->i_flags &= ~I_BUSY ;
				break ;
			    default:
			        dbmessage(2,("gs_startrep on %ld returns %s", inode,
					err_why(err)));
				/* Something went wrong. We keep the inode
				 * in the cache. But it is up for grabs.
				 */
				break ;
			    }
			}
		    }
		}
		unlock_inode(inode);
	    }
	}

	/* Sweep out any delayed inode blocks */
	bs_block_flush() ;

	{
	    register long	sum;

	    /* Calculate the load over previous BS_LOAD_BASE sweeps */
	    if (++time == BS_LOAD_BASE)
		time = 0;
	    usage[time] = Stats.i_lastsweep;
	    Stats.i_lastsweep = 0;
	    for (sum = 0, i = 0; i < BS_LOAD_BASE; i++)
		sum += usage[i];

	    /* If not busy or too much fragmentation */
	    if (sum <= BS_LOAD_BASE/4 || a_high_fragmentation(A_CACHE_MEM))
		cache_compact();
	}
    }
}


#ifndef NDEBUG

/*
 * CACHE_CHECK
 *
 *	Check cache consistency.  Assumes cache is locked.
 *	--Guido van Rossum, CWI, 19-Dec-89.
 */

cache_check()
{
    Cache_ent *	cp;
    Cache_ent *	cq;
    Cache_ent *	prev;
    int		i;

    /* Check end point consistency */
    if (Youngest == 0 || Oldest == 0)
    {
	message("Youngest = 0x%lx, Oldest 0x%lx",Youngest, Oldest);
	bpanic("Corrupted LRU list");
	/*NOTREACHED*/
    }
    else
    {
	if (Youngest->c_junior != 0)
	{
	    dump_cache_entry(Youngest);
	    bpanic("Youngest->c_junior != 0");
	    /*NOTREACHED*/
	}
	if (Oldest->c_senior != 0)
	{
	    dump_cache_entry(Oldest);
	    bpanic("Oldest->c_senior != 0");
	    /*NOTREACHED*/
	}
    }

    /* Check LRU chain consistency */
    for (cp = Youngest, prev = 0; cp != 0; prev = cp, cp = cp->c_senior)
    {
	if (cp->c_junior != prev)
	{
	    dump_cache_entry(cp);
	    if (prev != 0)
	    {
		message("prev: ");
		dump_cache_entry(prev);
	    }
	    message("junior 0x%x, prev 0x%x", cp->c_junior, prev);
	    bpanic("inconsistent junior pointer in LRU chain");
	}
    }
    if (prev != Oldest)
    {
	dump_cache_entry(prev);
	message("Oldest: ");
	dump_cache_entry(Oldest);
	bpanic("LRU chain not ending at Oldest");
    }

    /* Check hash table consistency */
    for (i = 0; i < Htabsize; i++)
    {
	for (cp = Hashtab[i], prev = 0; cp != 0; prev = cp, cp = cp->c_hnext)
	{
	    if (cp->c_hprev != prev)
	    {
		dump_cache_entry(cp);
		if (prev != 0)
		{
		    message("prev: ");
		    dump_cache_entry(prev);
		}
		message("hprev 0x%x, prev 0x%x", cp->c_hprev, prev);
		bpanic("inconsistent hprev pointer, i=%d", i);
	    }
	    for (cq = Youngest; cq != 0 && cq != cp; cq = cq->c_senior)
		/* do nothing */ ;
	    if (cq == 0)
	    {
		dump_cache_entry(cp);
		bpanic("Hashtab entry not in LRU chain");
	    }
	}
    }
}


void
dump_cache_entry(cp)
Cache_ent *	cp;
{
    message("CACHE ENTRY: junior 0x%x, senior 0x%x, hnext 0x%x, hprev 0x%x",
			cp->c_junior, cp->c_senior, cp->c_hnext, cp->c_hprev);
    message("  addr 0x%x, bufsize 0x%x, innum 0x%x, flags 0x%x, timeout 0x%x",
	    cp->c_addr, cp->c_bufsize, cp->c_innum, cp->c_flags, cp->c_timeout);
    message("CACHED ");
    dump_inode((Inode *)&cp->c_inode);
    message("REAL ");
    dump_inode(&Inode_table[cp->c_innum]);
}


Cache_ent *
dump_cache_for_inode(inum)
Inodenum inum;
{
    Cache_ent *	cp;

    for (cp = Youngest; cp != 0; cp = cp->c_senior)
    {
	if (cp->c_innum == inum)
	{
	    dump_cache_entry(cp);
	    return cp;
	}
    }
    message("dump_cache_for_inode: inode 0x%x not in cache", inum);
    return 0;
}

void
dump_inode(ip)
Inode *	ip;
{
    disk_addr addr;
    b_fsize size;

    addr = ip->i_dskaddr;
    DEC_DISK_ADDR_BE(&addr);
    size = ip->i_size;
    DEC_FILE_SIZE_BE(&size);
    message("INODE: i_dskaddr 0x%x, i_size 0x%x", addr, size);
    message("       i_random %p, i_owner 0x%x, i_flags 0x%x",
				&ip->i_random, ip->i_owner, ip->i_flags);
}

#endif /* NDEBUG */
