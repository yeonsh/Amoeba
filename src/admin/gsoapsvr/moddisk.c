/*	@(#)moddisk.c	1.1	96/02/27 10:02:44 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */
 
#include <assert.h>
#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"
#include "soap/super.h"
#include "disk.h"
#include "module/disk.h"
#include "module/buffers.h"

#include "moddisk.h"

#ifdef MAKESUPER
#include <stdio.h>
#include <string.h>
#define Scream(list)	{ printf list; printf("\n"); }
#else
#include "main.h"
#define Scream(list)	{ scream list; }
#endif

/*
 * Implementation of a disk-based modification list storage module.
 *
 * The only assumption is that disk writes of 1 block are atomic.
 * If this is not the case, we would have to use the ``spare-block''
 * solution like in the ``non-volatile ram'' case.
 */

#define NTRIES		3		/* #tries in disk operations      */
#define MOD_MINBLOCKS	1		/* minimum #blocks for modlist    */
#define MOD_MAXBLOCKS	64		/* maximum #blocks (sanity check) */
#define MOD_MAGIC	0x3083A61C

#define MOD_OVERHEAD	sizeof(int32)	/* for the magic number           */
#define MOD_SIZE	(BLOCKSIZE - MOD_OVERHEAD)

static capability Mod_vdisk;
static char       Mod_buf[BLOCKSIZE];
static char      *Mod_user_start = &Mod_buf[MOD_OVERHEAD];
static disk_addr  Mod_firstblock;
static disk_addr  Mod_nblocks;

/* Note: although block 0 is a valid block nr, it may only be used
 * in this module.  TODO: disallow using block 0 by outsiders.
 */
#define goodblocknr(b)	(0 <= (b) && (b) <= Mod_nblocks)

char *
md_get_buf()
{
    /* Return the buffer in which to store the modification list.
     * For this disk-based server it can always be the same,
     * as long as the modifications are not written concurrently.
     *
     * For the mapped-in non-volatile ram solution however,
     * we could return a pointer to the appropriate place in novram.
     */
    return Mod_user_start;
}

errstat
md_writeblock(block)
disk_addr block;
{
    /* Write the modifications for block blocknr to disk,
     * guaranteeing atomicity.
     */
    disk_addr block_nr;
    int       i;
    errstat   err;

    assert(goodblocknr(block));

    /* add the magic number */
    (void) buf_put_int32(Mod_buf, Mod_user_start, (int32) MOD_MAGIC);

    block_nr = Mod_firstblock + block;

    for (i = 0; i < NTRIES; i++) {
	err = disk_write(&Mod_vdisk, L2VBLKSZ,
			 block_nr, (disk_addr) 1, (bufptr) Mod_buf);

	if (err != STD_OK) {
	    Scream(("md_writeblock: disk_write %ld failed (%s)",
		    block_nr, err_why(err)));
	} else {
	    break;
	}
    }

    return err;
}

errstat
md_readblock(block, bufp)
disk_addr block;
char    **bufp;
{
    /* Only used on startup.  The user of this module (modlist.c)
     * typically reads all blocks on startup, to see whether they
     * contain updates that still need to be performed.
     */
    disk_addr block_nr;
    int       i;
    errstat   err;

    assert(goodblocknr(block));

    *bufp = NULL;
    block_nr = Mod_firstblock + block;

    for (i = 0; i < NTRIES; i++) {
	err = disk_read(&Mod_vdisk, L2VBLKSZ,
			block_nr, (disk_addr) 1, (bufptr) Mod_buf);

	if (err != STD_OK) {
	    Scream(("md_readblock(%d): disk_read %ld failed (%s)",
		    block, block_nr, err_why(err)));
	} else {
	    break;
	}
    }

    if (err == STD_OK) {
	/* check magic number */
	int32     magic;

	(void) buf_get_int32(Mod_buf, Mod_user_start, &magic);
	if (magic != MOD_MAGIC) {
	    Scream(("md_readblock(%d): bad magic number 0x%lx",
		    block, (long) magic));
	    err = STD_SYSERR;
	} else {
	    *bufp = Mod_user_start;
	}
    }

    return err;
}

