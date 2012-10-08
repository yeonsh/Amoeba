/*	@(#)user_dep.c	1.1	96/02/27 14:07:41 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
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
 *  Modified:
 *	Ed Keizer, 1995 - converted to group bullet server
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "capset.h"
#include <thread.h>
#include "sp_dir.h"
#include "soap/soap.h"
#include "module/prv.h"
#include "module/name.h"
#include "module/host.h"
#include "string.h"
#include "module/disk.h"
#include "module/disk.h"
 
#include <server/vdisk/disklabel.h>

#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "stats.h"
#include "stdio.h"
#include "stdlib.h"
#include "cache.h"
#include "bs_protos.h"
#include "grp_bullet.h"

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


#ifndef MAX_BULLET_THREADS
#define MAX_BULLET_THREADS	25
#endif

#ifndef MIN_MEMB_THREADS
#define MIN_MEMB_THREADS	1
#endif

#ifndef MIN_FILE_THREADS
#define MIN_FILE_THREADS	1
#endif

#ifndef MIN_SUPER_THREADS
#define MIN_SUPER_THREADS	1
#endif

#ifndef DEF_BULLET_MEMSIZE
#define	DEF_BULLET_MEMSIZE	2*1024*1024
#endif

#ifdef AMOEBA
static int	num_threads = MAX_BULLET_THREADS;
static int	num_memb_threads;
static int	num_file_threads;
static int	num_super_threads;
int		bs_sep_ports;
#endif

/* Flag for existance of separate super & file port */
int		bs_sep_ports;

static int	memsize = DEF_BULLET_MEMSIZE;
static Inodenum	Inodes_in_cache;

#define STACKSIZE 8192

extern	Statistics	Stats;
extern	Superblk	Superblock;
extern	Superblk	DiskSuper;

/* Capability of the disk server */
static	capability	Diskcap;

/* The user version of the bullet server needs the name of the virtual disk */
static char *		Disk_name;

/* The user version of the bullet server allows naming of capability paths */
#ifdef AMOEBA
#define	OPT_USAGE	"[-c capability] [-i numinodes] [-m memsize] [-t nthread] [vdiskname]"
#define	OPTIONS		"i:m:t:"
#else
#define	OPT_USAGE	"[-c filename] [-i numinodes] [-m memsize] [vdiskname]"
#define	OPTIONS		"i:m:"
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
    char	*fail_reason ;

    while ((opt = getopt(argc, argv, OPTIONS)) != EOF)
    {
	switch (opt)
	{
	case 'i':
		Inodes_in_cache = strtol(optarg, (char **) 0, 0);
		if (Inodes_in_cache <= 0)
		{
		    fprintf(stderr,
			"Invalid value for number of inodes to cache: %d\n",
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
	capability *p, *getcap();

	if ((p = getcap("BULLET_VDISK")) == 0)
	    bpanic("no vdisk given and BULLET_VDISK not set");
	Diskcap = *p;
	Disk_name = "vdisk";
    }

    /* Initialize the server */
    if (bullet_init() == 0)
	exit(1);

    num_memb_threads= num_threads/3 ;
    if ( num_memb_threads<MIN_MEMB_THREADS ) {
	bwarn("Increased # member threads from %d to %d",
		num_memb_threads,MIN_MEMB_THREADS) ;
	num_memb_threads=MIN_MEMB_THREADS ;
    }

    /* Start threads for private port */
#ifdef AMOEBA
    {
	int i;

	i = num_memb_threads  ;
	while (i-- > 0)
	    if (thread_newthread(member_thread, STACKSIZE, (char *) 0, 0) == 0)
		bpanic("cannot start member threads");

    }
#endif

    if (thread_newthread(bullet_send_queue, STACKSIZE, (char *) 0, 0) == 0 )
	bpanic("cannot start send queue thread");
	    

    /* Start the server threads */
    fail_reason=bs_can_start_service();
#ifdef AMOEBA
    if ( !fail_reason ) {
	bs_start_service() ;
	if (!bs_build_group()) {
	    bpanic("cannot build group");
	}
    } else bwarn("postponing file service: %s",fail_reason);
    member_thread((char *) 0, 0);
#else
    if ( !fail_reason ) {
	bullet_thread();
    } else {
	bpanic("Can not start bullet service: %s",fail_reason);
    }
#endif
    exit(0);
    /*NOTREACHED*/
}


void
bs_start_service()
{
    int		i;

    if ( !bullet_start() ) exit(1) ;

    if ( PORTCMP(&Superblock.s_fileport,&Superblock.s_supercap.cap_port) ) {
	/* Super & file port are equal, switch all through file port */
	bs_sep_ports=0 ;
	num_super_threads=0  ;
    } else {
	/* Super and file port differ, make separate threads */
	bs_sep_ports=1 ;
        num_super_threads= num_threads/4 ;
        if ( num_super_threads<MIN_SUPER_THREADS ) {
	    bwarn("Increased # super threads from %d to %d",
		    num_super_threads,MIN_SUPER_THREADS) ;
	    num_super_threads=MIN_SUPER_THREADS ;
        }
    
    }

    num_file_threads= num_threads-num_memb_threads-num_super_threads-1 ;
    if ( num_file_threads<MIN_FILE_THREADS ) {
	bwarn("Increased # file threads from %d to %d",
		num_file_threads,MIN_FILE_THREADS) ;
	num_file_threads=MIN_FILE_THREADS ;
    }

    /* Start the threads */
    if (thread_newthread(bullet_sweeper, STACKSIZE, (char *) 0, 0) == 0 )
	bpanic("cannot start sweeper thread");
	    
    if (thread_newthread(bullet_back_sweeper, STACKSIZE, (char *) 0, 0) == 0 )
	bpanic("cannot start second sweeper thread");
    i = num_super_threads  ;
    while (i-- > 0)
	if (thread_newthread(super_thread, STACKSIZE, (char *) 0, 0) == 0)
		bpanic("cannot start server threads");
    i = num_file_threads ;
    while (--i)
	if (thread_newthread(bullet_thread, STACKSIZE, (char *) 0, 0) == 0)
		bpanic("cannot start file threads");
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
    errstat	err;

    /* Read block 0 into Superblock. */
    err=disk_read(&Diskcap, D_PHYS_SHIFT, (disk_addr) 0,
			    (disk_addr) 1, (bufptr) &DiskSuper);
    if ( err != STD_OK)
	bpanic("Bullet Server: Cannot access %s(%s)\n",
				Disk_name,err_why(err));

    /* If it is a valid superblock it is saved and we can go on */
    if (valid_superblk(&DiskSuper, &Superblock))
    {
	if (Inodes_in_cache != 0)
	{
	    if (Inodes_in_cache < Superblock.s_numinodes)
	    {
		bwarn("Only caching %d of the %d inodes in system.",
				Inodes_in_cache, Superblock.s_numinodes);
		Superblock.s_numinodes = Inodes_in_cache;
	    }
	    else
	    {
		fprintf(stderr,
			"Requested number of inodes to be cached (%d) ",
							Inodes_in_cache);
		fprintf(stderr,
			"exceeds available inodes (%d)\n",
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
    return disk_read(&Diskcap, (int) Superblock.s_blksize, from, blkcnt, to);
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
    return disk_write(&Diskcap, (int) Superblock.s_blksize, to, blkcnt, from);
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
    vprintf(fmt, args);
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
