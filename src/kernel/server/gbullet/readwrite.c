/*	@(#)readwrite.c	1.1	96/02/27 14:07:34 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * READWRITE.C
 *
 *	This file contains the routines for validating the superblock and
 *	reading the inode table into core and unpacking them and the routines
 *	to pack an inode and write it to disk.
 *
 *  Author: 
 *      Greg Sharp  12-01-89
 *  Modified:
 *	Ed Keizer, 1995 - to fit in group bullet
 */

#include "amoeba.h"
#include "assert.h"
#ifdef INIT_ASSERT
INIT_ASSERT
#endif /* INIT_ASSERT */
#include "cmdreg.h"
#include "stderr.h"
#include "byteorder.h"
#include "group.h"

#include "module/disk.h"
#include "module/mutex.h"
#include "gbullet/bullet.h"
#include "gbullet/inode.h"
#include "gbullet/superblk.h"
#include "cache.h"
#include "alloc.h"
#include "bs_protos.h"
#include "grp_bullet.h"
#include "event.h"
#include "grp_defs.h"

/* In core copy of the super block */
extern Superblk	Superblock;

/* Pointer to in core copy of the inode table */
extern Inode *	Inode_table;

/* Size of a disk block in bytes */
extern b_fsize	Blksizemin1;

/* The size of the super blocks, also start of inodes */
extern disk_addr	bs_super_size ;

/* Local definitions */
static void delay_write _ARGS((disk_addr)) ;
static void clear_delay_write _ARGS((disk_addr)) ;

/* Data for delayed inode write's */
/* These are typically writes resulting from inode changes through
 * recv_inodes.
 * This is needed because otherwise delays in handling incoming
 * inodes will be too long.
 */
static mutex		guard_block_list ;
/* Points to array with one byte per block */
static char		*block_list ;

/*
 * VALID_SUPERBLK
 *
 *	This routine looks at Superblock to it to see if it is a valid
 *	superblock.   If it is it returns 1. Otherwise it returns 0.
 *	The superblock is stored in big-endian format on the disk.  This
 *	routine unpacks it to the "endian" of the host architecture. 
 * 
 */

int
valid_superblk(Disk, Super)
	Superblk	*Disk;
	Superblk	*Super;
{
    uint16	checksum();

    uint16	sum;
    int		i;

    /* Calculate superblock checksum */
    sum = checksum((uint16 *) Disk);

    *Super = *Disk ;
    /* Decode the data from big endian to current architecture */
    dec_l_be(&Super->s_magic);
    if (Super->s_magic != S_MAGIC || sum != 0) {
#ifdef USER_LEVEL
	message("invalid superblock: magic = 0x%lx; sum = 0x%lx\n",
	       Super->s_magic, sum);
#endif
	return 0;
    }
    dec_s_be(&Super->s_version);
    if ( Super->s_version!=S_VERSION ) {
	message("invalid superblock: version = 0x%lx\n", Super->s_version);
	return 0 ;
    }
    dec_s_be(&Super->s_blksize);
    DEC_INODENUM_BE(&Super->s_numinodes);
    DEC_DISK_ADDR_BE(&Super->s_numblocks);
    DEC_DISK_ADDR_BE(&Super->s_inodetop);
    Blksizemin1 = (1 << Super->s_blksize) - 1;
    dec_s_be(&Super->s_maxmember);
    dec_s_be(&Super->s_member);
    dec_s_be(&Super->s_def_repl);
    dec_s_be(&Super->s_flags);
    dec_s_be(&Super->s_seqno);
    if ( Super->s_maxmember>S_MAXMEMBER ) {
	message("invalid superblock: s_maxmember = %d\n",
	       Super->s_maxmember);
	return 0 ;
    }
    for ( i=0 ; i<Super->s_maxmember ; i++ ) {
	dec_s_be(&Super->s_memstatus[i].ms_status) ;
    }
    return 1;
}


/*
 * WRITE_SUPERBLK
 *
 *	This routine writes a new Super Block to disk.
 */

void
write_superblk(Disk)
	Superblk	*Disk;
{
    dbmessage(31,("Writing super block, %d disk blocks",bs_super_size)) ;
    if (bs_disk_write((bufptr) Disk, (disk_addr) 0, bs_super_size)!= STD_OK)
	bpanic("write_superblk: bs_disk_write failed");
}


/*
 * READ_INODETAB
 *
 *	This routine reads the inode table from disk.  The inode table is
 *	stored in big-endian format on the disk.  It remains in BE format
 *	in core and is only decoded when the inode is cached.
 */

