/*	@(#)caching.c	1.5	96/02/27 10:21:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <assert.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"
#include "module/cap.h"
#include "monitor.h"

#include "global.h"
#include "dirfile.h"
#include "main.h"
#include "superfile.h"
#include "sp_impl.h"
#ifndef SOAP_GROUP
#include "intentions.h"
#endif
#include "caching.h"
#include "misc.h"

/*
 * The amount of memory that can be used to store rows is given as
 * a parameter to Soap (default 500).  However, directories are cached
 * as a whole, so the number of cache entries that are needed cannot
 * be determined exactly in advance.
 * What we can do is estimating the everage number of rows per directory.
 * If we estimate it too high, it will mean that we'll run out of
 * cache nodes too soon, so be careful.
 */
#define AVG_ROWS_PER_DIR	6

/* The following define is a wild guess at the memory spilled because
 * of malloc() fragmentation (not including the per-block malloc overhead
 * itself).
 * (Actually, to avoid bootstrapping problems, it has been defined such
 *  that we use about the same memory as the previous version of Soap.)
 */
#define FRAGMENTATION_EXTRA	64

#define MALLOC_EXTRA		8
#define NAME_SIZE		(16 + MALLOC_EXTRA)
#define CAPSET_EXTRA		((2 * sizeof(suite)) + MALLOC_EXTRA)
#define BYTES_PER_ROW		(sizeof(struct sp_row) + NAME_SIZE +\
				 CAPSET_EXTRA + FRAGMENTATION_EXTRA)

static int Max_rows;
int Max_cached_dirs;

#define INVALID_OBJNUM	(-1)

struct cache_info {
    objnum 	       c_obj;	/* object number of cached dir          */
    struct cache_info *c_older;	/* next older cache entry               */
    struct cache_info *c_newer;	/* next newer cache entry               */
    struct cache_info *c_hash;	/* next cache entry with same hash()    */
};

static struct cache_info *Cache_nodes = NULL;
static struct cache_info *Cache_freelist = NULL;

static struct cache_info *Cache_newest = NULL;
static struct cache_info *Cache_oldest = NULL;

#define HASH_BITS	10
#define HASHTAB_SIZE	(1 << HASH_BITS)
static struct cache_info *Hash_table[HASHTAB_SIZE];

#define hash(obj)	((obj) & (HASHTAB_SIZE - 1))

static int cache_initialised = 0;

/*
 * st_dir field of sp_table entries point here when they are cached out.
 * This should be fixed, but some code might still depend on it!
 */
static struct sp_dir cached_out_dir;
#define OUT_OF_CACHE (&cached_out_dir)

#ifdef CHECK_CACHE
static void
print_dir(obj)
objnum obj;
{
    sp_tab *st;
	
    st = &_sp_table[obj];
    message("OUT_OF_CACHE = %lx", OUT_OF_CACHE);
    message("st->st_dir   = %lx", st->st_dir);
    message("st->st_flags = %x", st->st_flags);
    message("st->st_time  = %x", st->st_time_left);
}
#endif

#define is_out_of_cache(obj) ((_sp_table[obj].st_flags & SF_CACHED_OUT) != 0)

int
out_of_cache(obj)
objnum obj;
{
    int cached_out;

    assert(!bad_objnum(obj));

    cached_out = is_out_of_cache(obj);

#ifdef CHECK_CACHE
  {
    sp_tab *st;

    st = &_sp_table[obj];
    if (cached_out != (st->st_dir == OUT_OF_CACHE)) {
	scream("out_of_cache: object %ld has inconsitent flags", obj);
	print_dir(obj);
    }
  }
#endif

    return cached_out;
}

static void
mark_out_of_cache(obj)
objnum obj;
{
    sp_tab *st = &_sp_table[obj];

    st->st_dir = OUT_OF_CACHE;

#ifdef CHECK_CACHE
    if ((st->st_flags & ~SF_STATE) != 0) {
	/* no flags should be present in this case */
	scream("mark_out_of_cache: dir %ld has flags", obj);
	print_dir(obj);
    }
#endif

    st->st_flags = SF_CACHED_OUT;
}

static void
mark_in_cache(obj)
objnum obj;
{
    sp_tab *st = &_sp_table[obj];

    assert(st->st_dir != NULL);
    assert(st->st_dir != OUT_OF_CACHE);

    /* switch to state SF_IN_CORE while retaining the other flags */
    st->st_flags &= ~SF_STATE;
    st->st_flags |= SF_IN_CORE;
}

