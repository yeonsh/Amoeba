/*	@(#)modnvram.c	1.1	96/02/27 10:02:51 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <assert.h>
#include <string.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/super.h"
#include "disk.h"
#include "proc.h"
#include "module/disk.h"
#include "module/buffers.h"
#include "module/ar.h"
#include "module/proc.h"
#include "bullet/bullet.h"
#include "machdep.h"

#include "modnvram.h"
#include "main.h"

/*
 * Implementation of a NOVRAM-based modification list storage module.
 */

#define NOVR_MINBLOCKS	2		/* minimum #blocks for modlist    */
#define NOVR_DEFBLOCKS	32		/* default #blocks for auto-init  */
#define NOVR_MAXBLOCKS	64		/* maximum #blocks (sanity check) */

#define NOVR_BLOCKSIZE	512
#define NOVR_MAGIC	0x5083A61C

#define NOVR_OVERHEAD	sizeof(int32)	/* reserved for the magic number  */
#define NOVR_SIZE	(NOVR_BLOCKSIZE - NOVR_OVERHEAD)

extern port my_super_getport;

static disk_addr Novr_nblocks = -1;

/* Note: although block 0 is a valid block nr, it may only be used
 * in this module.  TODO: disallow using block 0 by outsiders.
 */
#define log_blk_nr(b)	((0 < (b)) && ((b) < Novr_nblocks - 1))
#define phys_blk_nr(b)	((0 <= (b)) && ((b) < Novr_nblocks))

/*
 * Novr_blockmap points to the remap table residing in the first block
 * of Novram.  It is used to switch over to a new block atomically.
 * If Novr_blockmap[B] contains M, then logical novram block B can
 * found at physical block M.  Updates are done as follows:
 *
 * Novr_blockmap[0] contains the spare block.  Suppose we want to replace block
 * 3 by a new version.  To do this, we rewrite the spare block, and
 * update the remap table.  Suppose originally:
 *	Novr_blockmap[0] = 5  and  Novr_blockmap[3] = 7
 * To do the atomic update, we set Novr_blockmap[3] to 5,
 * and Novr_blockmap[0] to 7.  To account for a possible crash between
 * these two assignments, we need to examine the remap table on startup.
 * Novr_blockmap[0] must be set to physical block not mapped by an
 * other logical block.
 */
static char *    Novr_blockmap = NULL;
static char *    Novr_addr = NULL;    /* pointer to the mapped-in novram */
static disk_addr Novr_total_blocks;

static errstat
novr_map_in(novr_cap, novr_addr, novr_nblocks)
capability *novr_cap;
char      **novr_addr;
disk_addr  *novr_nblocks;
{
    b_fsize novr_bytes;
    segid   sid;
    errstat err;
    
    if ((err = b_size(novr_cap, &novr_bytes)) != STD_OK) {
	scream("novr_map_in: could not get segment size (%s)", err_why(err));
	return err;
    }

    *novr_nblocks = novr_bytes / 512;

    /*
     * Find a place in the adress space to map the novram segment in
     */
    if ((*novr_addr = findhole((long) novr_bytes)) == NULL) {
        scream("novr_map_in: findhole failed");
        return STD_NOSPACE;
    }

    sid = seg_map(novr_cap, (vir_bytes) *novr_addr, (vir_bytes) novr_bytes,
		  (long) (MAP_TYPEDATA | MAP_READWRITE | MAP_INPLACE));
    if (ERR_STATUS(sid)) {
	err = ERR_CONVERT(sid);
        scream("novr_map_in: seg_map returns %s", err_why(err));
    }

    message("novr_addr = %lx; novr_bytes = %ld; novr_blocks = %ld",
	    novr_addr, (long) novr_bytes, (long) *novr_nblocks);

    return err;
}

char *
novr_get_buf()
{
    int spare;

    /* Return a pointer to the current spare block in novram. */
    assert(Novr_blockmap != NULL);
    spare = (int) Novr_blockmap[0];
    assert(phys_blk_nr(spare) && spare > 0);
#ifdef NOVR_DEBUG
    message("novr_get_buf: return spare block %ld", spare);
#endif

    return Novr_addr + (spare * NOVR_BLOCKSIZE) + NOVR_OVERHEAD;
}

