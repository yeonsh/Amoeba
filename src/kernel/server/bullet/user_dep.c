/*	@(#)user_dep.c	1.11	96/02/27 14:13:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	These routines manage the interface between the bullet server and
 *	the virtual disk server for the user mode bullet server.  The main
 *	routine is used to start a single threaded bullet server under UNIX
 *	or a multithreaded bullet server under Amoeba.
 *
 *  Author:
 *	Greg Sharp 26-06-90
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "capset.h"
#include "sp_dir.h"
#include "soap/soap.h"
#include "module/prv.h"
#include "module/name.h"
#include "module/host.h"
#include "string.h"

#include "module/disk.h"
#include "bullet/bullet.h"
#include "bullet/inode.h"
#include "bullet/superblk.h"
#include "stats.h"
#include "stdio.h"
#include "stdlib.h"
#include "cache.h"
#include "bs_protos.h"
#define	thread ___thread  /* Yuk - got to get rid of the kernel thread defn */
#include "sys/proto.h"
#undef thread
#include "thread.h"

#ifdef __STDC__
#include "stdarg.h"
#define VA_LIST		va_list
#define VA_ALIST
#define VA_DCL
#define VA_START( ap, last )	va_start( ap, last )
#define VA_END		va_end
#define VA_ARG		va_arg
#else
#include "varargs.h"
#define VA_LIST		va_list
#define VA_ALIST	, va_alist
#define VA_DCL		va_dcl
#define VA_START( ap, last )	va_start( ap )
#define VA_END		va_end
#define VA_ARG		va_arg
#endif  /* __STDC__ */


#define DEF_AM_PATH "/super/cap/ubulletsvr/default"
#define DEF_UX_PATH "bullcap"

#ifndef MAX_BULLET_THREADS
#define MAX_BULLET_THREADS	10
#endif

#ifndef DEF_BULLET_MEMSIZE
#define	DEF_BULLET_MEMSIZE	2*1024*1024
#endif

#ifdef AMOEBA
static int	num_threads = MAX_BULLET_THREADS;
#endif

static int	memsize = DEF_BULLET_MEMSIZE;
static Inodenum	Inodes_in_cache;

#define STACKSIZE 8192

extern	Statistics	Stats;
extern	Superblk	Superblock;

/* Capability of the disk server */
static	capability	Diskcap;

/* The user version of the bullet server needs the name of the virtual disk */
static char *		Disk_name;

/* The user version of the bullet server allows naming of capability paths */
#ifdef AMOEBA
static char *		cap_path = DEF_AM_PATH;
#define	OPT_USAGE	"[-c capability] [-i numinodes] [-m memsize] [-t nthread] [vdiskname]"
#define	OPTIONS		"c:i:m:t:"
#else
static char *		cap_path = DEF_UX_PATH;
#define	OPT_USAGE	"[-c filename] [-i numinodes] [-m memsize] [vdiskname]"
#define	OPTIONS		"c:i:m:"
#endif

extern int	optind;
extern char *	optarg;


static
usage(prog_name)
char *	prog_name;
{
    fprintf(stderr, "Usage %s %s\n", prog_name, OPT_USAGE);
    exit(2);
    /*NOTREACHED*/
}


