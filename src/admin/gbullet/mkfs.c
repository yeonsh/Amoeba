/*	@(#)mkfs.c	1.1	96/02/27 10:01:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Author:
 * Modified:
 *	Ed Keizer        -  For group bullet file format
 */
 
/*
**	BULLET SERVER MAKE FILE SYSTEM (B_MKFS)
**
**	This routine returns the error status of any operation that failed
**	or STD_OK if all went well.  It simply writes a bullet server
**	superblock on block 0 of the virtual disk specified by the capability.
**	The parameters:
**	  disk block size:	Specified as the log2 of the size in bytes.
**				Default is physical block size.
**	  number of inodes:	Default is #disk blocks / 10.
**	  type of init:		1 if superblock for new file server
**				0 if superblock for storage server that
**				  should attach to existing file service.
**	  max. # members:	The maximum numbers of members in the
**				file service, irrelevant for init type 0.
**	  normal # replicas:	The number of replicas the server should
**				try to maintain. Irrelevant for init type 0.
**
**	The program finds out the disk size itself and works the rest out.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"
#include "module/disk.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "module/prv.h"
#include "module/rnd.h"

#include "stdlib.h"
#include "stdio.h"

static	Superblk	Super;

errstat		init_inodes _ARGS((capability *, int));

errstat
b_mkfs(cap, l2blksz, numinodes,first,maxmember,no_replicas,safe_rep,same_cap)
capability *	cap;
int		l2blksz;
int		numinodes;
int		first;
int		maxmember;
int		no_replicas;
int		safe_rep;
int		same_cap;
{
    uint16	checksum();
    errstat	disk_size();
    errstat	disk_write();

    errstat		error;
    disk_addr		numblocks;
    disk_addr		inodeblocks;	/* number of blocks for inodes */
    unsigned int	blksz;		/* size of disk block in bytes */
    int			members;	/* max # members */
    int			def_repl;	/* minimum # of replicas */
    int			thisserver;	/* Server # */

/* if l2blksz is unspecified the default is physical block size */
    if (l2blksz == 0)
	l2blksz = D_PHYS_SHIFT;

    if (l2blksz < D_PHYS_SHIFT || l2blksz > 14)
    {
	fprintf(stderr, "Invalid block size: %d\n", l2blksz);
	return STD_ARGBAD;
    }

/* get size of the disk */
    if ((error = disk_size(cap, l2blksz, &numblocks)) != STD_OK)
    {
	fprintf(stderr, "Cannot get size of disk\n");
	return error;
    }

/*
** if # inodes isn't specified then
**	# physical blocks in system / # physical blocks in average sized file
*/
    if (numinodes == 0)
	numinodes = (numblocks << (l2blksz - D_PHYS_SHIFT)) /
					(AVERAGE_FILE_SIZE >> D_PHYS_SHIFT);
    
    if (numinodes <= 1)
    {
	fprintf(stderr, "Insufficient inodes: %d\n", numinodes - 1);
	return STD_ARGBAD;
    }

/*
** First we find out how many disk blocks are needed to hold the inodes
** and then we work out the maximum number of inodes that fit in that many
** blocks.
*/
    blksz = 1 << l2blksz;
    inodeblocks = sizeof (Inode) * numinodes;
    inodeblocks = (inodeblocks + blksz - 1) / blksz;
    numinodes = inodeblocks * blksz / sizeof (Inode);
/* If maximum number of members is not specified set it to the maximum
   allowed. */

    if ( !first ) {
	members=0 ; def_repl=0 ; thisserver=S_UNKNOWN_ID ;
    } else { /* First */
	thisserver=0 ;
        if ( maxmember!=0 ) members=maxmember ; else members=S_MAXMEMBER ;
        if ( members<1 || members>S_MAXMEMBER ) 
        {
    	fprintf(stderr, "Numbers of members (%d) must lie between 1 and %d.\n",
    		members,S_MAXMEMBER);
    	return STD_ARGBAD;
        }
    
    /* determine usual numbers of replicas, the default is 2 */
        if ( no_replicas!=0 ) def_repl=no_replicas ; else def_repl=2 ;
        if ( def_repl<1 || def_repl>S_MAXMEMBER ) 
        {
    	fprintf(stderr, "Minimum number of replicas (%d) must lie between 1 and %d.\n",
    		def_repl,S_MAXMEMBER);
    	return STD_ARGBAD;
        }
    }

/* Sanity test on size of super block */
    if ( sizeof Super!=D_PHYSBLKSZ ) {
	fprintf(stderr,"Improper size of superblock structure (%d)\n",
		sizeof Super) ;
    }

/* Clear the superblock */
    memset(&Super,0,D_PHYSBLKSZ) ;

