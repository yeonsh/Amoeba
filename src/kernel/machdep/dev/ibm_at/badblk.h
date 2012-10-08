/*	@(#)badblk.h	1.4	94/04/06 09:17:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * The winchester controller is capable of detecting bad blocks on
 * the harddisk. These bad blocks can be remapped to alternate (good)
 * blocks. The advantage of this procedure is that the disk will
 * look smooth to any higher level layer.
 * The bad block table is stored (with its alternate blocks) into a special
 * partition at the end of the disk. This table covers the whole disk,
 * it is up to the individual operating system to use this information.
 */
#define	BB_MAGIC	0x050AA050	/* magic cookie */
#define	BB_NBADMAP	1023		/* number of bad blocks in BB table */
#define	BB_BBT_SIZE	16		/* bad block table size in blocks */

/*
 * This fits exactly into BB_BBT_SIZE disk sectors !
 */
struct badblk {
    long  bb_magic;		/* magic word */
    short bb_unused;		/* unused */
    short bb_count;		/* number of bad blocks in table */
    struct badmap {
	disk_addr bb_bad;	/* bad block */
	disk_addr bb_alt;	/* alternate block */
    } bb_badmap[BB_NBADMAP];
};