#define is_in_cache(obj)	((_sp_table[obj].st_flags & SF_IN_CORE) != 0)

int
in_cache(obj)
objnum obj;
{
    int in_core;

    assert(!bad_objnum(obj));

    in_core = is_in_cache(obj);

#ifdef CHECK_CACHE
  {
    sp_tab *st;

    st = &_sp_table[obj];
    if (in_core != ((st->st_dir != NULL) && (st->st_dir != OUT_OF_CACHE))) {
	scream("in_cache: object %ld has inconsistent flags", obj);
	print_dir(obj);
    }
  }
#endif

    return in_core;
}

static void
cache_add_to_freelist(cp)
struct cache_info *cp;
{
    assert(cp != NULL);

    cp->c_obj = INVALID_OBJNUM;
    cp->c_older = Cache_freelist;
    cp->c_newer = NULL;
    cp->c_hash = NULL;
    Cache_freelist = cp;
}

/* Forward: */
static int uncache_dir _ARGS((void));

static struct cache_info *
get_free_cache_node()
{
    struct cache_info *cp;

    if (Cache_freelist == NULL) {
	if (uncache_dir()) {
	    MON_EVENT("uncached directory");
	} else {
	    panic("could not uncache directory");
	}
    }
    assert(Cache_freelist != NULL);

    cp = Cache_freelist;
    Cache_freelist = cp->c_older;
    return cp;
}

static void
cache_link(cp, obj)
register struct cache_info *cp;
objnum obj;
{
    struct cache_info **hash_ptr;

    assert(!bad_objnum(obj));

    /*
     * add it to the hash chain for obj.
     */
    hash_ptr = &Hash_table[hash(obj)];
    cp->c_hash = *hash_ptr;
    *hash_ptr = cp;

    /*
     * add it in front of the lru list
     */
    cp->c_newer = NULL;
    cp->c_older = Cache_newest;
    if (Cache_newest != NULL) {
	assert(Cache_newest->c_newer == NULL);
	Cache_newest->c_newer = cp;
    }
    Cache_newest = cp;
    if (Cache_oldest == NULL) {
	Cache_oldest = cp;
    }
}

static void
cache_add(obj, dir)
objnum obj;
struct sp_dir *dir;
{
    sp_tab *st;
    struct cache_info *cp;

    assert(!bad_objnum(obj));
    st = &_sp_table[obj];

    /* get a new node */
    cp = get_free_cache_node();
    cp->c_obj = obj;
    st->st_dir = dir;
    
    cache_link(cp, obj);
    
    mark_in_cache(obj);
}

static errstat
cache_refresh(obj)
objnum obj;
{
    register struct cache_info *cp;

    assert(Cache_newest != NULL);
    if (Cache_newest->c_obj == obj) {
	/* already in front of the lru-list */
	return STD_OK;
    }

    /* Find the cache entry via the hash chain */
    for (cp = Hash_table[hash(obj)];
	 cp != NULL && cp->c_obj != obj;
	 cp = cp->c_hash)
    {
	/* skip */
    }
    assert(cp != NULL);

    /* Remove it from the lru list */
    assert(cp->c_newer != NULL);
    cp->c_newer->c_older = cp->c_older;
    if (cp->c_older != NULL) {
	cp->c_older->c_newer = cp->c_newer;
    } else {
	Cache_oldest = cp->c_newer;
    }

    /* Add it back in front of the lru list */
    cp->c_newer = NULL;
    cp->c_older = Cache_newest;
    Cache_newest->c_newer = cp;
    Cache_newest = cp;

    return STD_OK;
}

errstat
cache_read(obj)
objnum     obj;
{
    errstat err;

    assert(!bad_objnum(obj));

    if (is_out_of_cache(obj)) {	/* Read back it into cache: */
	capability origcaps[NBULLETS];
	capability filecaps[NBULLETS];
	struct sp_dir *dir = NULL;

	get_dir_caps(obj, origcaps);
	get_dir_caps(obj, filecaps);

	err = dirf_read(obj, filecaps, NBULLETS, &dir);
	if (err == STD_OK) {
#ifndef SOAP_GROUP
	    /* Now check if dirf_read() descovered a bad capability,
	     * and zeroed it.  In that case we try to create an intention
	     * for it, so that the problem gets fixed at both sides.
	     * But only do this when we've fully initialised,
	     * and we have some spare room for the intention.
	     */
	    if (fully_initialised() && intent_avail() > 2) {
		int j;

		for (j = 0; j < NBULLETS; j++) {
		    if (!cap_cmp(&origcaps[j], &filecaps[j])) {
			MON_EVENT("sp_refresh: cleared bad filecap");
			intent_add(obj, filecaps);
			break;
		    }
		}
	    }
#endif
	} else {
	    /* Something went wrong while reading the directory from disk */
	    if (dir == NULL) {
		MON_EVENT("directory read failed");
		scream("cache_read: cannot read dir %ld (%s)",
		       obj, err_why(err));
	    } else {
		/* set a flag? */
		message("cache_read: non fatal error reading dir %ld (%s)",
			obj, err_why(err));
	    }
	}

	if (dir != NULL) {
	    cache_add(obj, dir);
	}
    } else {
	err = cache_refresh(obj);
    }

    return err;
}


