/*	@(#)superfile.c	1.7	96/02/27 10:23:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "monitor.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/soapsvr.h"
#include "soap/super.h"

#include "module/disk.h"
#include "module/ar.h"

#include "global.h"
#include "filesvr.h"
#include "main.h"
#include "intentions.h"
#include "superfile.h"
#include "superblock.h"
#include "seqno.h"

#define SPB_MIN_CACHED	8
#define SPB_MAX_CACHED	1000

extern int Max_cached_dirs;

static sp_block *Super_blocks = NULL;

/* The in-core version of the super block 0 (in our endianness): */
static sp_block SUPER;

/* For (un)marshalling super blocks: */
static char Block_buf[BLOCKSIZE];


static capability My_vdisk;	/* vdisk server storing super blocks */

int _sp_entries;
sp_tab *_sp_table;	/* the in-core directory table */

struct spb_cache {
    disk_addr         c_blkno; /* block number of cached superblock    */
    sp_block	     *c_block; /* the data block proper		       */
    int		      c_dirty; /* is data block dirty?		       */
    struct spb_cache *c_older; /* next older cache entry               */
    struct spb_cache *c_newer; /* next newer cache entry               */
    struct spb_cache *c_hash;  /* next cache entry with same hash()    */
    struct spb_cache *c_ndirt; /* next dirty cache entry               */
    struct spb_cache *c_pdirt; /* previous dirty cache entry           */
};

static struct spb_cache *Spb_cache_nodes = NULL;
static struct spb_cache *Spb_cache_freelist = NULL;

static struct spb_cache *Spb_cache_newest = NULL;
static struct spb_cache *Spb_cache_oldest = NULL;

static struct spb_cache *Spb_first_dirty = NULL;

#define HASH_BITS             9
#define HASHTAB_SIZE          (1 << HASH_BITS)
static struct spb_cache *Spb_hashtable[HASHTAB_SIZE];

#define hash(blkno)           ((blkno) & (HASHTAB_SIZE - 1))

/* Forward: */
static void spb_cache_delete _ARGS((disk_addr blkno));
static void read_blocks _ARGS((disk_addr start, int nblocks, char *buf));
static void write_block _ARGS((disk_addr block_nr, char *buf));

static void
spb_cache_add_to_freelist(cp)
struct spb_cache *cp;
{
    assert(cp != NULL);

    cp->c_blkno = -1;
    cp->c_dirty = 0;
    cp->c_older = Spb_cache_freelist;
    cp->c_newer = NULL;
    cp->c_hash = NULL;
    cp->c_ndirt = NULL;
    cp->c_pdirt = NULL;
    Spb_cache_freelist = cp;
}

static void
spb_cache_flush(cp)
struct spb_cache *cp;
{
    if (cp->c_dirty) {
	MON_EVENT("flush superblock");
	write_block(cp->c_blkno, cp->c_block->sb_block);
	cp->c_dirty = 0;
	if (cp->c_pdirt != NULL) {
	    cp->c_pdirt->c_ndirt = cp->c_ndirt;
	}
	if (cp->c_ndirt != NULL) {
	    cp->c_ndirt->c_pdirt = cp->c_pdirt;
	}
	if (cp == Spb_first_dirty) {
	    Spb_first_dirty = Spb_first_dirty->c_ndirt;
	}
	cp->c_pdirt = cp->c_ndirt = NULL;
    }
}

static struct spb_cache *
spb_get_free_cache_node()
{
    struct spb_cache *cp;

    if (Spb_cache_freelist == NULL) {
	assert(Spb_cache_oldest != NULL);

	cp = Spb_cache_oldest;
	spb_cache_delete(cp->c_blkno);
	spb_cache_add_to_freelist(cp);
    }
    assert(Spb_cache_freelist != NULL);

    cp = Spb_cache_freelist;
    Spb_cache_freelist = cp->c_older;
    return cp;
}