errstat
novr_writeblock(block)
disk_addr block;
{
    /* Replace the novram block indicated with the current contents
     * of the spare block.  To implement this, we only need to update
     * the index block atomically, as described above.
     */
    char newspare;

    assert(log_blk_nr(block));
#ifdef NOVR_DEBUG
    message("novr_writeblock: %ld: %d -> %d",
	    (long) block, (int) Novr_blockmap[block], (int) Novr_blockmap[0]);
#endif

    newspare = Novr_blockmap[block];
    Novr_blockmap[block] = Novr_blockmap[0];
    Novr_blockmap[0] = newspare;

    return STD_OK;
}

errstat
novr_readblock(block, bufp)
disk_addr block;
char    **bufp;
{
    /* Only used on startup.  The user of this module (modlist.c)
     * typically reads all blocks on startup, to see whether they
     * contain updates that still need to be performed.
     */
    errstat   err = STD_OK;
    disk_addr remap;
    char     *novram;
    int32     magic;

    assert(log_blk_nr(block));
    assert(Novr_blockmap != NULL);

    remap = (disk_addr) Novr_blockmap[block];

#ifdef NOVR_DEBUG
    message("novr_readblock: %ld: %d", (long) block, (long) remap);
#endif
    assert(phys_blk_nr(remap) && remap > 0);

    novram = Novr_addr + (remap * NOVR_BLOCKSIZE);

    /* check magic number */
    (void) buf_get_int32(novram, novram + NOVR_OVERHEAD, &magic);
    if (magic != NOVR_MAGIC) {
	scream("novr_readblock(%d): bad magic number 0x%lx",
	       block, (long) magic);
	err = STD_SYSERR;
    } else {
	*bufp = novram + NOVR_OVERHEAD;
    }

    return err;
}

errstat
novr_init(servercap, firstblock, nblocks, size)
capability    *servercap;  /* server storing blocks 		       */
disk_addr      firstblock; /* first mod-list block on that server      */
disk_addr     *nblocks;	   /* Return: number of blocks		       */
unsigned int  *size;	   /* Return: size available for modifications */
{
    /* Initialise this module.  The first block used is an administrative one,
     * telling how many blocks there are, etc.  It also contains the
     * novram-block index table.
     */
    char      block_mapped[NOVR_MAXBLOCKS];
    port      checkport;
    int32     modmagic;
    char     *buf, *end, *p;
    int       i;
    int	      unmapped;
    errstat   err;

    if (Novr_addr == NULL) {	/* not mapped in yet; do it now */
	err = novr_map_in(servercap, &Novr_addr, &Novr_total_blocks);
	if (err != STD_OK) {
	    scream("cannot map in novram (%s)", err_why(err));
	    return err;
	}
    }

    /* scan the first block */
    buf = Novr_addr;
    end = buf + NOVR_SIZE;

    p = buf;
    p = buf_get_int32(p, end, &modmagic);
    p = buf_get_int32(p, end, &Novr_nblocks);
    p = buf_get_port(p, end, &checkport);
    
    if ((p == NULL) || (modmagic != NOVR_MAGIC)) {
	scream("novr_init: no modlist at block %ld", firstblock);
	return STD_SYSERR;
    }

    if (!PORTCMP(&checkport, &my_super_getport)) {
	scream("novr_init: checkport `%s' does not match mine",
	       ar_port(&checkport));
	return STD_ARGBAD;
    }

    if ((Novr_nblocks < NOVR_MINBLOCKS) || (Novr_nblocks > NOVR_MAXBLOCKS) ||
	(Novr_nblocks > Novr_total_blocks))
    {
	scream("novr_init: illegal #blocks: %ld", (long) Novr_nblocks);
	return STD_ARGBAD;
    }

    Novr_blockmap = p;
    
    for (i = 0; i < Novr_nblocks; i++) {
	block_mapped[i] = 0;
    }

    message("Novr_blockmap:");
    for (i = 0; i < Novr_nblocks - 1; i++) {
	message("%d -> %d", i, (int) Novr_blockmap[i]);
	block_mapped[Novr_blockmap[i]]++;
    }

    /* Now look for an unmapped block;  there may be at most one! */
    unmapped = 0;
    for (i = 1; i < Novr_nblocks - 1; i++) {
	if (!block_mapped[i]) {
	    if (!unmapped) {
		message("block %d is not mapped: make it the spare block", i);
		Novr_blockmap[0] = i;
	    } else {
		scream("block %d is unmapped as well", i);
	    }

	    unmapped++;
	}
    }

    if (unmapped > 1) {
	return STD_ARGBAD;
    }

    *nblocks = Novr_nblocks - 2; /* one spare */
    *size = NOVR_SIZE;
    return STD_OK;
}

