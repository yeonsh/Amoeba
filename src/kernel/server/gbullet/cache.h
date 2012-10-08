/*	@(#)cache.h	1.1	96/02/27 14:07:08 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CACHE_H__
#define	__CACHE_H__

/*
 *	Bullet server buffer cache structures and related constants.
 *
 *	The buffer cache also caches decoded inodes in the cache slot.
 *	This is to save having a separate cache for inodes.
 *
 *  Author:
 *	Greg Sharp  17-01-89
 *  Modified:
 *      Ed Keizer, 1995 - converted to group bullet server
 */

typedef struct Cache_ent	Cache_ent;	/* cache entry description */

/*
 * c_junior:
 * c_senior:	These are pointers that implement the least recently used list
 *		of cache entries.  Every time a cache entry is accessed it is
 *		put at the head of the list.
 * c_hnext:
 * c_hprev:	These are pointers used for hash table lookup of an inode.
 * c_addr:	The memory address of the cached file.  0 if file is not in
 *		the cache.
 * c_bufsize	The number of bytes in the cache actually allocated to this
 *		cache entry.  Often this will be more than the file size since
 *		cache memory is allocated in multiples of the disk block size.
 * c_innum:	The inode number of the file in the cache. 0 if invalid cache
 *		entry.
 * c_inode:	Decoded copy of the inode for the file.
 * c_cmpprv:	This holds the private part of a capability when doing a
 *		create with compare.
 * c_flags:	This contains useful information such as whether the inode or
 *		the file needs writing.
 * c_timeout:	This count is decremented by the sweeper thread every sweep
 *		time if the cached file is uncommitted.  If the counter reaches
 *		zero the file is destroyed.
 */

struct Cache_ent
{
    Cache_ent *	c_junior;	/* lru list pointer */
    Cache_ent *	c_senior;	/* lru list pointer */
    Cache_ent *	c_hnext;	/* hash table pointer */
    Cache_ent *	c_hprev;	/* hash table pointer */
    bufptr	c_addr;		/* core address of cached file */
    b_fsize	c_bufsize;	/* size of allocated buffer */
    Inodenum	c_innum;	/* inode number of cached file */
    struct Inode_cache	c_inode;	/* decoded copy of inode */
    private	c_cmpprv;	/* cap of original file for create with cmp */
    char	c_flags;	/* for possible values, see below */
    char	c_timeout;	/* time to live for uncommitted file */
};

/* Valid c_flags */
#define	NOTCOMMITTED	0x02	/* set if file is not yet committed */
#define	WRITEDATA	0x04	/* set if file needs writing */
#define	WRITEINODE	0x08	/* set if inode needs writing */
#define	COMPARE		0x10	/* set if must compare on commit */
#define USEDONCE	0x20	/* set if cache entry used once */
#define REPLICATE	0x40	/* set if replication needed */
#define REPLICATE_BUSY	0x80	/* set if replication busy */

/* Size above which we do not cache files presnt on other members */
#define MAXLOCALCACHE	10240	/* 10K */

/*
 * NB.	You can only lock a cache slot if the capability for the file is
 *	not yet known outside the bullet server and the file is uncommitted.
 *	Used by std_copy.
 */

/* routines from cache.c */
#ifndef NDEBUG
void		dump_inode _ARGS(());
Cache_ent *	dump_cache_for_inode _ARGS(());
void		dump_cache_entry _ARGS(());
#endif /* NDEBUG */
Cache_ent *	alloc_cache_slot _ARGS(());
b_fsize		free_cache_mem _ARGS(());
errstat		alloc_cache_mem _ARGS(());
errstat		growbuf _ARGS(());
errstat		make_cache_entry
			    _ARGS((private *, rights_bits, Cache_ent **, int));
void		bs_flush_committed _ARGS((void));
void		cache_init _ARGS(());
void		cache_usage_stats _ARGS(());
bufptr		alloc_buffer _ARGS((b_fsize,int));
Cache_ent *	find_cache_entry _ARGS((Inodenum));
void		destroy_cache_entry _ARGS((Cache_ent *));
errstat		bs_is_stable _ARGS((void));
#if defined(USER_LEVEL)
/*ARGSUSED*/
void		bullet_sweeper _ARGS((char *, int));
#else  /* Kernel version */
void		bullet_sweeper _ARGS((void));
#endif

/* from new_file.c */
Cache_ent *	new_file _ARGS((void));

#endif /* __CACHE_H__ */