/* fill in the superblock */
    Super.s_magic = S_MAGIC;
    Super.s_version = S_VERSION;
    Super.s_maxmember = members;
    Super.s_member = thisserver ;
    Super.s_blksize = l2blksz;
    Super.s_numblocks = numblocks;
    /* Member ports are all different */
    uniqport(&Super.s_membercap.cap_port);
    uniqport(&Super.s_membercap.cap_priv.prv_random);
    if (prv_encode(&Super.s_membercap.cap_priv, (objnum) 0, (rights_bits) PRV_ALL_RIGHTS,
    				    &Super.s_membercap.cap_priv.prv_random) < 0)
	return STD_CAPBAD;
    
    if ( first ) {
	/* Set ports of first server and initialize id 0 */
	uniqport(&Super.s_groupcap.cap_port);
	uniqport(&Super.s_groupcap.cap_priv.prv_random);
        if (prv_encode(&Super.s_groupcap.cap_priv, (objnum) 0, (rights_bits) PRV_ALL_RIGHTS,
    				    &Super.s_groupcap.cap_priv.prv_random) < 0)
	    return STD_CAPBAD;
        uniqport(&Super.s_supercap.cap_port);
        uniqport(&Super.s_supercap.cap_priv.prv_random);
        if (prv_encode(&Super.s_supercap.cap_priv, (objnum) 0, (rights_bits) PRV_ALL_RIGHTS,
    				    &Super.s_supercap.cap_priv.prv_random) < 0)
	    return STD_CAPBAD;

	if ( same_cap ) {
	    /* file & super ports are the same */
	    Super.s_fileport=Super.s_supercap.cap_port ;
	} else {
	    uniqport(&Super.s_fileport);
	}  
    
	Super.s_memstatus[0].ms_status=MS_ACTIVE ;
	/* Deduce put-port from get-port */
	priv2pub(&Super.s_membercap.cap_port,
		 &Super.s_memstatus[0].ms_cap.cap_port) ;
	Super.s_memstatus[0].ms_cap.cap_priv=Super.s_membercap.cap_priv ;
#ifdef OLD_VERSION
	/* Give first one control over all inodes */
	Super.s_firstfree=1 ;
	Super.s_lastfree= numinodes ;
#endif
	Super.s_def_repl = def_repl ;
	Super.s_flags= (safe_rep?S_SAFE_REP:0) ;
	Super.s_numinodes = numinodes;
	Super.s_inodetop = inodeblocks;
	if (Super.s_inodetop > Super.s_numblocks)
	{
	    fprintf(stderr, "Disk space for inodes > size of the disk\n");
	    return STD_ARGBAD;
	}
    }

/* warn the user about what they are getting */
    if ( first ) {
	printf("Initializing new file service\n");
	printf("Maximum number of storage servers = %d\n",members);
	printf("Default replication count = %d\n", def_repl);
/* print one less than real inode count since inode 0 cannot be used */
	printf("Number of inodes = %d\n", numinodes - 1);
	printf("No. blocks for inodes = %ld\n", Super.s_inodetop);
    } else {
	printf("Initializing additional storage server\n");
    }
    printf("Blocksize = %d bytes\n", (1 << l2blksz));
    printf("Sizeof disk = %ld blocks\n", numblocks);

/*
** Zero the blocks for the inode before writing the superblock,
** since the existence of a valid superblock implies a valid file system
** and the inodes must be *successfully* zeroed before that is made.
*/
    if ( first ) {
	error=init_inodes(cap,l2blksz);
	if ( error!=STD_OK ) return error ;
    }

/* encode to big endian */
    enc_l_be(&Super.s_magic);
    enc_s_be(&Super.s_version);
    enc_s_be(&Super.s_maxmember);
    enc_s_be(&Super.s_member);
    enc_s_be(&Super.s_def_repl);
    enc_s_be(&Super.s_flags);
    enc_s_be(&Super.s_blksize);
#ifdef OLD_VERSION
    ENC_INODENUM_BE(&Super.s_firstfree);
    ENC_INODENUM_BE(&Super.s_lastfree);
#endif
    ENC_INODENUM_BE(&Super.s_numinodes);
    ENC_DISK_ADDR_BE(&Super.s_numblocks);
    ENC_DISK_ADDR_BE(&Super.s_inodetop);
    enc_s_be(&Super.s_memstatus[0].ms_status);

/* put in checksum */
    Super.s_checksum = 0;
    Super.s_checksum = checksum((uint16 *) &Super);

/* write out the superblock */
    return disk_write(cap, D_PHYS_SHIFT, (disk_addr) 0,
					    (disk_addr) 1, (char *) &Super);
}

errstat
init_inodes(cap,l2blksz)
capability *	cap;
int		l2blksz;
{
    char *		initblock;
    int			i;
    unsigned int	blksz;		/* size of disk block in bytes */
    errstat		error;
    Inode		*ip ;

    blksz = 1 << l2blksz;
/* allocate a block of zeroes the size of a diskblock */
    if ((initblock = (char *) calloc(1, blksz)) == 0)
	return STD_NOMEM;

    /* Initialize the inodes in the inode block, to version number 1 */
    for( ip= (Inode *)initblock, i=0 ; i< blksz/sizeof(Inode) ; i++, ip++ ){
	ip->i_version=1 ;
	ENC_VERSION_BE(&ip->i_version);
    }

    /* Repeatedly write the inode block over the inode area */
    for (i = 1; i <= Super.s_inodetop; i++)
    {
	error = disk_write(cap, l2blksz, i, (disk_addr) 1, initblock);
	if (error != STD_OK)
	    return error;
    }
    return STD_OK ;
}

/*
 * check_disk
 *	Returns STD_OK if the disk specified by disk_cap does not contain
 *	a bullet file system.  Otherwise it returns an error code.
 */
errstat
check_disk(disk_cap)
capability *	disk_cap;
{
    uint16      checksum();

    uint16      sum;
    errstat 	err;

    err = disk_read(disk_cap, D_PHYS_SHIFT, (disk_addr) 0, (disk_addr) 1,
							    (char *) &Super);
    if (err != STD_OK)
	return err;

    /* calculate superblock checksum */
    sum = checksum((uint16 *) &Super);

    /* decode the data from big endian to current architecture */
    dec_l_be(&Super.s_magic);
    if ( (Super.s_magic == S_MAGIC || Super.s_magic == S_OLDMAGIC) && sum == 0)
        return STD_EXISTS;
    return STD_OK;
}