static errstat
novr_init_block(block)
disk_addr block;
{
    char   *buf, *end, *p;
    errstat err;

    buf = novr_get_buf();
    end = buf + NOVR_SIZE;

    /* add the magic number */
    p = buf - NOVR_OVERHEAD;
    p = buf_put_int32(p, end, (int32) NOVR_MAGIC);

    /* Clear the rest of the buffer and add a marker telling
     * that there are no modifications.
     */
    (void) memset((_VOIDSTAR) buf, '\0', (size_t) (end - buf));
    p = buf_put_int16(buf, end, (int16) 0); /* see modlist.c */
    assert(p != NULL);

    if ((err = novr_writeblock(block)) != STD_OK) {
	scream("could install block %ld of modlist (%s)\n",
	       (long) block, err_why(err));
	return err;
    }

    return STD_OK;
}

errstat
novr_format(servercap, firstblock, nblocks)
capability *servercap;  /* server storing blocks 	       */
disk_addr   firstblock; /* first mod-list block on that server */
disk_addr   nblocks;	/* number of blocks		       */
{
    /* Used to create a new, empty modification list in novram.
     * Should only be done once, or modifications will be lost!!
     */
    char     *buf, *end;
    char     *p;
    disk_addr block;
    errstat   err;

    if (Novr_addr == NULL) {	/* not mapped in yet; do it now */
	err = novr_map_in(servercap, &Novr_addr, &Novr_total_blocks);
	if (err != STD_OK) {
	    scream("cannot map in novram (%s)", err_why(err));
	    return err;
	}
    }

    if (nblocks == 0) {	/* use default */
	nblocks = NOVR_DEFBLOCKS;
    }

    if (nblocks > Novr_total_blocks || nblocks > NOVR_MAXBLOCKS) {
	scream("novr_format: not enough novr blocks; %ld > min(%ld, %ld)",
	       (long) nblocks, (long) Novr_total_blocks, (long)NOVR_MAXBLOCKS);
	return STD_ARGBAD;
    }

    /* Initialise the first block: */
    buf = Novr_addr;
    end = buf + NOVR_SIZE;
    (void) memset((_VOIDSTAR) buf, '\0', (size_t) (end - buf));

    p = buf;
    p = buf_put_int32(p, end, (int32) NOVR_MAGIC);
    p = buf_put_int32(p, end, (int32) nblocks);
    p = buf_put_port (p, end, &my_super_getport);

    /* initialise the block remap table: */
    Novr_blockmap = p;
    if (Novr_blockmap + nblocks > end) {
	scream("marshalling error for block 0 of modlist");
	return STD_SYSERR;
    }
    for (block = 1; block < nblocks - 1; block++) {
	Novr_blockmap[block] = (char) block;
    }
    Novr_blockmap[0] = nblocks - 1;
    Novr_nblocks = nblocks;

    /* Initialise the other blocks to contain no modifications: */
    for (block = 1; block < nblocks - 1; block++) {
	if ((err = novr_init_block(block)) != STD_OK) {
	    return err;
	}
    }
    /* Need one more initialisation to get the magic number on the
     * current spare block, which is the only uninitialised one left.
     */
    return novr_init_block((disk_addr) 1);
}

