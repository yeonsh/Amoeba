/*	@(#)showbadblk.c	1.5	96/02/27 10:06:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * badblk.c
 *
 * Display the contents of a bad block table on an ISA winchester hard disk.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <assert.h>
#include <amtools.h>
#include <stdio.h>
#include <ctype.h>
#include <module/disk.h>
#include <badblk.h>
#include <pclabel.h>
#include <vdisk/disklabel.h>

void blk_to_hst();

struct geometry geom;

main(argc, argv)
    int argc;
    char **argv;
{
    char pclabel[PCL_BOOTSIZE];
    char *program, *diskname;
    struct pcpartition *pe;
    int cyl, head, sector;
    struct badblk badblk;
    capability diskcap;
    errstat err;
    int i;

    program = argv[0];
    diskname = argv[1];
    if (argc != 2) {
	fprintf(stderr, "Usage: %s disk-capability\n", program);
	exit(1);
    }

    if ((err = name_lookup(argv[1], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: lookup %s failed (%s)\n",
	    program, argv[1], err_why(err));
	exit(1);
    }

    /*
     * Display the disk's geometry as extra information
     */
    if ((err = disk_getgeometry(&diskcap, &geom)) != STD_OK) {
	fprintf(stderr, "%s: disk_geometry error: %s\n", program, err_why(err));
	exit(1);
    }
    printf("(%s: %ld cylinders, %d heads, %d sectors/head)\n",
	diskname, geom.g_numcyl, geom.g_numhead, geom.g_numsect);

    /*
     * Find bad block partition, and read the bad block table
     */
    if ((err=disk_read(&diskcap, PCL_LG2BOOTSIZE, 0L, 1L, pclabel)) != STD_OK) {
	fprintf(stderr, "%s: disk_read error: %s\n", program, err_why(err));
	exit(1);
    }
    if (!PCL_MAGIC(pclabel)) {
	fprintf(stderr, "%s: invalid master boot block\n", program);
	exit(1);
    }
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++)
	if (pe->pp_sysind == PCP_BADBLK)
	    break;
    if (i == PCL_NPART) {
	fprintf(stderr, "%s: no bad block partion found\n", program);
	exit(1);
    }
#ifdef DEBUG
    printf("Bad block table at %07d (x%08lx)\n", pe->pp_first, pe->pp_first);
#endif
    if ((err = disk_read(&diskcap, D_PHYS_SHIFT,
      (long)pe->pp_first, (long)BB_BBT_SIZE, (char *) &badblk)) != STD_OK) {
	fprintf(stderr, "%s: bad block table read failed: %s\n",
	    program, err_why(err));
	exit(1);
    }
    if (badblk.bb_magic != BB_MAGIC) {
	fprintf(stderr, "%s: bad block table has bad magic number\n", program);
	exit(1);
    }

    /*
     * Print contents of bad block table
     */
    printf("\nBad Block Table Summary (%d bad blocks)\n", badblk.bb_count);
    printf("   Block  Cyl  Head Sector    Alternate  Cyl Head Sector\n");
    for (i = 0; i < badblk.bb_count; i++) {
	blk_to_hst(badblk.bb_badmap[i].bb_bad, &cyl, &head, &sector);
	printf(" %7ld %4d    %2d     %2d",
	    badblk.bb_badmap[i].bb_bad, cyl, head, sector);

	if (badblk.bb_badmap[i].bb_alt != 0) {
	    blk_to_hst(badblk.bb_badmap[i].bb_alt, &cyl, &head, &sector);
	    printf("      %7ld %4d   %2d     %2d\n",
		badblk.bb_badmap[i].bb_alt, cyl, head, sector);
	} else
	    printf("  unassigned\n");
    }
    printf("\n");

    exit(0);
}

void
blk_to_hst(blk, cyl, head, sector)
    uint32 blk;
    int *cyl, *head, *sector;
{
    *cyl = blk / (geom.g_numhead * geom.g_numsect);
    *sector = (blk % geom.g_numsect) + 1;
    *head = ((blk % (geom.g_numhead * geom.g_numsect)) / geom.g_numsect);
}
