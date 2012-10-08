/*	@(#)nvram.h	1.1	96/02/27 10:36:23 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * If we have a significant chunk of NVRAM then it is useful for caching
 * disk writes.  The following data structures are used for storing the
 * disk block cache information.
 *
 * Author: Gregory J. Sharp, Jan 1995
 */

#ifndef __NVRAM_H__
#define	__NVRAM_H__

#include "disk/disk.h"

/*
 * NB. Because different compilers will align things in different ways and
 * we must ensure that alignments are the same for as many compilers as
 * possible, structs should be multiples of 8 bytes.
 */


/***********************************************************/
/* The data structures for information stored in the NVRAM */
/***********************************************************/

/*
 * Magic numbers to identify if the NVRAM contains data that we might
 * have put in it.
 */

#define	NV_MAGIC_1	0x346B092
#define	NV_MAGIC_2	0x58C72A0

typedef	struct nv_magic	nv_magic_t;
struct nv_magic
{
    uint32	m1;
    uint32	m2;
};


/*
 * For each disk device attached to the disk server we need to store
 * sufficient information in NVRAM to make sure that when we start
 * after a crash that it is the same device as before the crash.
 *
 * nv_devaddr:	The controller id for the device.
 * nv_unit:	The unit # of the device on the controller.
 * nv_read:
 * nv_write:	The read and write function pointers for the device.
 *		They may change from after a reboot.  They are stored
 *		here for convenience.
 * nv_lchksum:	This is the 16-bit xor checksum from the disk label (block 0).
 *		If this has changed after a crash then the disk label has been
 *		modified without our knowing (unlikely) or there is a different
 *		disk on the controller.
 */

typedef	struct nv_dev	nv_dev_t;
struct nv_dev
{
    long	nv_devaddr;
    int32	nv_unit;
    errstat	(*nv_read)();
    errstat	(*nv_write)();
    uint16	nv_lchksum;
    uint16	pad[3]; /* To 8-byte align */
};


/*
 * The table of known disks is small.  We don't expect more than MAXTABSZ
 * disks.  This struct goes at the beginning of the NVRAM.
 */

#define	MAXTABSZ	20	/* Cannot exceed 31 */

typedef struct nv_devtab	nv_devtab_t;
struct nv_devtab
{
    nv_magic_t	nv_magic1;
    nv_dev_t	nv_devs[MAXTABSZ];
    nv_magic_t	nv_magic2;
};


/*
 * The second table to be stored in NVRAM is the list of disk blocks in
 * the NVRAM showing which device and disk block # they correspond to.
 *
 * nv_blknum:	Address of block on specified disk device.
 * nv_devinfo: 	bit 7:
 *			Valid bit: There is data in the page.
 *		bit 6:
 *			Dirty bit: The data is not yet on disk.  This bit
 *				   only has meaning if the valid bit is set.
 *		bits 0-5:
 *			Device id: An index into the nv_devtab table.
 */

typedef struct nv_blk	nv_blk_t;
struct nv_blk
{
    disk_addr	nv_blknum;
    uint8	nv_devinfo;
    uint8	pad[3];
};

#define	NV_VALID_BIT	0x80
#define	NV_DIRTY_BIT	0x40

typedef struct nv_blkcnt	nv_blkcnt_t;
struct nv_blkcnt
{
    uint32	blkcnt;
    uint32	pad;
};

/*
 * And now for the tricky bit:
 *   At this stage we don't know how much NVRAM we'll get so we work out
 *   how many nv_blk_t entries we need at run time.  In front of the
 *   table and at the end we set some magic numbers as a check on the
 *   validity of the memory.  After the magic # is the count of the #
 *   nv_blk_t structs in the table.
 */


#define	NV_MIN_SIZE	(8*1024)


/***************************/
/* In-core data structures */
/***************************/


/*
 * For each piece of nv_ram that we find we keep the following information.
 * (ie, you can have more than one NVSIMM of SBUS card.)
 */

typedef struct nv_ram	nv_ram_t;
struct nv_ram
{
    nv_ram_t *	nvr_next;	/* Linked list */
    vir_bytes	nvr_addr;	/* Kernel virtual address of the NVRAM */
    vir_bytes	nvr_size;	/* Size of NVRAM in bytes */
    vir_bytes	nvr_skip;	/* The 1st skip bytes are unusable */
    int		nvr_dvma;	/* 1 => DVMA direct to NVRAM is possible */
};


/*
 * For free lists and in-use lists of NVRAM:
 * NVRAM is handed out in chunks, typically larger than one 512 byte block.
 */

typedef struct nv_chunk	nv_chunk_t;
struct nv_chunk
{
    nv_chunk_t *	nvc_next;
    nv_chunk_t *	nvc_prev;
    vir_bytes		nvc_start;	/* Address in NVRAM */
    vir_bytes		nvc_numblks;	/* # contiguous blocks in chunk */
    nv_dev_t *		nvc_pte;	/* Pointer to page table in NVRAM */
};

#endif /* __NVRAM_H__ */
