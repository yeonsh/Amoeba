/*	@(#)kernel_dep.c	1.2	96/03/26 11:35:46 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
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
 *  Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "capset.h"
#include "sp_dir.h"
#include "soap/soap.h"
#include "module/prv.h"

#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "stats.h"
#include "string.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_bullet.h"

/* From the disk server */
errstat		disk_rw();
void		disk_wait();

extern	Statistics	Stats;
extern	Superblk	Superblock;
extern	Superblk	DiskSuper;

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
	if (strncmp(dp->d_name, "vdisk:8", 7) == 0)     /* a virtual ramdisk! */
#else
	if (strncmp(dp->d_name, "vdisk:", 6) == 0)      /* a virtual disk! */
#endif
	{
	    if (sp_lookup(&dir, dp->d_name, &vd_cap) != STD_OK)
		bpanic("Lookup of capability %s failed", dp->d_name);

	    tmp = &vd_cap.cs_suite[0].s_object;
	    /* Read block 0 into Superblock. */
	    if (disk_rw(DS_READ, &tmp->cap_priv, D_PHYS_SHIFT, (disk_addr) 0,
				(disk_addr) 1, (bufptr) &DiskSuper) != STD_OK)
	    {
		bwarn("Bullet Server WARNING: Cannot access %s", dp->d_name);
		continue;
	    }

	    /* If it is a valid superblock it is saved and we can go on */
	    if (valid_superblk(&DiskSuper, &Superblock))
	    {
		message("Valid File System on %s",dp->d_name);
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
    bwarn("No valid Bullet File System.");
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
#define BULLET_STKSIZ		4000
#endif
#ifndef BSWEEP_STKSIZ
#define BSWEEP_STKSIZ		4000
#endif

void NewKernelThread();

static int	nr_memb_threads;
int		bs_sep_ports;

static void
gbullet_do_first()
{
    int i;
    char	*fail_reason;

    nr_memb_threads= NR_BULLET_THREADS/3 ;
    if ( nr_memb_threads<1 ) {
	nr_memb_threads=1 ;
    }
    if (bullet_init())
    {
	NewKernelThread(bullet_send_queue, (vir_bytes) BSWEEP_STKSIZ);
	fail_reason=bs_can_start_service();
	if ( !fail_reason ) {
	    bs_start_service() ;
	    if (!bs_build_group()) {
		bpanic("cannot build group");
	    }
	} else message("Postponing file service: %s\n",fail_reason) ;
	
	for (i = 0; i < nr_memb_threads-1; i++)
		NewKernelThread(member_thread, (vir_bytes) BULLET_STKSIZ);
    }
    member_thread();
}

void
gbullet_init_all()
{
	NewKernelThread(gbullet_do_first, (vir_bytes) BSWEEP_STKSIZ);
}


void
bs_start_service()
{
    int i;
    int	nr_file_threads ;
    int	nr_super_threads ;

    if ( !bullet_start() ) {
	bwarn("Failed to start startable server") ;
	return ;
    }

    if ( PORTCMP(&Superblock.s_fileport,&Superblock.s_supercap.cap_port) ) {
	/* Super & file port are equal, switch all through file port */
	bs_sep_ports=0 ;
	nr_super_threads=0  ;
    } else {
	/* Super and file port differ, make separate threads */
	bs_sep_ports=1 ;
        nr_super_threads= NR_BULLET_THREADS/3 ;
        if ( nr_super_threads<1 ) {
	    nr_super_threads=1 ;
        }
    
    }

    nr_file_threads= NR_BULLET_THREADS-nr_memb_threads-nr_super_threads-1 ;
    if ( nr_file_threads<1 ) {
	nr_file_threads=1 ;
    }


    for (i = 0; i < nr_file_threads; i++)
	NewKernelThread(bullet_thread, (vir_bytes) BULLET_STKSIZ);
    NewKernelThread(bullet_sweeper, (vir_bytes) BSWEEP_STKSIZ);
    NewKernelThread(bullet_back_sweeper, (vir_bytes) BSWEEP_STKSIZ);
    for (i = 0; i < nr_super_threads; i++)
	NewKernelThread(super_thread, (vir_bytes) BULLET_STKSIZ);
}
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * vsprintf - print formatted output without ellipsis on an array
 * A hack, really. We should create a vbprintf, the +100 below is
 * bloody awfull.
 */

#include <stdio.h>

#ifdef __STDC__
#include "stdarg.h"
#else
#include "varargs.h"
#endif  /* __STDC__ */

#ifdef __STDC__
int vsprintf(char *s, const char *format, va_list arg)
#else
int vsprintf(s, format, arg) char *s; char *format; va_list arg;
#endif
{
	extern char *doprnt();
	char	*rc;

	rc = doprnt(s, s+100, format, arg);
	if (rc && rc < s+100) *rc = '\0';

	return rc-s;
}
