/*	@(#)gb_info.c	1.1	96/02/27 10:01:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Author: Ed Keizer
 */
 

/*
	Bullet server get information utility.

	This utility can be used to print out information from
	the super block of a storage server.
	It also provides a way to get at the capabilities stored in
	the superblock.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"
#include "module/name.h"
#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/ar.h"

#include "stdio.h"
#include "stdlib.h"

int		getopt();
static void	usage();

extern char *	optarg;
extern int	optind;

extern uint16	checksum();

static void		cap_store();

#define		T_GET		1
#define		T_PUT		0

char		*progname;

char		mbuf[20] ;

/* Flags */
int		port_type= T_PUT ;	/* Type of port to get */
int		verbose;
char		*storage_cap;		/* Cap of storage server */
char		*log_cap;		/* Cap of log server */
char		*bullet_cap;		/* Cap of file service */

char *memstat[] = { "none", "new", "active", "passive" } ;

valid_superblk(super)
Superblk *	super;
{
    uint16	sum;

    /* Calculate superblock checksum */
    sum = checksum((uint16 *) super);

    dec_l_be(&super->s_magic);
    if ( !(super->s_magic == S_MAGIC || super->s_magic == S_OLDMAGIC) || sum != 0)
	return 0;
    return 1;
}


main(argc, argv)
int	argc;
char *	argv[];
{
    Superblk	super;
    capability	cap;
    char *	diskname;
    errstat	status;
    int		i;
    char	*mstat ;
    int		opt;

    progname = argv[0];
/* process arguments */
    while ((opt = getopt(argc, argv, "gvs:l:b:")) != EOF)
	switch (opt)
	{
	case 'v':
	    verbose = 1;
	    break;

	case 'g':
	    port_type = T_GET;
	    break;

	case 's':
	    storage_cap= optarg; 
	    break;

	case 'l':
	    log_cap= optarg; 
	    break;

	case 'b':
	    bullet_cap= optarg ; 
	    break;

	default:
	    usage(progname);
	    /*NOTREACHED*/
	}

    if (optind != argc - 1)
    {
	usage(progname);
	/*NOTREACHED*/
    }
    diskname = argv[optind];
    

    /* get capability for disk */
    if ((status = name_lookup(diskname, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
					progname, diskname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    /* get old superblock from disk */
    if ((status = disk_read(&cap, D_PHYS_SHIFT, (disk_addr) 0,
				(disk_addr) 1, (char *) &super)) != STD_OK)
    {
	fprintf(stderr, "%s: disk_read failed: %s\n", 
						    progname, err_why(status));
	exit(1);
	/*NOTREACHED*/
    }

    if (!valid_superblk(&super))
    {
	fprintf(stderr,"%s: %s does not have a valid super block\n",
		progname,diskname);
	exit(1);
    }
    if ( super.s_magic == S_OLDMAGIC ) {
	fprintf(stderr,"%s: %s has an old-style super block\n",
		progname,diskname);
	exit(1);
    }
    dec_s_be(&super.s_version);
    if ( super.s_version != S_VERSION ) {
	fprintf(stderr,"%s: warning, expected version %d, got version %u super block\n",
		progname,S_VERSION,super.s_version);
    }
    if ( storage_cap ) {
	cap_store(&super.s_membercap,storage_cap,T_GET,port_type) ;
    }
    if ( bullet_cap ) {
	cap_store(&super.s_supercap,bullet_cap,T_GET,port_type) ;
    }
    if ( log_cap ) {
	cap_store(&super.s_logcap,log_cap,T_GET,port_type) ;
    }
    if ( verbose ) {
	printf("Superblock information:\n") ;
	printf("Version              %8u\n",super.s_version) ;
	dec_s_be(&super.s_member);
	if ( super.s_member == S_UNKNOWN_ID ) {
	    printf("Server #              unknown\n") ;
	} else {
	    printf("Server #             %8u\n",super.s_member) ;
	}
	dec_s_be(&super.s_flags);
	if ( super.s_flags ) {
	    printf("Flags                %#8x\n",super.s_flags);
	}
	dec_s_be(&super.s_def_repl);
	printf("Default rep. count   %8u\n",super.s_def_repl) ;
	dec_s_be(&super.s_blksize);
	printf("Log 2 of Block size  %8u\n",super.s_blksize) ;
	DEC_DISK_ADDR_BE(&super.s_numblocks);
	printf("Number of blocks     %8ld\n",(long)super.s_numblocks) ;
	DEC_INODENUM_BE(&super.s_numinodes);
	printf("Number of inodes     %8ld\n",(long)super.s_numinodes) ;
	DEC_DISK_ADDR_BE(&super.s_inodetop);
	printf("Highest inode block  %8ld\n",(long)super.s_inodetop) ;
	printf("Group capability          %s\n",ar_cap(&super.s_groupcap)) ;
	printf("Super capability          %s\n",ar_cap(&super.s_supercap)) ;
	printf("Storage server capability %s\n",ar_cap(&super.s_membercap)) ;
	printf("File port                 %s\n",ar_port(&super.s_fileport)) ;
	printf("Log capability            %s\n",ar_cap(&super.s_logcap));
	dec_s_be(&super.s_seqno);
	printf("Sequence number      %8u\n",super.s_seqno);
	dec_s_be(&super.s_maxmember);
	if (super.s_maxmember==0) printf("no member information present\n") ;
	for ( i=0 ; i<super.s_maxmember ; i++ ) {
	    dec_s_be(&super.s_memstatus[i].ms_status);
	    if ( super.s_memstatus[i].ms_status >= (sizeof memstat)/sizeof (char *) ) {
		sprintf(mbuf,"?%u",super.s_memstatus[i].ms_status) ;
		mstat= mbuf ;
	    } else {
		mstat= memstat[super.s_memstatus[i].ms_status] ;
	    }
	    printf("%3d:  %7s %s\n",i,mstat,
		ar_cap(&super.s_memstatus[i].ms_cap));
	}
    }
    exit(0);
    /*NOTREACHED*/
}

static void
usage(prog_name)
	char	*prog_name;
{
	fprintf(stderr,"usage: %s [-g] [-v] [-s name] [-l name] [-b name] diskcap\n",
		prog_name);
	exit(9);
}

static void
cap_store(cap,path,type,type_wanted)
	capability	*cap;
	char		*path;
	int		type;
	int		type_wanted;
{
    errstat		err ;
    capability		oldcap ;
    capability		outcap ;

    /* Convert to public cap if needed */
    outcap= *cap ;
    if ( type!=type_wanted ) {
	if ( type_wanted==T_PUT ) {
	    priv2pub(&cap->cap_port,&outcap.cap_port);
	} else {
	    fprintf(stderr,"%s: can not convert putport to getport(%s)\n",
		progname,path) ;
	    return ;
	}
    }

    if ((err = name_lookup(path, &oldcap)) == STD_OK) {
        err= name_replace(path,&outcap);
        if ( err != STD_OK && err != STD_NOTFOUND ) {
            fprintf(stderr, "%s: name_replace %s failed (%s)\n",
                                    progname, path, err_why(err));
            exit(1);
        }
        /* Fall through on NOTFOUND & OK */
    }
    switch( err ) {
    case STD_NOTFOUND:
        if ((err = name_append(path, &outcap)) != STD_OK) {
            fprintf(stderr, "%s: name_append %s failed (%s)\n",
                                    progname, path, err_why(err));
            exit(1);
        }
    case STD_OK:
        break ;
    default:
        fprintf(stderr, "%s: name_lookup %s failed (%s)\n",
                                progname, path, err_why(err));
        exit(1);
    } 
}
