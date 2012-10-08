/*	@(#)di.c	1.6	96/02/27 10:23:53 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** di.c
**	Show the disk partition information for a physical disk.
**	Derived from the original di written by Peter Bosch but radically
**	different.
** Author: Greg Sharp, 13/03/91
**
** Properly initialized automatic array ptab, Leendert van Doorn, 7/5/91
** Added proper endian conversions, Kees Verstoep, 26/03/92
*/

#include <amoeba.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <byteorder.h>
#include <module/name.h>
#include <module/prv.h>
#include <module/disk.h>
#include <vdisk/disklabel.h>
#include <machdep/dev/sunlabel.h>
#include <machdep/dev/ultpart.h>
#include <machdep/dev/bblock.h>
#include <machdep/dev/pclabel.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__STDC__) && !defined(AMOEBA)
#define memcpy(to, from, size)	bcopy(from, to, size)
#endif

extern uint16		checksum();

#define MIN(x, y)	(((x) <= (y))? (x): (y))

static char *		Prog_name;
static char *		Disk_name;
static capability	Disk_cap;

/* Forward declarations */
void	show_disk_info();


main(argc, argv)
int	argc;
char *	argv[];
{
    errstat	rv;

    Prog_name = argv[0];
    if (argc != 2 || strcmp(argv[1], "-?") == 0)
    {
	fprintf(stderr, "Usage: %s disk-capability\n", Prog_name);
	fprintf(stderr, "where the disk capability is for a physical disk\n");
	exit(1);
    }
    
    Disk_name = argv[1];
    if ((rv = name_lookup(Disk_name, &Disk_cap)) != STD_OK)
    {
	fprintf(stderr, "%s: cannot lookup %s: %s.\n",
					    Prog_name, Disk_name, err_why(rv));
	exit(1);
    }

    show_disk_info();
    exit(0);
}

/* Possible return values of amoeba_label(): */
#define ENDIAN_NONE	0
#define ENDIAN_BIG	1
#define ENDIAN_LITTLE	2

/*
** Print the partition information in an Amoeba label
*/

void
print_am_label(label, endian, host_offset)
disklabel *	label;
int		endian;
disk_addr	host_offset;
{
    int i;

    printf("\t\tvdisk  start      end        size\n");
    for (i = 0; i < MAX_PARTNS_PDISK; i++) {
	int32 firstblk_i, numblks_i;

	firstblk_i = label->d_ptab[i].p_firstblk;
	numblks_i  = label->d_ptab[i].p_numblks;
	if (endian == ENDIAN_BIG) {
	    DEC_INT32_BE(&firstblk_i);
	    DEC_INT32_BE(&numblks_i);
	} else {
	    DEC_INT32_LE(&firstblk_i);
	    DEC_INT32_LE(&numblks_i);
	}

	if (numblks_i != 0)
	{
	    printf("\t\t %03lx   0x%08lx 0x%08lx (0x%08lx)\n",
		   (long) prv_number(&label->d_ptab[i].p_cap.cap_priv),
		   (long) (firstblk_i + host_offset), 
		   (long) (firstblk_i + numblks_i + host_offset - 1),
		   (long) numblks_i);
	}
    }
}

int
amoeba_label(buf)
char	* buf;
{
    disklabel *	dlp = (disklabel *) buf;
    int		endian = ENDIAN_NONE;
    uint32      magic;

    /* We have to try both endians in this case. */
    magic = dlp->d_magic;
    DEC_INT32_BE(&magic);

    if (magic == AM_DISK_MAGIC) {	/* Yup. The label is big endian */
	endian = ENDIAN_BIG;
    } else {
	magic = dlp->d_magic;
	DEC_INT32_LE(&magic);
	if (magic == AM_DISK_MAGIC) {	/* The label is little endian */
	    endian = ENDIAN_LITTLE;
	}
    }

    /* The checksum() routine can be used for either endian. */
    if (endian != ENDIAN_NONE && checksum((uint16 *) dlp) != 0) {
	endian = ENDIAN_NONE;
    }

    return endian;
}


/*
**	show_disk_info
**
** Try to deduce what sort of disk label is on the disk and then analyze the
** disk structure accordingly.  In the case of an Amoeba label on the start of
** the disk, all other assumptions about the disk are impossible.  For example,
** it does not have a boot block or anything else special on the disk.
*/

void
show_disk_info()
{
    int		endian;
    errstat	rv;
    char	buf[D_PHYSBLKSZ];

    /* Read block 0 of the physical disk */
    rv = disk_read(&Disk_cap, D_PHYS_SHIFT, (disk_addr) 0, (disk_addr) 1, buf);
    if (ERR_STATUS(rv))
    {
	fprintf(stderr, "%s: can't read block 0 of %s: %s\n",
					Prog_name, Disk_name, err_why(rv));
	exit(1);
    }

    /*
    ** We try to see what sort of disk label it is.  We start by seeing if it
    ** is an Amoeba label, as is the case with ramdisks.  In this case there
    ** will not be any sub-partitioning.
    */

    if ((endian = amoeba_label(buf)) != ENDIAN_NONE)
    {
	printf("This disk has an Amoeba label\n");
	print_am_label((disklabel *) buf, endian, (disk_addr) 0);
	return;
    }

    /*
    ** It wasn't an Amoeba label so we try host OS specific labels.  First
    ** we copy the partition table info to a generic partition table and
    ** then we work through each partition, seeing if it is for Amoeba
    */

    if (dos_label(buf))
	return;
    
    if (sunos_label(buf))
	return;
    
    /* This is known to work for a vax but isn't tested on a pmax */
    if (ultrix_label(buf))
	return;
    
    printf("Unkown Disk label type.\n");
}