static struct cache_info *
cache_unlink(obj)
objnum obj;
{
    register struct cache_info *prevhash, *cp;

    assert(!bad_objnum(obj));

    /*
     * Remove it from the hash chain
     */
    for (prevhash = NULL, cp = Hash_table[hash(obj)];
	 cp != NULL && cp->c_obj != obj;
	 prevhash = cp, cp = cp->c_hash)
    {
	/* skip */
    }

    if (cp == NULL) {
	panic("cached object %ld not on hash chain", obj);
    }

    if (prevhash == NULL) {
	/* We're removing the first one from the hash list */
	Hash_table[hash(obj)] = cp->c_hash;
    } else {
	prevhash->c_hash = cp->c_hash;
    }

    /*
     * Remove it from the lru list
     */
    if (cp->c_newer != NULL) {
	cp->c_newer->c_older = cp->c_older;
    } else {
	Cache_newest = cp->c_older;
    }
    if (cp->c_older != NULL) {
	cp->c_older->c_newer = cp->c_newer;
    } else {
	Cache_oldest = cp->c_newer;
    }

    return cp;
}

/* Indicate that the specified directory is out of the cache (regardless of
 * whether it was currently thought to exist).  If it was in the cache, free
 * its storage and take it off the least-recently-used list:
 */
static void
cache_do_delete(obj)
objnum obj;
{
    struct cache_info *cp;

    assert(!bad_objnum(obj));

    /*
     * Check that the sp_table entry may be removed from the cache
     * (in-cache flag, no pending mods etc.)
     * All of this should probably turn into assert()s when we're
     * finished with this module.
     */
    cp = cache_unlink(obj);
    if (cp == NULL) {
	panic("could not uncache cached dir %ld", obj);
    }

    /*
     * add cache node to the free list
     */
    cache_add_to_freelist(cp);
}

void
cache_delete(obj)
objnum obj;
{
    sp_tab *st = &_sp_table[obj];

    /* when destroying, cache_destroy should be called instead */
    assert((st->st_flags & SF_DESTROYING) == 0);

    if (in_cache(obj)) {
	cache_do_delete(obj);

	free_dir(st->st_dir);
	st->st_dir = NULL;
    }

    mark_out_of_cache(obj);
}

void
cache_destroy(obj)
objnum obj;
{
    sp_tab *st = &_sp_table[obj];

    /* first remove it from the cache hashing structure */
    if (in_cache(obj)) {
	cache_do_delete(obj);
    }

    /* take care of actually freeing the directory, when needed */
    if (st->st_flags & SF_HAS_MODS) {
	/* destroy command was initiatiated from this side */
	sp_free_mods(st);
    } else {
	/* destroy command came from other side */
	if (st->st_dir != NULL && st->st_dir != OUT_OF_CACHE) {
	    free_dir(st->st_dir);
	    st->st_dir = NULL;
	}
    }

    sp_add_to_freelist(st);
}

void
cache_new_dir(obj)
objnum obj;
{
    /*
     * A new in-core dir was created and added to _sp_table;
     * now update the cache to reflect this.
     */
    sp_tab *st = &_sp_table[obj];
    
    assert(st->st_dir != NULL);
    assert((st->st_flags & SF_STATE) == SF_IN_CORE);

    cache_add(obj, st->st_dir);
}