static void
spb_cache_link(cp, blkno)
register struct spb_cache *cp;
disk_addr blkno;
{
    struct spb_cache **hash_ptr;

    /* add it to the hash chain for blkno */
    hash_ptr = &Spb_hashtable[hash(blkno)];
    cp->c_hash = *hash_ptr;
    *hash_ptr = cp;

    /* add it in front of the lru list */
    cp->c_newer = NULL;
    cp->c_older = Spb_cache_newest;
    if (Spb_cache_newest != NULL) {
        assert(Spb_cache_newest->c_newer == NULL);
        Spb_cache_newest->c_newer = cp;
    }
    Spb_cache_newest = cp;
    if (Spb_cache_oldest == NULL) {
        Spb_cache_oldest = cp;
    }
}

static void
spb_cache_delete(blkno)
disk_addr blkno;
{
    register struct spb_cache *prevhash, *cp;

    /* Remove it from the hash chain */
    for (prevhash = NULL, cp = Spb_hashtable[hash(blkno)];
         cp != NULL && cp->c_blkno != blkno;
         prevhash = cp, cp = cp->c_hash)
    {
        /* skip */
    }

    if (cp == NULL) {
        panic("cached block %ld not on hash chain", blkno);
    }

    /* If we're removing a dirty cache entry, flush it first */
    spb_cache_flush(cp);

    if (prevhash == NULL) {
        /* We're removing the first one from the hash list */
        Spb_hashtable[hash(blkno)] = cp->c_hash;
    } else {
        prevhash->c_hash = cp->c_hash;
    }

    /* Remove it from the lru list */
    if (cp == Spb_cache_oldest) {
        Spb_cache_oldest = cp->c_newer;
    }
    if (cp == Spb_cache_newest) {
        Spb_cache_newest = cp->c_older;
    }
    if (cp->c_newer != NULL) {
        cp->c_newer->c_older = cp->c_older;
    }
    if (cp->c_older != NULL) {
        cp->c_older->c_newer = cp->c_newer;
    }
}

static void
spb_init_cache(maxcache, nblocks)
int maxcache;
int nblocks;
{
    int i;

    if (maxcache < SPB_MIN_CACHED) {
	maxcache = SPB_MIN_CACHED;
    }
    if (maxcache > SPB_MAX_CACHED) {
	maxcache = SPB_MAX_CACHED;
    }
    if (maxcache > nblocks) {
	maxcache = nblocks;
    }

    message("caching %d of %d super blocks", maxcache, nblocks);

    Super_blocks = (sp_block *) malloc((size_t)(maxcache * BLOCKSIZE));
    if (Super_blocks == NULL) {
	panic("spb_init_cache: cannot allocate %ld super blocks", maxcache);
    }

    Spb_cache_nodes = (struct spb_cache *) alloc(maxcache,
						 sizeof(struct spb_cache));
    if (Spb_cache_nodes == NULL) {
        panic("cannot allocate %d cache nodes", maxcache);
    }

    /* put them all in the free list */
    Spb_cache_freelist = NULL;
    for (i = 0; i < maxcache; i++) {
	Spb_cache_nodes[i].c_block = &Super_blocks[i];
        spb_cache_add_to_freelist(&Spb_cache_nodes[i]);
    }

    for (i = 0; i < HASHTAB_SIZE; i++) {
        Spb_hashtable[i] = NULL;
    }
}

static struct spb_cache *
spb_cache_in(blkno)
disk_addr blkno;
{
    /* Cache in block blkno, if needed, and return a pointer to its
     * cache entry.
     */
    register struct spb_cache *cp;

    if (Spb_cache_newest != NULL && Spb_cache_newest->c_blkno == blkno) {
	return Spb_cache_newest;
    }

    /* See if we have it cached in already */
    for (cp = Spb_hashtable[hash(blkno)];
	 cp != NULL && cp->c_blkno != blkno;
	 cp = cp->c_hash)
    {
	/* skip */
    }

    if (cp != NULL) {
	/* Have to move it in front of the lru list. First remove it. */
	assert(cp->c_newer != NULL);
	cp->c_newer->c_older = cp->c_older;
	if (cp->c_older != NULL) {
	    cp->c_older->c_newer = cp->c_newer;
	} else {
	    Spb_cache_oldest = cp->c_newer;
	}

	/* Add it back in front */
	cp->c_newer = NULL;
	cp->c_older = Spb_cache_newest;
	Spb_cache_newest->c_newer = cp;
	Spb_cache_newest = cp;
    } else {
	/* Have to create a new cache entry. */
	cp = spb_get_free_cache_node();

	cp->c_blkno = blkno;
	cp->c_dirty = 0;
	read_blocks(blkno, 1, cp->c_block->sb_block);

	spb_cache_link(cp, blkno);
    }

    return cp;
}