/*
** Print the host label disk partition info.
*/

void
print_host_partitions(ptab, host_offset)
part_tab	ptab[];
disk_addr	host_offset;
{
    int		endian;
    int		n;
    errstat	rv;
    char	buf[D_PHYSBLKSZ];

    printf("Partition start      end        size         owner\n");
    for (n = 0; n < MAX_PARTNS_PDISK; n++)
    {
	ptab[n].p_firstblk += host_offset;
	printf("%02x (%c)    0x%08lx 0x%08lx (0x%08lx) ", 
	       n, 'a' + n,
	       (long) ptab[n].p_firstblk, 
	       (long) (ptab[n].p_firstblk + ptab[n].p_numblks - 1),
	       (long) ptab[n].p_numblks);
	
	if (ptab[n].p_numblks == 0)
	{
	    printf("Empty\n");
	    continue;
	}

	/* Read vdisk info. */
	rv = disk_read(&Disk_cap, D_PHYS_SHIFT, 
			ptab[n].p_firstblk, (disk_addr) 1, buf);
	if (ERR_STATUS(rv))
	{
	    fprintf(stderr, "%s: Cannot read block %ld from %s: %s\n",
		    Prog_name, (long) ptab[n].p_firstblk,
		    Disk_name, err_why(rv));
	    exit(1);
	}

	if ((endian = amoeba_label(buf)) != ENDIAN_NONE)
	{
	    printf("Amoeba\n\t\tAmoeba partitioning for partition %d:\n", n);
	    print_am_label((disklabel *) buf, endian, host_offset);
	}
	else
	    printf("Unknown\n");
    }
}



/*
** Returns 1 if it is a dos disk label.  Otherwise returns 0.
** As a side-effect it also prints the label.
*/

int
dos_label(buf)
char *	buf;
{
    struct pcpartition  *pcp;
    part_tab		ptab[MAX_PARTNS_PDISK];
    int			i;

    /* The PCL_MAGIC test is endian independent: */
    if (PCL_MAGIC(buf))
    {
	printf("This disk has a DOS label\n");

	for (i = 0; i < MAX_PARTNS_PDISK; i++)
	    ptab[i].p_firstblk = ptab[i].p_numblks = 0;

	pcp = PCL_PCP(buf);
	for (i = 0; i < MIN(PCL_NPART, MAX_PARTNS_PDISK); i++, pcp++)
	{
	    uint32 first, size;

	    /* The first block and the size are in little endian format.
	     * We must memcpy the fields because they are not aligned properly.
	     */
	    memcpy((_VOIDSTAR)&first, (_VOIDSTAR)&pcp->pp_first,sizeof(first));
	    DEC_INT32_LE(&first);
	    memcpy((_VOIDSTAR) &size, (_VOIDSTAR)&pcp->pp_size, sizeof(size));
	    DEC_INT32_LE(&size);

	    if (size > AMOEBA_BOOT_SIZE)
	    {
		ptab[i].p_firstblk = first;
		ptab[i].p_numblks = size;
		if (pcp->pp_sysind == PCP_AMOEBA)
		{
		    ptab[i].p_firstblk += AMOEBA_BOOT_SIZE;
		    ptab[i].p_numblks -= AMOEBA_BOOT_SIZE;
		}
	    }
	    else
	    {
		ptab[i].p_firstblk = 0;
		ptab[i].p_numblks = 0;
	    }
	}

	print_host_partitions(ptab, (disk_addr) 0);
	return 1;
    }
    return 0;
}


/*
** Returns 1 if it is a sunos disk label.  Otherwise returns 0.
** As a side-effect it also prints the label.
*/

