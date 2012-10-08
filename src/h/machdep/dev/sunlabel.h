/*	@(#)sunlabel.h	1.2	94/04/06 16:46:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	Layout of a Sun Disk label for the Sun 3 family.
**	NB: sizeof (sun_label) must be 512 bytes.
**
** Author:
**	Greg Sharp
*/

#define	SUN_MAGIC		0xDABE	/* sun's magic number */
#define	SUN_MAX_PARTNS		8	/* max # of logical partitions */


typedef	struct sun_part_map	sun_part_map;
typedef	struct sun_label	sun_label;

struct sun_part_map
{
    long		sun_cylno;	/* starting cylinder */
    long		sun_nblk;	/* number of blocks */
};


/*
** SUN_FILLER_SIZE is set up to generate sun_label struct of the wrong size if
** unsigned short is not 2 bytes and disk_addr is not 4 bytes.
*/
#define	SUN_FILLER_SIZE	(512 - (8*SUN_MAX_PARTNS + 2*12))

struct sun_label
{
    char		sun_pad[SUN_FILLER_SIZE];
    unsigned short	sun_apc;	/* alternates per cylinder */
    unsigned short	sun_gap1;	/* size of gap 1 */
    unsigned short	sun_gap2;	/* size of gap 2 */
    unsigned short	sun_intlv;	/* interleave factor */
    unsigned short	sun_numcyl;	/* # of data cylinders */
    unsigned short	sun_altcyl;	/* # of alternate cylinders */
    unsigned short	sun_numhead;	/* # of r/w heads */
    unsigned short	sun_numsect;	/* # of sectors per track */
    unsigned short	sun_bhead;	/* identifies proper label location */
    unsigned short	sun_ppart;	/* physical partition # */
    sun_part_map	sun_map[SUN_MAX_PARTNS]; /* partition table */
    unsigned short	sun_magic;	/* identifies this label format */
    unsigned short	sun_cksum;	/* xor checksum of sector */
};