static int
uncache_dir()
/*
 * Throw the oldest entry out of cache that doesn't have pending changes.
 * The newest entry is also left untouched.
 *
 * TODO: directories not having a good Bullet file for it should be marked
 * as such, so that they don't get cached out until we really have no
 * other option (we will always need a few free cache entries to satisfy
 * requests concerning other directories).
 */
{
    register struct cache_info *cp, *cp_next;

    if (Cache_oldest != NULL) {
	assert(Cache_newest != NULL);

	for (cp = Cache_oldest, cp_next = cp->c_newer;
	     cp != Cache_newest;
	     cp = cp_next, cp_next = cp->c_newer)
	{
	    objnum obj = cp->c_obj;

#ifdef CHECK_CACHE
	    if (!in_cache(obj)) {
		scream("uncache_dir: dir %ld not in cache ?!", obj);
		print_dir(obj);
		continue;
	    }
#endif

#ifdef SOAP_GROUP
	    /* Don't throw out directories with mods on the modlist.
	     * Otherwise we would later on cache in the unmodified version!
	     */
	    if (ml_has_mods(obj)) {
		continue;
	    }
#endif
	    
	    /* don't throw out cache entries with pending modifications */
	    if ((_sp_table[obj].st_flags & SF_HAS_MODS) == 0) {
		cache_delete(obj);
		return 1;
	    }
	}
    }

    /* couldn't uncache a directory */
    return 0;
}

void
make_room(nrows)
int nrows;
{
    while (nrows > (Max_rows - sp_allocated_rows())) {
	if (uncache_dir()) {
	    MON_EVENT("uncached directory for rowspace");
	} else {
	    MON_EVENT("make_room aborted");
	    break;
	}
    }
}

int
get_time_to_live(obj)
objnum obj;
{
    assert(!bad_objnum(obj));

    return _sp_table[obj].st_time_left;
}

void
set_time_to_live(obj, time)
objnum obj;
int time;
{
    assert(!bad_objnum(obj));

    _sp_table[obj].st_time_left = time;
}

/*
 * Reset time-to-live fields of all known dirs
 */
void
refresh_all()
{
    objnum obj;

    for (obj = 1; obj < _sp_entries; obj++) {
	if (sp_in_use(&_sp_table[obj])) {
	    set_time_to_live(obj, SP_MAX_TIME2LIVE);
	}
    }
}

char *
sp_alloc(size, n)
unsigned size, n;
{
    char *p;
    
    if ((p = (char *) calloc(n, size)) != NULL) {
	return p;
    }

    if (cache_initialised) {
	/* Uncache enough directories to make the calloc succeed
	 * (but not the most recently used one, or any that have pending
	 * modifications; they may be needed in cache)
	 */
	while (p == NULL) {
	    if (uncache_dir()) {
		MON_EVENT("uncached directory for sp_alloc");
		p = (char *) calloc(n, size);	/* try again */
	    } else {
		MON_EVENT("sp_alloc: could not uncache any more directories");
		break;
	    }
	}
    }

    if (p == NULL) {
	MON_EVENT("sp_alloc: ran out of memory");
	scream("ran out of memory");
    }

    return p;
}

/*
 * sp_getobj is calling us to tell us that there is interest in this object.
 * Update its time2live field and ensure that it is in the cache.  Return
 * error status.
 */
#ifdef __STDC__
errstat sp_refresh(objnum obj, command cmd)
#else
errstat sp_refresh(obj, cmd) objnum obj; command cmd;
#endif
{
    errstat    err;
    
    if (bad_objnum(obj) || !sp_in_use(&_sp_table[obj])) {
	return STD_ARGBAD;
    }
    
    set_time_to_live(obj, SP_MAX_TIME2LIVE);
    
    /* NOTE: We should only need to read the directory into the cache for a
     * subset of the commands, but our caller needs the random field in order
     * to validate the capability, and it is stored only in the bullet file.
     *
     * In particular it would be nice if STD_INFO and STD_TOUCH could be
     * performed without needing to cache in the directory.
     *
     * Although it is possible, in principle, to change the format of the
     * superfile so that it also contains the random number for each object,
     * a simpler solution is probably to keep a cache of random numbers.
     */
    if (cmd == SP_CREATE) {
	/* This is called after sp_docreate.  Need to insert the new dir
	 * into the lru cache:
	 */
	cache_new_dir(obj);
	err = STD_OK;
    } else {
	if (cmd == STD_TOUCH) {
	    /* Touching the directory should immediately touch its
	     * Bullet files, to protect against aging.
	     */
	    capability filecaps[NBULLETS];
	    int	       good_files[NBULLETS];

	    get_dir_caps(obj, filecaps);
	    (void) dirf_touch(obj, filecaps, good_files);

            /* Even if dirf_touch() discovers some problem, it still
             * doesn't harm to try reading in the directory.
             * In case the problem is a bad bullet capability, it will
             * be (re-)discovered by cache_read().
             *
             * TODO: what if one Soap replica has an invalid dirfile replica,
	     * while at least one of the others doesn't have this problem ?!
             */
	}

	err = cache_read(obj);
    }

#ifdef SOAP_GROUP
    if (err != STD_OK) {
	/* Bad news: we failed to cache in a directory.
	 * In case we are handling a *write* operation in the group-based
	 * soap server this is really serious because presumably only
	 * one member (namely us) had a problem with its Bullet server.
	 * The other members have no way to know about this, so to avoid
	 * inconsistent directory states we currently panic() here.
	 * 
	 * TODO: an alternative would be to enter recovery mode, and see
	 * whether the problem is restricted to only one directory.
	 * In that case it could be recovered using the state from
	 * an other member.
	 */
	switch (cmd) {
	case SP_GETMASKS:  case SP_LIST:  case SP_LOOKUP:
	case SP_SETLOOKUP: case STD_INFO: case STD_RESTRICT:
	    /* read operations */
	    message("sp_refresh for %ld failed (%s), but don't crash",
		    (long) obj, err_why(err));
	    break;
	default:
	    panic("sp_refresh for %ld failed (%s); cmd = %d",
		  (long) obj, err_why(err), cmd);
	    break;
	}
    }
#endif

    return err;
}