int
sunos_label(buf)
char *	buf;
{
    sun_label *	slp = (sun_label *) buf;
    part_tab	ptab[MAX_PARTNS_PDISK];
    uint16	magic;
    int		i;

    /* The sun label is in big endian format: */
    magic = slp->sun_magic;
    DEC_INT16_BE(&magic);
 
    if (magic == SUN_MAGIC && checksum((uint16 *) slp) == 0)
    {
	uint16 numhead, numsect;

	printf("This disk has a SunOS label\n");

	for (i = 0; i < MAX_PARTNS_PDISK; i++)
	    ptab[i].p_firstblk = ptab[i].p_numblks = 0;

	numhead = slp->sun_numhead;
	DEC_INT16_BE(&numhead);
	numsect = slp->sun_numsect;
	DEC_INT16_BE(&numsect);

	for (i = 0; i < MIN(PT_NPART, MAX_PARTNS_PDISK); i++)
	{
	    int32 cylno_i, nblk_i;

	    cylno_i = slp->sun_map[i].sun_cylno;
	    DEC_INT32_BE(&cylno_i);
	    nblk_i = slp->sun_map[i].sun_nblk;	    
	    DEC_INT32_BE(&nblk_i);

	    ptab[i].p_firstblk = cylno_i * numhead * numsect;
	    ptab[i].p_numblks  = nblk_i;
	}

	print_host_partitions(ptab, (disk_addr) (numhead * numsect));
	return 1;
    }
    return 0;
}


/*
** Returns 1 if it looks like there is an ultrix disk label or a vax bootblock.
** Otherwise it returns 0.  If there is a valid ultrix label it prints the
** partition info.
*/

int
ultrix_label(buf)
char *	buf;
{
    b1_block *	bb1;
    b2_block	bb2;
    int		i;
    errstat	rv;
    part_tab	ptab[MAX_PARTNS_PDISK];
    struct pt *	pt;
    int32	magic;

    /* See if it is an ultrix disk on a vax -may also work on a decstation.
     * Note: bb1->b1_n is an unsigned character, so it doesn't need to
     * be converted to this program's endianness.
     */
    bb1 = (b1_block *) buf;
    if ((bb1->b1_n < (sizeof(b1_block)/2)) || 
	(bb1->b1_n > (512 - sizeof(b2_block) + 8)/2))
    {
	return 0;
    }
    else /* look at the boot block !? */
    {
	uint32 start, loadaddr, nblock, sum;

	memcpy((_VOIDSTAR) &bb2, (_VOIDSTAR) (buf + bb1->b1_n * 2),
	       sizeof(b2_block));

	/* convert the fields we use from little endian */
	start = bb2.b2_start;
	DEC_INT32_LE(&start);
	loadaddr = bb2.b2_loadaddr;
	DEC_INT32_LE(&loadaddr);
	nblock = bb2.b2_nblock;
	DEC_INT32_LE(&nblock);
	sum = bb2.b2_sum;
	DEC_INT32_LE(&sum);

	if (start + loadaddr + nblock != sum)
	{
	    printf("Invalid bootblock.\n");
	    return 0;
	}
	else
	{
	    uint16 lbn, hbn;
	    uint16 instr;

	    lbn = bb1->b1_lbn;
	    DEC_INT16_LE(&lbn);
	    hbn = bb1->b1_hbn;
	    DEC_INT16_LE(&hbn);

	    instr = bb2.b2_instr;
	    DEC_INT16_LE(&instr);

	    printf("This disk has a VAX-Ultrix label\n");
	    printf("Bootblock info:\n");
	    printf("Boot image disk address:\tx%lx\n",
		   (long) ((hbn << 16) + lbn));
	    printf("Instruction set:\t\t%s\n",
		   (instr == 0x18) ? "VAX": "Not VAX");
	    printf("Boot image size (blocks):\tx%lx (%ld)\n",
		   (long) nblock, (long) nblock);
	    printf("Boot image load address:\tx%lx\n", (long) loadaddr);
	    printf("Boot image entry address:\tx%lx\n", (long) start);
	}
    }

    /* Try to get an Ultrix partition table */
    rv = disk_read(&Disk_cap, D_PHYS_SHIFT,
		   (disk_addr) ((SB_OFFSET + PT_OFFSET) >> D_PHYS_SHIFT),
		   (disk_addr) 1, buf);
    if (ERR_STATUS(rv))
    {
	fprintf(stderr, "%s: Cannot read block %ld from %s: %s\n",
		Prog_name, (long) ((SB_OFFSET + PT_OFFSET) >> D_PHYS_SHIFT),
		Disk_name, err_why(rv));
	return 1;
    }

    pt = (struct pt *) &buf[D_PHYSBLKSZ - sizeof(struct pt)];

    magic = pt->pt_magic;
    DEC_INT32_LE(&magic);
    if (magic != PT_MAGIC)
    {
	fprintf(stderr, "%s: No partitioning on disk.\n", Prog_name);
	return 1;
    }

    for (i = 0; i < MAX_PARTNS_PDISK; i++)
	ptab[i].p_firstblk = ptab[i].p_numblks = 0;

    for (i = 0; i < MIN(PT_NPART, MAX_PARTNS_PDISK); i++)
    {
	int32 start_i, size_i;

	start_i = pt->pt_tab[i].pt_start;
	DEC_INT32_LE(&start_i);
	size_i = pt->pt_tab[i].pt_size;
	DEC_INT32_LE(&size_i);

	ptab[i].p_firstblk = start_i;
	ptab[i].p_numblks  = size_i;
    }

    print_host_partitions(ptab,
			  (disk_addr) ((SB_OFFSET + SB_SIZE) >> D_PHYS_SHIFT));
    return 1;
}