main(argc, argv)
int	argc;
char	*argv[];
{
    int opt;

    while ((opt = getopt(argc, argv, OPTIONS)) != EOF)
    {
	switch (opt)
	{
	case 'c':
		cap_path = optarg;
		break;

	case 'i':
		Inodes_in_cache = strtol(optarg, (char **) 0, 0);
		if (Inodes_in_cache <= 0)
		{
		    printf("Invalid value for number of inodes to cache: %d\n",
							    Inodes_in_cache);
		    exit(3);
		}
		break;

	case 'm':
		memsize = strtol(optarg, (char **) 0, 0);
		break;

#ifdef AMOEBA
	case 't':
		num_threads = atoi(optarg);
		break;
#endif
	default:
		usage(argv[0]);
		/*NOTREACHED*/
	}
    }
    if (optind > argc)
    {
	usage(argv[0]);
	/*NOTREACHED*/
    }
    if (optind == argc - 1)
    {
	Disk_name = argv[optind];
#ifdef AMOEBA
	if (name_lookup(Disk_name, &Diskcap) != STD_OK)
#else
	if (host_lookup(Disk_name, &Diskcap) != STD_OK)
#endif /* AMOEBA */
	    bpanic("Bullet server: lookup failed for %s", Disk_name);
    }
    else
    {
	capability *p;

	if ((p = getcap("BULLET_VDISK")) == 0)
	    bpanic("no vdisk given and BULLET_VDISK not set");
	Diskcap = *p;
	Disk_name = "vdisk";
    }

    /* Initialize the server */
    if (bullet_init() == 0)
	exit(1);

    /* Start the server threads */
#ifdef AMOEBA
    {
	int i;

	if (thread_newthread(bullet_sweeper, STACKSIZE, (char *) 0, 0) == 0 )
	    bpanic("cannot start sweeper thread");
	    
	i = num_threads - 1;
	while (--i)
	    if (thread_newthread(bullet_thread, STACKSIZE, (char *) 0, 0) == 0)
		bpanic("cannot start bullet threads");
    }
#endif
    bullet_thread((char *) 0, 0);
    exit(0);
    /*NOTREACHED*/
}


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
    /* Read block 0 into Superblock. */
    if (disk_read(&Diskcap, D_PHYS_SHIFT, (disk_addr) 0,
			    (disk_addr) 1, (bufptr) &Superblock) != STD_OK)
	bpanic("Bullet Server: Cannot access %s\n", Disk_name);

    /* If it is a valid superblock it is saved and we can go on */
    if (valid_superblk())
    {
	if (Inodes_in_cache != 0)
	{
	    if (Inodes_in_cache < Superblock.s_numinodes)
	    {
		bwarn("Only caching %d of the %d inodes in system.\n",
				Inodes_in_cache, Superblock.s_numinodes);
		Superblock.s_numinodes = Inodes_in_cache;
	    }
	    else
	    {
		printf("Requested number of inodes to be cached (%d) ",
							Inodes_in_cache);
		printf("exceeds available inodes (%d)\n",
							Superblock.s_numinodes);
	    }
	}
	return 1;
    }
    bpanic("Bullet server: Invalid superblock");
    /*NOTREACHED*/
}


/*
 * DISK_READ
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
    return disk_read(&Diskcap, Superblock.s_blksize, from, blkcnt, to);
}


/*
 * DISK_WRITE
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
    errstat disk_write();

    Stats.i_disk_write++;
    return disk_write(&Diskcap, Superblock.s_blksize, to, blkcnt, from);
}


vir_bytes
seg_maxfree()
{
    return memsize;
}


char *
aalloc(size, align)
vir_bytes	size;
int		align;
{
    char *	b;

    if ((b = (char *) malloc((size_t)(size + align))) == (char *) 0)
	bpanic("Bullet server: memory allocation error");
    b = (char *) (((unsigned int)b & ~(align-1)) + align);
    return b;
}


/*ARGSUSED*/
errstat
dirappend(name, cap)
char *		name;
capability *	cap;
{
    capability cap2;

#ifndef AMOEBA
    int fd;
#endif

    if (name_append(cap_path, cap) != STD_OK)
    {
	if (name_lookup(cap_path, &cap2) == STD_OK &&
	    memcmp((_VOIDSTAR) &cap2, (_VOIDSTAR) cap, sizeof(capability)) == 0)
	{
	    return STD_OK;
	}
	bwarn("couldn't store capability in %s\n", cap_path);
#ifndef AMOEBA
	if ((fd = creat(cap_path, 0444)) > 0)
	{
	    write(fd, cap, sizeof(*cap));
	    close(fd);
	    bwarn("Capability stored on UNIX file %s\n", cap_path);
	}
	else
	{
	    bwarn("Create of %s failed, capability lost\n", cap_path);
	    return STD_SYSERR;
	}
#endif /* AMOEBA */
    }
    return STD_OK;
}


#ifdef __STDC__
panic(char *fmt, ...)
#else
/*VARARGS1*/
panic(fmt VA_ALIST)
char *fmt;
VA_DCL
#endif /* __STDC__ */
{
    VA_LIST     args;

    VA_START(args, fmt);
    printf(fmt, args);
    VA_END(args);
    exit(1);
}


#if !defined(NDEBUG) && !defined(AMOEBA)

void
badassertion(file, line)
char *file;
int line;
{
    panic("assertion failed in %s, line %d", file, line);
    /*NOTREACHED*/
}

#endif

#if !defined(AMOEBA)

void
threadswitch()
{
}

#endif