static int Recovered;	/* set to 1 once we've fully recovered */

int
fully_initialised()
{
    return Recovered;
}

/*
 * Read all directories, and start the user threads.
 */
void
init_sp_table(do_fsck)
int do_fsck;
{
    register objnum obj;
    
    for (obj = 1; obj < _sp_entries; obj++) {
	capability filecaps[NBULLETS];

	get_dir_caps(obj, filecaps);
	
	if (caps_zero(filecaps, NBULLETS) == NBULLETS) { /* not in use */
	    sp_add_to_freelist(&_sp_table[obj]);
	    continue;
	}
	
	/* Mark the dir as existing, but not in cache: */
	mark_out_of_cache(obj);
	
	if (do_fsck) {
	    /* Purely as a safety check, bring it into memory: */
	    (void) sp_refresh(obj, SP_LOOKUP);
	
	    /* Another sanity check:  If servers available, check all replicas
	     * accessibility and size (sp_refresh only checks one replica).
	     *
	     * Note that we ignore the error code; if something is wrong
	     * with a replica, we don't want to panic or something.
	     * The diagnostics supplied should give an indication whether
	     * it is necessary to apply a manual sp_fix().
	     */
	    (void) dirf_check(obj, filecaps);
	}
    }

    Recovered = 1;
    refresh_all();
}

void
cache_set_size(rowmem)
int rowmem;
{
    Max_rows = ((long)rowmem * 1024) / BYTES_PER_ROW;
    Max_cached_dirs = Max_rows / AVG_ROWS_PER_DIR;

    message("cache_init: estimated bytes per row = %d", BYTES_PER_ROW);
    message("cache_init: rowmem = %dK, Max_rows = %d", rowmem, Max_rows);
    message("cache_init: Max_dirs = %d", Max_cached_dirs);

    /* TODO: sanity checks on sizes computed */
}

/*ARGSUSED*/
void
cache_init(rowmem)
int rowmem;
{
    int i;

    Cache_nodes = (struct cache_info *) alloc(Max_cached_dirs,
					      sizeof(struct cache_info));
    if (Cache_nodes == NULL) {
	panic("cannot allocate %d cache nodes", Max_cached_dirs);
    }

    /* put them all in the free list */
    Cache_freelist = NULL;
    for (i = 0; i < Max_cached_dirs; i++) {
	cache_add_to_freelist(&Cache_nodes[i]);
    }

    for (i = 0; i < HASHTAB_SIZE; i++) {
	Hash_table[i] = NULL;
    }

    cache_initialised = 1;
}

int
cached_rows()
{
#ifdef CHECK_CACHE
    /* Temporarily at this point we also check the consistency of
     * sp_allocated_rows() against the number of rows *really* in cache.
     */
    struct cache_info *cp;
    int nrows;

    nrows = 0;
    for (cp = Cache_newest; cp != NULL; cp = cp->c_older) {
	nrows += _sp_table[cp->c_obj].st_dir->sd_nrows;
    }

    if (sp_allocated_rows() != nrows) {
	scream("sp_allocated_rows(): %d; actually: %d",
	       sp_allocated_rows(), nrows);
    }
#endif

    return sp_allocated_rows();
}
