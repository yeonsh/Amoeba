/*	@(#)kernel_dep.c	1.9	96/02/27 14:12:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * KERNEL_DEP.C
 *
 *	These routines manage the interface between the bullet server and
 *	the virtual disk server.  It is assumed that the disk thread
 *	and the bullet server have the buffer cache mapped to the same
 *	virtual address so that they can share use each other's pointers.
 *
 *  Author:
 *	Greg Sharp 30-01-89
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "capset.h"
#include "sp_dir.h"
#include "soap/soap.h"
#include "module/prv.h"

#include "disk/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "stats.h"
#include "string.h"
#include "cache.h"
#include "bs_protos.h"

/* From the disk server */
errstat		disk_rw();
void		disk_wait();

extern	Statistics	Stats;
extern	Superblk	Superblock;

/* Capability of the disk server */
static	capability	Diskcap;


/*
 * GET_DISK_SERVER_CAP
 *
 *	Get the capability for the virtual disk thread which we have to talk to.
 *	In the process we also need to read the superblock to see if we have
 *	the right disk.
 */

int
get_disk_server_cap()
{
    capability *	tmp;
    SP_DIR *		dd;
    struct sp_direct *	dp;
    capset		dir;
    capset		vd_cap;

    /* Wait until virtual disk is initialized */
    disk_wait();

    /*
     * go through all the directory entries and look for a virtual disk with a
     * valid bullet file system
     */
    if (sp_lookup(SP_DEFAULT, "/", &dir) != STD_OK ||
						(dd = sp_opendir("/")) == 0)
	bpanic("get_disk_server_cap: can't open kernel directory");

    while ((dp = sp_readdir(dd)) != 0)
#ifdef COLDSTART
	if (strncmp(dp->d_name, "vdisk:8", 7) == 0)	/* a virtual ramdisk! */
#else
	if (strncmp(dp->d_name, "vdisk:", 6) == 0)	/* a virtual disk! */
#endif
	{
	    if (sp_lookup(&dir, dp->d_name, &vd_cap) != STD_OK)
		bpanic("Lookup of capability %s failed", dp->d_name);

	    tmp = &vd_cap.cs_suite[0].s_object;
	    /* Read block 0 into Superblock. */
	    if (disk_rw(DS_READ, &tmp->cap_priv, D_PHYS_SHIFT, (disk_addr) 0,
				(disk_addr) 1, (bufptr) &Superblock) != STD_OK)
	    {
		bwarn("Bullet Server WARNING: Cannot access %s\n", dp->d_name);
		continue;
	    }

	    /* If it is a valid superblock it is saved and we can go on */
	    if (valid_superblk())
	    {
		sp_closedir(dd);
		Diskcap = *tmp;
		return 1;
	    }
	}

    /*
    ** If we get here then none of the directory entries in the kernel had a
    ** valid bullet file system
    */
    sp_closedir(dd);
    bwarn("No valid Bullet File System.\n");
    return 0;
}


/*
 * BS_DISK_READ
 *
 *	Low level routine to read blocks from the disk.
 *	Higher level routines are found in readwrite.c
 */

errstat
bs_disk_read(from, to, blkcnt)
disk_addr	from;		/* address on disk to read from */
bufptr		to;		/* address in cache memory to read into */
disk_addr	blkcnt;		/* # disk blocks to read */
{
    Stats.i_disk_read++;
    return (short) disk_rw(DS_READ, &Diskcap.cap_priv, Superblock.s_blksize,
							    from, blkcnt, to);
}


/*
 * BS_DISK_WRITE
 *
 *	Low level routine to write data to the disk.  The actual writing of
 *	inodes and data is found in readwrite.c
 */

errstat
bs_disk_write(from, to, blkcnt)
bufptr		from;		/* address in cache to write from */
disk_addr	to;		/* address on disk to write to */
disk_addr	blkcnt;		/* # disk blocks to write */
{ 
    Stats.i_disk_write++;
    return (short) disk_rw(DS_WRITE, &Diskcap.cap_priv, Superblock.s_blksize,
							    to, blkcnt, from);
}


/*
 * Initialisation code
 */

/*DBG*/
#ifndef NR_BULLET_THREADS
#define NR_BULLET_THREADS	30
#endif
#ifndef BULLET_STKSIZ
#define BULLET_STKSIZ		0 /* default size */
#endif
#ifndef BSWEEP_STKSIZ
#define BSWEEP_STKSIZ		0 /* default size */
#endif

void NewKernelThread();

void
bullet_init_all()
{
    int i;

    if (bullet_init())
    {
	for (i = 0; i < NR_BULLET_THREADS; i++)
		NewKernelThread(bullet_thread, (vir_bytes) BULLET_STKSIZ);
	NewKernelThread(bullet_sweeper, (vir_bytes) BSWEEP_STKSIZ);
    }
}