static void
spb_mark_dirty(cp)
struct spb_cache *cp;
{
    /* Mark the given cache entry dirty, so that spb_flush_dirty_blocks
     * knows that it should be written to disk.
     */
    if (cp->c_dirty == 0) {
	cp->c_dirty = 1;
	cp->c_pdirt = NULL;
	cp->c_ndirt = Spb_first_dirty;
	if (Spb_first_dirty != NULL) {
	    Spb_first_dirty->c_pdirt = cp;
	}
	Spb_first_dirty = cp;
    }
}

static void
spb_flush_dirty_blocks()
{
    /* Write all the super blocks marked dirty to disk */
    struct spb_cache *cp, *cp_ndirty;

    cp_ndirty = NULL;
    for (cp = Spb_first_dirty; cp != NULL; cp = cp_ndirty) {
	assert(cp->c_dirty);
	cp_ndirty = cp->c_ndirt;
	spb_cache_flush(cp);
	assert(cp->c_dirty == 0);
    }
}


static void
read_blocks(start, nblocks, buf)
disk_addr start;
int       nblocks;
char     *buf;
{
    /*
     * Read 'nblocks' superblocks from the vdisk server, starting at 'start'.
     * Try three times before giving up.
     */
    int j;
    
    MON_EVENT("read superblock");
    for (j = 0; j < 3; j++) {
	errstat err;

	err = disk_read(&My_vdisk, L2VBLKSZ,
			start, (disk_addr) nblocks, (bufptr) buf);
	if (err == STD_OK) {
	    return;
	} else {
	    MON_EVENT("superblock read error");
	    scream("could not read %d super blocks starting at %ld (%s)",
		   nblocks, start, err_why(err));
	}
    }

    panic("cannot read superblock");
}

static void
write_block(block_nr, buf)
disk_addr block_nr;
char     *buf;
{
    /*
     * Write superblock block_nr to the vdisk server.
     * Try three times before giving up.
     * Writing block 0 is supposed to be atomic at the vdisk server's side.
     */
    int j;
    
    MON_EVENT("write superblock");
    for (j = 0; j < 3; j++) {
	errstat err;

	err = disk_write(&My_vdisk, L2VBLKSZ,
			 (disk_addr) block_nr, (disk_addr) 1, (bufptr) buf);

	if (err == STD_OK) {
	    return;
	} else {
	    MON_EVENT("superblock write error");
	    scream("cannot write super block %d (%s)",
		   block_nr, err_why(err));
	}
    }

    panic("cannot write superblock");
}

static void
write_SUPER()
{
    errstat err;

#ifdef SOAP_DIR_SEQNR
    get_global_seqno(&SUPER.sb_dirseq);
#endif
    err = sp_marshall_super(Block_buf, &Block_buf[sizeof(Block_buf)], &SUPER);
    if (err != STD_OK) {
	panic("could not marshall super block (%s)", err_why(err));
    }
    write_block((disk_addr) 0, Block_buf);
}

long
get_seq_nr()
{
    return SUPER.sb_seq;
}

void
set_seq_nr(seq)
long seq;
{
    SUPER.sb_seq = seq;
}

int
get_copymode()
{
    return SUPER.sb_copymode;
}

void
set_copymode(mode)
int mode;
{
#ifdef SOAP_GROUP
    /* copy mode is now a config vector telling which members are (or were)
     * part of the last living soap group.  It also contains a bit which is
     * set during recovery.
     */
    message("switching to 0x%x copy mode", mode);
#else
    assert(mode == 1 || mode == 2);
#endif
    SUPER.sb_copymode = mode;
}