void
read_inodetab(numblks)
disk_addr	numblks;
{
    /*
     * Read inode table into top of buffer cache
     * memory since disk can DMA to there.
    */
    if (bs_disk_read(bs_super_size, (bufptr) Inode_table, numblks) != STD_OK)
	bpanic("read_inodetab: bs_disk_read failed");
}


/*
 * WRITE_INODETAB
 *
 *	This routine writes the whole inode table to disk.
 *	happens only when not providing full service, no locks necessary
 */

void
write_inodetab(numblks)
disk_addr	numblks;
{
    if (bs_disk_write((bufptr) Inode_table, bs_super_size, numblks)!= STD_OK)
	bpanic("write_inodetab: bs_disk_write failed");
}


/*
 * WRITECACHE
 *
 *	This routine encodes an inode into big endian format and
 *	writes it away to disk.  Note, it is usually not necessary to write the
 *	entire inode table since the on-disk copy is always kept up to date.
 */

void
writecache(cp,mode)
Cache_ent *	cp;
int		mode;
{
    Inode *	ip;

    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(!(cp->c_inode.i_flags&I_INTENT));
    ip = Inode_table + cp->c_innum;
    lock_ino_cache(cp->c_innum) ;
    /* Do not touch ownership, ttl's, version, random */
    ip->i_flags= cp->c_inode.i_flags ;
    ip->i_dskaddr= cp->c_inode.i_dskaddr ;
    ip->i_size= cp->c_inode.i_size ;
    ENC_DISK_ADDR_BE(&ip->i_dskaddr);
    ENC_FILE_SIZE_BE(&ip->i_size);
    if (!(cp->c_flags & NOTCOMMITTED))	/* only write committed inodes! */
    {
	writeinode(ip,ANN_DELAY|ANN_MINE,mode);
	cp->c_flags &= ~WRITEINODE;
    } else {
	unlock_ino_cache(cp->c_innum) ;
    }
}

/* Lock a block in the inode cache.
 * This is used when changes to an inode are intended.
 * The inode should first be locked, then the block in which the changes take
 * place. After the changes the lock on the block should be released.
 * Either by a call to unlock_ino_cache or to write_inode.
 */

void
lock_ino_cache(inode)
    Inodenum	inode ;
{
    disk_addr	block;	/* diskblock where inode should be written */

    block = (((inode) * sizeof (Inode)) >> Superblock.s_blksize) + bs_super_size;

    typed_lock(BLOCK_LCLASS,(long)block);
}

void
unlock_ino_cache(inode)
    Inodenum	inode ;
{
    disk_addr	block;	/* diskblock where inode should be written */

    block = (((inode) * sizeof (Inode)) >> Superblock.s_blksize) + bs_super_size;

    typed_unlock(BLOCK_LCLASS,(long)block);
}

/*
 * WRITEINODE
 *
 *	This routine figures out which block of memory the in core inode
 *	is in and writes that block to the appropriate place on the disk.
 *	Note that inodes occupy blocks bs_super_size to
 *	Superblock.s_inodetop on the disk, so we need to add the size of
 *	the super block to the block number.
 *	This routine assumes that the block containg the inode
 *	was locked with lock_ino_cache.
 */

void
writeinode(ip,announce,mode)
Inode *	ip;
int	announce;
int     mode;
{
    bufptr	caddr;	/* pointer to cache memory block holding inode */
    disk_addr	block;	/* diskblock where inode should be written */
    errstat	err;	/* status from bs_disk_write */
    Inodenum	inode;	/* number of inode ip */
    int		can_delay ;

    inode = ip - Inode_table;
    assert(inode_locked_by_me(inode) == 2);
    /* Must point to start of block containing inode */
    caddr = (bufptr) (((long) ip) & ~Blksizemin1);
    block = (((inode) * sizeof (Inode)) >> Superblock.s_blksize) +bs_super_size;

    /* See whether we can delay writing */
    can_delay=0 ;
    switch( announce&ANN_MASK ) {
    case ANN_NEVER:
	can_delay=1 ;
	break ;
    case ANN_MINE:
	if ( ip->i_owner!=Superblock.s_member ) can_delay=1 ;
    }
    if ( can_delay ) {
	delay_write(block) ;
    } else {
        if (block < bs_super_size || block > Superblock.s_inodetop)
	    bpanic("writeinode: illegal inode %d", inode);
    
	/* backgrounder can assume it is written now */
	/* To avoid a race we do this BEFORE writing the block */
	clear_delay_write(block) ;

        err = bs_disk_write(caddr, block, (disk_addr) 1) ;
        if ( err  != STD_OK )
	    err = bs_disk_write(caddr, block, (disk_addr) 1) ;
    
        if ( err  != STD_OK )
	    bpanic("writeinode: bs_disk_write failed (%d)", err);
    }
    typed_unlock(BLOCK_LCLASS,(long)block);

    /* Send inode data to others after write succeeded */
    switch( announce&ANN_MASK ) {
    case ANN_NEVER:
	return ;
    case ANN_MINE:
	if ( ip->i_owner!=Superblock.s_member ) return ;
    case ANN_ALL:
	if ( announce&ANN_NOW ) {
	    bs_send_inodes(inode,(Inodenum)1,0,1,mode) ;
	} else {
	    bs_publish_inodes(inode,(Inodenum)1,mode) ;
	}
    }
    
}