errstat
md_init(servercap, firstblock, nblocks, size)
capability    *servercap;  /* server storing blocks (or NULL)	       */
disk_addr      firstblock; /* first mod-list block on that server      */
disk_addr     *nblocks;	   /* Return: number of blocks		       */
unsigned int  *size;	   /* Return: size available for modifications */
{
    /* Initialise this module.  The first block used is an administrative one,
     * telling how many blocks there are, etc.
     */
    port    checkport;
    int32   modmagic;
    char   *buf, *end, *p;
    errstat err;

    Mod_vdisk = *servercap;
    Mod_firstblock = firstblock;
    
    /* read the first block, and get the size */
    if ((err = md_readblock((disk_addr) 0, &buf)) != STD_OK) {
	return err;
    }

    end = buf + MOD_SIZE;

    p = buf;
    p = buf_get_int32(p, end, &modmagic);
    p = buf_get_int32(p, end, &Mod_nblocks);
    /* p = buf_get_port(p, end, &checkport); */
    
    if ((p == NULL) || (modmagic != MOD_MAGIC)) {
	Scream(("md_block_init: no modlist at block %ld", firstblock));
	return STD_SYSERR;
    }

#if 0
    if (!PORTCMP(&checkport, &my_super_getport)) {
	Scream(("md_block_init: checkport `%s' does not match mine",
		ar_port(&checkport)));
	return STD_SYSERR;
    }
#endif

    if ((Mod_nblocks < MOD_MINBLOCKS) || (Mod_nblocks > MOD_MAXBLOCKS)) {
	Scream(("md_block_init: illegal #blocks: %ld", (long) Mod_nblocks));
	return STD_SYSERR;
    }

    *nblocks = Mod_nblocks;
    *size = MOD_SIZE;
    return STD_OK;
}

#ifdef MAKESUPER

errstat
md_blockformat(servercap, firstblock, nblocks)
capability *servercap;  /* server storing blocks 	       */
disk_addr   firstblock; /* first mod-list block on that server */
disk_addr   nblocks;	/* number of blocks		       */
{
    /* Used to create a new, empty modification list on disk.
     * Should only be done once, or modifications may be lost!!
     * This code should only be present in makesuper, or related programs.
     */
    char     *buf, *end;
    char     *p;
    disk_addr block;
    errstat   err;

    Mod_vdisk = *servercap;
    Mod_firstblock = firstblock;
    Mod_nblocks = nblocks;

    /* The first block is special: */
    buf = md_get_buf();
    end = buf + MOD_SIZE;
    (void) memset(buf, '\0', (size_t) (end - buf));

    p = buf;
    p = buf_put_int32(p, end, (int32) MOD_MAGIC);
    p = buf_put_int32(p, end, (int32) nblocks);
    /* p = buf_put_port (p, end, checkport); */
    if (p == NULL) {
	err = STD_SYSERR;
    } else {
	err = md_writeblock(0);
    }

    if (err != STD_OK) {
	fprintf(stderr, "could not write block 0 of modlist (%s)\n",
		err_why(err));
	return err;
    }

    /* Initialise the other blocks to contain no modifications: */
    for (block = 1; block <= nblocks; block++) {
	buf = md_get_buf();
	end = buf + MOD_SIZE;
	(void) memset(buf, '\0', (size_t) (end - buf));
    
	p = buf_put_int16(buf, end, (int16) 0); /* see modlist.c */
	if (p == NULL) {
	    err = STD_SYSERR;
	} else {
	    err = md_writeblock(block);
	}

	if (err != STD_OK) {
	    fprintf(stderr, "could not write block %ld of modlist (%s)\n",
		    (long) block, err_why(err));
	    return err;
	}
    }

    return STD_OK;
}

#endif /* MAKESUPER */