void
flush_copymode()
{
    /*
     * save super block containing variable Sp_copymode.
     */
    write_SUPER();
}


#ifndef SOAP_GROUP
/*
 * Copy my super file to the other server.  We do this from back to front
 * to write the commit block last.  If all blocks have been written
 * successfully, I switch to two-copy-mode (if not already in two-copy-mode)
 * and send my sequence number to acknowledge that I know that the other
 * server has the current super file.  The other server cannot continue
 * operation until this acknowledgement because the last reply might have
 * gotten lost, causing the transaction that wrote the commit block to fail.
 */
errstat
copy_super_blocks()
{
    header  hdr;
    bufsize retsize;
    long    my_seq;
    int     block_nr;
    int     oldcopymode;
    errstat err;

    MON_EVENT("copy super file");
    message("copy super file to other side");

    oldcopymode = get_copymode();
    my_seq = get_seq_nr();
    set_copymode(2);	/* temporarily */

    for (block_nr = SUPER.sb_nblocks - 1; block_nr >= 0; block_nr--) {
	char *buf;

	hdr.h_command = SP_COPY;
	hdr.h_seq = my_seq;
	hdr.h_extra = block_nr;

	if (block_nr == 0) {
	    err = sp_marshall_super(Block_buf, &Block_buf[sizeof(Block_buf)],
				    &SUPER);
	    if (err != STD_OK) {
		panic("could not marshall superblock (%s)", err_why(err));
	    }
	    buf = Block_buf;
	} else {
	    /* Blocks with cappairs don't need any marshalling */
	    struct spb_cache *cp;

	    cp = spb_cache_in((disk_addr) block_nr);
	    buf = cp->c_block->sb_block;
	}

	err = to_other_side(3, &hdr, buf, BLOCKSIZE,
			    &hdr, NILBUF, 0, &retsize);

	if (err == STD_OK) {
	    if (hdr.h_seq == my_seq) {
		/* It now has the same sequence number, i.e., we have copied
		 * everything (including block 0, containing the seq nr.),
		 * or it already had the same sequence number to begin with.
		 */
		if (oldcopymode == 1) {	/* we switched to 2 copy mode */
		    flush_copymode();
		}
		MON_EVENT("switched to 2 copy mode");
		break;
	    } else if (hdr.h_seq > my_seq) {	/* protocol sanity check */
		panic("copy_super: protocol bug: other seq_nr too high");
	    }
	} else {
	    scream("copy_super_blocks: SP_COPY error (%s)", err_why(err));
	    break;
	}
    }

    if (err == STD_OK && hdr.h_seq != my_seq) {	/* protocol sanity check */
	panic("copy_super: protocol bug: other side did not update seq_nr");
    }

    if (err != STD_OK) {
	set_copymode(oldcopymode);	/* restore */
    }

    return err;
}

/*
 * Received a block from the other server's super file.  If I'm recovering,
 * I gratefully accept it and write it to my super file.  I know that the
 * other server makes sure that the commit block (block #0) is written last.
 * I reply with my sequence number such that the other server knows when
 * I've completed my recovery.  I can't continue, however, until the other
 * server has told me to do so by sending its sequence number across.
 *
 * TODO: send super blocks should be properly marshalled & unmarshalled,
 * and block 0 should be handled differently, because it has an entirely
 * different structure + semantics.
 */