/*
 * WRITEFILE
 *
 *	This routine writes the data of a file from the cache to disk.
 */

void
writefile(cp)
Cache_ent *	cp;
{
    disk_addr	nblks;
    disk_addr	daddr;
    errstat	err;

    assert(inode_locked_by_me(cp->c_innum) == 2);
    assert(cp->c_flags & WRITEDATA);

    cp->c_flags &= ~WRITEDATA;

    /* Calculate the number of blocks in the file */
    if ((nblks = NUMBLKS(cp->c_inode.i_size)) == 0)
	return;	/* nothing to write */

    daddr = cp->c_inode.i_dskaddr;

    /* Get the data written */
    if (daddr <= Superblock.s_inodetop)
	bpanic("writefile: trying to write data to inode disk area.");

    /* Check that the allocated diskblocks exist! */
    if (daddr >= Superblock.s_numblocks ||
				    daddr + nblks > Superblock.s_numblocks)
    {
	message("writefile: trying to write to non-existent disk blocks");
	message("writefile:   disk address = %lx, #blks = %lx", daddr, nblks);
	message("writefile:   max diskblock = %lx", Superblock.s_numblocks);
	bpanic("writefile: internal inconsistency.");
    }

    assert(cp->c_addr);
    /* Try the write again if it fails the first time */
    if ((err = bs_disk_write(cp->c_addr, daddr, nblks)) != STD_OK
		&& (err = bs_disk_write(cp->c_addr, daddr, nblks)) != STD_OK)
    {
	message("bs_disk_write: addr=%d, nblks=%d, err=%d", daddr, nblks, err);
	bpanic("writefile: bs_disk_write failed");
    }
}

void
bs_init_irw() {
	Res_size	numbytes ;

	numbytes=Superblock.s_inodetop+1 ;
	CEILING_BLKSZ(numbytes);
	block_list = (char *)a_alloc(A_CACHE_MEM, numbytes);
	if ( !block_list ) bpanic("no memory for block list") ;
}

static void
clear_delay_write(block)
	disk_addr	block ;
{
	mu_lock(&guard_block_list) ;
	assert( block_list!=0 ) ;
	block_list[block]= 0 ;
	mu_unlock(&guard_block_list) ;
}

static void
delay_write(block)
	disk_addr	block ;
{
	mu_lock(&guard_block_list) ;
	assert( block_list!=0 ) ;
	block_list[block]= 1 ;
	mu_unlock(&guard_block_list) ;
}

void
bs_block_flush()
{
	char		*next_block ;
	bufptr		caddr;	/* pointer to cache memory block holding inode*/
	disk_addr	block;	/* diskblock where inode should be written */
	errstat		err;	/* status from bs_disk_write */
#ifdef DEBUG_MSGS
	int		inode_per = (1<<Superblock.s_blksize)/ sizeof (Inode) ;
#endif

	mu_lock(&guard_block_list) ;
	for ( next_block= block_list+bs_super_size ;
	      next_block<=block_list+Superblock.s_inodetop ; next_block++ ) {
		if ( *next_block ) {
			/* We need to write */
			/* Hit the delayed write bit first */
			*next_block= 0 ;
			/* Then leave the table open for setting bits */
			mu_unlock(&guard_block_list) ;
			block= next_block-block_list ;
caddr = (char *)Inode_table + ((block-bs_super_size) << Superblock.s_blksize);
			dbmessage(2,("delayed: inodes %ld..%ld",
				(block-bs_super_size)*inode_per,
				(block-bs_super_size+1)*inode_per-1));
			typed_lock(BLOCK_LCLASS,(long)block);
			err = bs_disk_write(caddr, block, (disk_addr) 1) ;
			if ( err  != STD_OK )
			    err = bs_disk_write(caddr, block, (disk_addr) 1) ;
		    
			typed_unlock(BLOCK_LCLASS,(long)block);
			if ( err  != STD_OK ) {
			    bwarn("block_flush: bs_disk_write failed (%d)",err);
			    delay_write(block) ; /* reset the flag */
			    return ;		/* and forget about flushing */
			}
			/* get the table again */
			mu_lock(&guard_block_list) ;
		}
	}
	mu_unlock(&guard_block_list) ;
}