int
got_super_block(hdr, buf, size)
header *hdr;
char *buf;
{
    unsigned int block_nr;

    if (!for_me(hdr)) {
	return 0;
    }
    if (size != BLOCKSIZE) {
	panic("gotcopy: bad blocksize %ld", size);
    }

    block_nr = hdr->h_extra;
    if (block_nr >= SUPER.sb_nblocks) {    
	panic("got_super_block: bad block number %d (max %d)",
	      block_nr, SUPER.sb_nblocks);
    }

    if (get_copymode() == 1 || fully_initialised()) {
	panic("gotcopy: unexpected copy of super block");
    }
    MON_EVENT("got super file block");
    
    if (block_nr == 0) {
	/* super block needs special treatment */
	errstat err;

	err = sp_unmarshall_super(buf, &buf[size], &SUPER);
	if (err != STD_OK) {
	    panic("could not unmarshall received superblock (%s)",
		  err_why(err));
	}
	set_seq_nr(hdr->h_seq);
#ifdef SOAP_DIR_SEQNR
	set_global_seqno(&SUPER.sb_dirseq);
	message("got_super_block: switching to new global seqno [%ld, %ld]",
		hseq(&SUPER.sb_dirseq), lseq(&SUPER.sb_dirseq));
#endif
	write_SUPER();
    } else {
	struct spb_cache *cp;

	cp = spb_cache_in((disk_addr) block_nr);
	memcpy((_VOIDSTAR) cp->c_block->sb_block, (_VOIDSTAR) buf, BLOCKSIZE);
	write_block((disk_addr) block_nr, buf);
    }

    /* in the reply send my current sequence number back */
    hdr->h_seq = get_seq_nr();
	
    sp_end();
    hdr->h_status = STD_OK;
    return 0;
}

#endif /* !SOAP_GROUP */

#define SOAP_SUPER	"SOAP_SUPER"

void
super_init(generic, my_super, other_super)
capability *generic;
capability *my_super;
capability *other_super;
{
    capability bull_caps[NBULLETS];
    capability *soap_super;
    long    nblocks;
    int     i;
    int     me;
    errstat err;

    if ((PAIRSPERBLOCK * sizeof(sp_cappair)) != BLOCKSIZE) {
	/* Note: this is not really required, but it is a sensible sanity
	 * check.  The cappairs should fill a superblock exactly.
	 */
	panic("unexpected BLOCKSIZE");
    }
    
    /* first try capability environment */
    soap_super = getcap(SOAP_SUPER);

    if (soap_super == NULL) {
	/* try string environment */
	char *soap_super_env;

	if ((soap_super_env = getenv(SOAP_SUPER)) != NULL) {
	    static capability super_svr;
	    char *after;

	    after = ar_tocap(soap_super_env, &super_svr);
	    if (after == NULL || *after != '\0') {
		panic("environment variable `%s' has bad format", SOAP_SUPER);
	    } else {
		soap_super = &super_svr;
	    }
	}
    }

    if (soap_super == NULL) {
	panic("super_init: can't get the `%s' capability", SOAP_SUPER);
    }
    My_vdisk = *soap_super;

    /*
     * First only read super block 0.  After we have read it in, we know how
     * much we need to allocate to store the cappairs.
     */
    read_blocks((disk_addr) 0, 1, Block_buf);
    err = sp_unmarshall_super(Block_buf, &Block_buf[sizeof(Block_buf)],&SUPER);
    if (err != STD_OK) {
	panic("super_init: super file has bad format");
    }

    nblocks = SUPER.sb_nblocks;
    if (nblocks <= 2 || nblocks > 10000) {
	panic("super_init: invalid number of super blocks (%ld)", nblocks);
    }

    /* Since several cached directories will typically share the same
     * superblock, we may assume that caching half of that amount is enough.
     */
    spb_init_cache(Max_cached_dirs / 2, (int) nblocks);

    _sp_entries = (nblocks - 1) * PAIRSPERBLOCK;
    if (_sp_entries < 2 || _sp_entries > 99999) {
	panic("super_init: bad number of directory slots (%ld)", _sp_entries);
    }

    _sp_table = (sp_tab *) alloc(_sp_entries, sizeof(*_sp_table));
    if (_sp_table == NULL) {
	panic("super_init: cannot allocate sp_table (not enough memory)");
    }
    
    intent_init();

    /* Make sure at least one Bullet server is up and
     * set each server to prefer a different Bullet server.
     * The bullet caps are stored as dir caps for object 0.
     *
     * We only have two file server priority classes:
     * the file server with index "sp_whoami()" has priority 1; others 0.
     */
    me = sp_whoami();
    get_dir_caps((objnum) 0, bull_caps);
    for (i = 0; i < NBULLETS; i++) {
	int which;

	which = fsvr_add_svr(&bull_caps[i], i == me);
	if (which != i) {
	    panic("bad Bullet sequence number");
	}
    }

    fsvr_check_svrs();
    if (fsvr_nr_avail() == 0) {
	panic("no Bullet servers available");
    }

    /*
     * Initialise SOAP (get-)capabilities as stored in super block 0.
     */
    *generic = SUPER.sb_public;

#ifdef SOAP_GROUP
    /* The superblock unmarshalling routine takes care of swapping the
     * super caps, when necessary.
     */
    *my_super = SUPER.sb_private.sc_caps[0];
    *other_super = SUPER.sb_private.sc_caps[1];
#else
    *my_super = SUPER.sb_private.sc_caps[me];
    *other_super = SUPER.sb_private.sc_caps[1 - me];
#endif

#ifdef SOAP_DIR_SEQNR
    message("super_init: global seqno in superblock is [%ld, %ld]",
	    hseq(&SUPER.sb_dirseq), lseq(&SUPER.sb_dirseq));
    set_global_seqno(&SUPER.sb_dirseq);
#endif
}

void
write_blocks_modified()
{
    /*
     * flush the updated super blocks to disk
     */
    spb_flush_dirty_blocks();
}

void
super_store_intents()
{
    /*
     * save super block currently used to store intentions
     */
    write_SUPER();
}

int
super_nintents()
{
    return SUPER.sb_nintent;
}

int
super_avail_intents()
{
    return MAXINTENT - SUPER.sb_nintent;
}

int
super_add_intent(obj, filecaps)
objnum obj;
capability filecaps[NBULLETS];
{
    sp_new *intent;
    int j;

    if (super_avail_intents() == 0) {
	return 0;
    }

    intent = &SUPER.sb_intent[SUPER.sb_nintent];
    for (j = 0; j < NBULLETS; j++) {
	intent->sn_caps.sc_caps[j] = filecaps[j];
    }
    intent->sn_object = obj;
    SUPER.sb_nintent++;

    return 1;
}

void
super_get_intent(intent_nr, intent)
int intent_nr;
struct sp_new *intent;
{
    assert(intent_nr < super_nintents());
    *intent = SUPER.sb_intent[intent_nr];
}

void
super_reset_intents(nintents)
int nintents;
{
    assert(0 <= nintents && nintents <= SUPER.sb_nintent);
    SUPER.sb_nintent = nintents;
}

#define BLOCK(obj)   (1 + (obj / PAIRSPERBLOCK))
#define Cappair(cp,obj) &(cp)->c_block->sb_cappairs[(obj)%PAIRSPERBLOCK]

void
set_dir_caps(obj, filecaps)
objnum 	   obj;
capability filecaps[NBULLETS];
{
    /*
     * Set the capabilities for the directory data of object obj.
     */
    struct sp_cappair *sc;
    struct spb_cache *cp;
    int i;

    cp = spb_cache_in(BLOCK(obj));
    sc = Cappair(cp, obj);
    for (i = 0; i < NBULLETS; i++) {
	sc->sc_caps[i] = filecaps[i];
    }

    /* A directory was updated in-core, but the intention is still on disk.
     * Mark the block containing the capability pair for this object as dirty.
     *
     * The function intent_make_permanent() will take care of actually
     * performing the committed intention on the super table.
     * It will be called by the intentions watchdog, or when there
     * is more room needed for new intentions.
     */
    spb_mark_dirty(cp);
}

void
get_dir_caps(obj, filecaps)
objnum 	   obj;
capability filecaps[NBULLETS];
{
    /*
     * Get the capabilities for the directory data of object obj.
     */
    struct sp_cappair *sc;
    struct spb_cache *cp;
    int i;

    cp = spb_cache_in(BLOCK(obj));
    sc = Cappair(cp, obj);
    for (i = 0; i < NBULLETS; i++) {
	filecaps[i] = sc->sc_caps[i];
    }
}

