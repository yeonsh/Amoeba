/*	@(#)fdisk.c	1.8	96/02/27 10:03:45 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * fdisk.c
 *
 * Amoeba's fixed disk utility. This utility allows users to manage the
 * partition table and scan the disk for bad blocks.
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

/*
 * Mappings from system index to system names.
 * It would be nice to keep these as accurate
 * as possible.
 */
struct ostypes {
    char *ot_name;		/* operating system name */
    uint8 ot_index;		/* operating system index */
} ostypes[] = {
    "NOSYS",		0x00,	/* no system */
    "DOS-12",		0x01,	/* MS/DOS, 12-bit FAT */
    "XENIX /",		0x02,	/* Xenix root */
    "XENIX /usr",	0x03,	/* Xenix user */
    "DOS-16",		0x04,	/* MS/DOS, 16-bit FAT */
    "DOS-EXT",		0x05,	/* MS/DOS Extended */
    "DOS-BIG",		0x06,	/* MS/DOS Large */
    "HPFS",		0x07,	/* OS/2 High Performance File System */
    "AIX",		0x08,	/* A/IX */
    "AIX-BOOT",		0x09,	/* bootable A/IX partition */
    "OPUS",		0x0A,	/* Opus */
    "OPUS",		0x10,	/* Opus */
    "VENIX",		0x40,	/* Venix 80286 */
    "NOVELL",		0x51,	/* possibly Novell */
    "SYSTEM5",		0x52,	/* Microport's V/AT or CP/M */
    "DOS-DATA",		0x56,	/* DOS data */
    "UNIX",		0x63,	/* 386/IX and AT&T's system V */
    "NOVELL",		0x64,	/* Novell */
    "MACH",		0x65,	/* MACH 2.5, i386 */
    "PC/IX",		0x75,	/* PC/IX */
    "MINIX-OLD",	0x80,	/* pre 1.4b Minix */
    "MINIX",		0x81,	/* Minix partition */
    "AMOEBA",		0x93,	/* Amoeba */
    "BADBLK",		0x94,	/* Amoeba bad block partition */
    "CCP/M",		0xDB,	/* Concurrent CP/M */
    "BADTRK",		0xFF,	/* Bad track table */
};

/*
 * This file contains a hex dump of the master bootstrap loader for the
 * AT-bus based 80386. This 8086 (!) program will be loaded by the ROM
 * boot routines at address 0x7C00 and it will examine its partition
 * table (starting at offset 446) to find an active partition. If it
 * finds one, the first sector of this partition is loaded into memory
 * and a jump to it is made. If something went wrong during this boot
 * phase the program will display an appropriate (and comprehensable)
 * message and halt the system.
 */
unsigned char masterbootblock[512] = {
    0xfa,0x33,0xc0,0x8e,0xd0,0xbc,0x00,0x7c,0x8b,0xf4,
    0x50,0x07,0x50,0x1f,0xfb,0xfc,0xbf,0x00,0x06,0xb9,
    0x00,0x01,0xf2,0xa5,0xea,0x1d,0x06,0x00,0x00,0xbe,
    0xbe,0x07,0xb3,0x04,0x80,0x3c,0x80,0x74,0x0e,0x80,
    0x3c,0x00,0x75,0x1c,0x83,0xc6,0x10,0xfe,0xcb,0x75,
    0xef,0xcd,0x18,0x8b,0x14,0x8b,0x4c,0x02,0x8b,0xee,
    0x83,0xc6,0x10,0xfe,0xcb,0x74,0x1a,0x80,0x3c,0x00,
    0x74,0xf4,0xbe,0x8b,0x06,0xac,0x3c,0x00,0x74,0x0b,
    0x56,0xbb,0x07,0x00,0xb4,0x0e,0xcd,0x10,0x5e,0xeb,
    0xf0,0xeb,0xfe,0xbf,0x05,0x00,0xbb,0x00,0x7c,0xb8,
    0x01,0x02,0x57,0xcd,0x13,0x5f,0x73,0x0c,0x33,0xc0,
    0xcd,0x13,0x4f,0x75,0xed,0xbe,0xa3,0x06,0xeb,0xd3,
    0xbe,0xc2,0x06,0xbf,0xfe,0x7d,0x81,0x3d,0x55,0xaa,
    0x75,0xc7,0x8b,0xf5,0xea,0x00,0x7c,0x00,0x00,0x49,
    0x6e,0x76,0x61,0x6c,0x69,0x64,0x20,0x70,0x61,0x72,
    0x74,0x69,0x74,0x69,0x6f,0x6e,0x20,0x74,0x61,0x62,
    0x6c,0x65,0x00,0x45,0x72,0x72,0x6f,0x72,0x20,0x6c,
    0x6f,0x61,0x64,0x69,0x6e,0x67,0x20,0x6f,0x70,0x65,
    0x72,0x61,0x74,0x69,0x6e,0x67,0x20,0x73,0x79,0x73,
    0x74,0x65,0x6d,0x00,0x4d,0x69,0x73,0x73,0x69,0x6e,
    0x67,0x20,0x6f,0x70,0x65,0x72,0x61,0x74,0x69,0x6e,
    0x67,0x20,0x73,0x79,0x73,0x74,0x65,0x6d,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x55,0xaa	/* signature */
};

/*
 * Name of master boot program, if any
 */
char *master = NULL;

/*
 * Disk geometry
 */
struct geometry geom;

/*
 * Disk label
 */
char pclabel[PCL_BOOTSIZE];
int pclabel_modified = 0;

/*
 * Bad block table
 */
struct badblk badblk;

/*
 * Bad block scan pattern, and track buffer
 */
char trackbuffer[D_REQBUFSZ];

/*
 * Program, disk name and disk file descriptor
 */
char *program;
char *diskname;
capability diskcap;

/*
 * Fdisk commands
 */
void part_create();
void part_change();
void part_delete();
void part_display();
void part_badblk();

struct command {
    void (*procedure)();
    char *title;
} commands[] = {
    part_display,	"Display partition table", 
    part_create,	"Create a partition",
    part_change,	"Change active partition", 
    part_delete,	"Delete a partition", 
    part_badblk,	"Scan and assign bad sectors",
};
#define	NCOMMANDS	(sizeof(commands) / sizeof(struct command))

errstat disk_read();
errstat disk_write();
errstat disk_getgeometry();

int confirm();
void blk_to_hst();
void getnumber();
void show_commands();
void show_badblock_table();
struct pcpartition *active_partition();

extern int optind;
extern char *optarg;

extern int atoi();


main(argc, argv)
    int argc;
    char **argv;
{
    char line[BUFSIZ];
    uint16 nheads = 0;
    uint16 nsectrk = 0;
    uint16 ncyl = 0;
    errstat err;
    int fd, opt;

    /* scan arguments */
    program = argv[0];
    while ((opt = getopt(argc, argv, "m:h:c:s:")) != EOF) {
	switch (opt) {
	case 'h': /* specify # of heads per cylinder */
	    if (!isdigit(*optarg) || (nheads = atoi(optarg)) == 0) {
		fprintf(stderr, "%s: incorrect number of heads/cylinder\n",
		    program);
		exit(1);
	    }
	    break;
	case 'c': /* specify # of cylinders on disk */
	    if (!isdigit(*optarg) || (ncyl = atoi(optarg)) == 0) {
		fprintf(stderr, "%s: incorrect number of cylinders\n", program);
		exit(1);
	    }
	    break;
	case 's': /* specify # of sectors per track */
	    if (!isdigit(*optarg) || (nsectrk = atoi(optarg)) == 0) {
		fprintf(stderr, "%s: incorrect number of sectors/track\n",
		    program);
		exit(1);
	    }
	    break;
	case 'm': /* specify master boot block */
	    master = optarg;
	    break;
	default:
	    usage();
	    /* NOTREACHED */
	}
    }
    if (optind >= argc) usage();

    diskname = argv[optind];
    if ((err = name_lookup(diskname, &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: can't get capability for %s: %s\n",
	    argv[0], argv[optind], err_why(err));
	exit(1);
    }
    if ((err=disk_read(&diskcap, PCL_LG2BOOTSIZE, 0L, 1L, pclabel)) != STD_OK) {
	fprintf(stderr, "%s: disk_read error: %s\n", program, err_why(err));
	exit(1);
    }

    if ((err = disk_getgeometry(&diskcap, &geom)) != STD_OK) {
	fprintf(stderr, "%s: disk_geometry error: %s\n", program, err_why(err));
	exit(1);
    }

    /*
     * Allow the user to change the geometry information provided
     * by the disk driver. In some cases this information might be
     * wrong (especially with RLL controllers) and the user must
     * override the default.
     */
    if (nheads) geom.g_numhead = nheads;
    if (nsectrk) geom.g_numsect = nsectrk;
    if (ncyl) geom.g_numcyl = ncyl;

    /*
     * Sanity checks
     */
    if (geom.g_numhead <= 0 || geom.g_numsect <= 0 || geom.g_numcyl <= 0) {
	fprintf(stderr, "%s: invalid disk geometry information\n", program);
	exit(1);
    }

    /*
     * When the master boot block of the hard disk is invalid,
     * we use our own version.
     */
    if (!PCL_MAGIC(pclabel)) {
	printf("WARNING: invalid master boot block; using alternate.\n");
	if (master != NULL) {
	    if ((fd = open(master, 0)) < 0) {
		fprintf(stderr, "%s: cannot open %s\n", program, master);
		exit(1);
	    }
	    read(fd, pclabel, sizeof(pclabel));
	    pclabel[BOOT_MAGIC_OFFSET_0] = BOOT_MAGIC_BYTE_0;
	    pclabel[BOOT_MAGIC_OFFSET_0] = BOOT_MAGIC_BYTE_1;
	    close(fd);
	} else {
	    memmove(pclabel, masterbootblock, sizeof(pclabel));
	}

	pclabel_modified = 1;
    }

    /*
     * Main command loop. It may be over cautious,
     * but I think it is wise to warn the user for
     * possible disasters.
     */
    printf("Amoeba Fixed Disk Utility Program\n\n");
    printf("BEWARE: IMPROPER USE CAN DESTROY DATA ON YOUR HARD DISK !\n\n");
    show_commands(commands, NCOMMANDS);
    printf("\nPress 'q' to return to Amoeba.\n");

    for (;;) {
	printf("\nChoice ? "); fflush(stdout);
	if (gets(line) != NULL && line[0]) {
	    if (tolower(line[0]) == 'q')
		break;
	    if (isdigit(line[0])) {
		int n = atoi(line) - 1;
		if (n < 0 || n >= NCOMMANDS) {
		    printf("Illegal choice: '%s'\n", line);
		    continue;
		}
		(*commands[n].procedure)();
		continue;
	    } else
		printf("Illegal choice: '%s'\n", line);
	}
	show_commands(commands, NCOMMANDS);
	printf("\nPress 'q' to return to Amoeba.\n");
    }

    if (active_partition() == NULL)
	printf("WARNING: no active partition; system is unbootable.\n");

    if (pclabel_modified &&
	confirm("\nDo you want to save the changes to the partition table"))
    {
	if ((err=disk_write(&diskcap,PCL_LG2BOOTSIZE,0L,1L,pclabel)) != STD_OK){
	    fprintf(stderr, "%s: disk_write error: %s\n",
		program, err_why(err));
	    exit(1);
	}
	printf("Changes saved.\n");
    }
    exit(0);
    /* NOTREACHED */
}

usage()
{
    fprintf(stderr,
	"Usage: %s [-h n] [-s n] [-c n] [-m master] disk-capability\n",
	program);
    exit(1);
    /* NOTREACHED */
}

void
show_commands(cmds, ncmds)
    struct command *cmds;
    int ncmds;
{
    register int i;

    printf("\nOptions:\n");
    for (i = 0; i < ncmds; i++)
	printf("\t%d. %s\n", i + 1, cmds[i].title);
}

/*
 * Create a new partition entry.
 * There must be room for another partition table entry,
 * and the new partition should be consistent with the
 * other partition entries.
 */
void
part_create()
{
    register struct pcpartition *pe;
    int n, firstcyl, lastcyl;
    int	sects_per_cyl;

    /* search partition table for free slot */
    for (n = 0, pe = PCL_PCP(pclabel); n < PCL_NPART; n++, pe++)
	if (pe->pp_sysind == PCP_NOSYS)
	    break;
    if (n == PCL_NPART) {
	fprintf(stderr, "\nNo free partition table entry.\n");
	fprintf(stderr, "Use the delete option to create one.\n");
	return;
    }

    pclabel_modified = 1;
    sects_per_cyl = geom.g_numsect * geom.g_numhead;
    do {
	/*
	 * The only reason for using cylinders as granuality is that
	 * MS/DOS's fdisk would choke on it if we didn't.
	 */
	part_display();
	getnumber("\nFirst cylinder", &firstcyl);
	getnumber("Last cylinder ", &lastcyl);
	pe->pp_sysind = PCP_AMOEBA;
	pe->pp_bootind = PCP_PASSIVE;

	/*
	 * Compute the physical sector/head/block/cylinder.
	 * Note that track 0 is reserved for bootstrap purposes.
	 */
	pe->pp_first = (firstcyl & 0xFFFF) * sects_per_cyl;
	if (pe->pp_first == 0) /* Skip track 0 */
	    pe->pp_first = geom.g_numsect;
	pe->pp_size = ((lastcyl + 1) & 0xFFFF) * sects_per_cyl - pe->pp_first;
	blk_to_hst(pe->pp_first,
		&pe->pp_start_head, &pe->pp_start_sec, &pe->pp_start_cyl);
	blk_to_hst(pe->pp_first + pe->pp_size - 1,
		&pe->pp_last_head, &pe->pp_last_sec, &pe->pp_last_cyl);
    } while (!partition_consistent());

    printf("Partition %d created (start %ld, size %ld blocks).\n",
	n, pe->pp_first, pe->pp_size);
}

/*
 * Determine whether the new partition table is consistent.
 * This routine only states whether the table is consistent
 * or not, and does not give a clue about which entry is
 * inconsistent.
 */
int
partition_consistent()
{
    register struct pcpartition *pe, *pp;
    register int i, n;

    /* walk through the all the partition table entries */
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	if (pe->pp_size != 0) {
	    /* make sure partition fits on the disk */
	    uint32 size = geom.g_numhead * geom.g_numsect * geom.g_numcyl;
	    if (pe->pp_first > size) {
		printf("First block of partition is not on the disk\n");
		return 0;
	    } else if (pe->pp_first + pe->pp_size > size) {
		printf("Partition has too many sectors to fit on the disk.\n");
		return 0;
	    } else if (pe->pp_first + pe->pp_size < pe->pp_size) {
		printf("Overflow from preposterous size.\n");
		return 0;
	    }
	    /* make sure it doesn't overlap */
	    for (pp = PCL_PCP(pclabel), n = 0; n < PCL_NPART; pp++, n++) {
		if (pp != pe && pp->pp_size != 0 &&
		  (pe->pp_first >= pp->pp_first &&
		  pe->pp_first < pp->pp_first + pp->pp_size ||
		  pe->pp_first + pe->pp_size - 1 >= pp->pp_first &&
		  pe->pp_first + pe->pp_size - 1 < pp->pp_first + pp->pp_size)){
		    printf("Partition overlaps with partition %d.\n", n);
		    return 0;
		}
	    }
	}
    }
    return 1;
}

/*
 * Convert absolute block to head/sector/cylinder
 */
void
blk_to_hst(blk, hd, sec, cyl)
    uint32 blk;			/* absolute disk block */
    uint8 *hd;			/* head */
    uint8 *sec;			/* sector relative to track */
    uint8 *cyl;			/* cylinder */
{
    int bigcyl = blk / (geom.g_numhead * geom.g_numsect);
    *sec = (blk % geom.g_numsect) + 1 + ((bigcyl >> 2) & 0xC0);
    *cyl = bigcyl & 0xFF;
    *hd = ((blk % (geom.g_numhead * geom.g_numsect)) / geom.g_numsect) +
	((bigcyl >> 4) & 0xC0);
}

/*
 * Change active partition
 */
void
part_change()
{
    register struct pcpartition *pe;
    int n;

    for (;;) {
	part_display();
	getnumber("Enter partition number that you want to activate", &n);
	if (n >= 0 && n <= PCL_NPART) break;
	printf("Please specify the partition by its rank number.\n");
    }
    if (PCL_PCP(pclabel)[n].pp_sysind == PCP_NOSYS) {
	printf("Partition slot %d is empty; nothing has been changed.\n", n);
	return;
    }
    if ((pe = active_partition()) != NULL)
	pe->pp_bootind = PCP_PASSIVE;
    PCL_PCP(pclabel)[n].pp_bootind = PCP_ACTIVE;
    printf("Partition %d made active.\n", n);
    pclabel_modified = 1;
}

/*
 * Delete a partition table entry
 */
void
part_delete()
{
    register struct pcpartition *pe;
    int n;

    part_display();
    printf("\nWARNING: Deleting a partition will DESTROY all data in it.\n");
    if (!confirm("Do you really want to continue"))
	return;

    for (;;) {
	getnumber("Enter partition number that you want to delete", &n);
	if (n >= 0 && n <= PCL_NPART) break;
	printf("Please specify the partition by its rank number.\n");
	part_display();
    }

    if ((pe = &PCL_PCP(pclabel)[n])->pp_sysind != PCP_NOSYS) {
	pe->pp_sysind = PCP_NOSYS;
	pe->pp_bootind = PCP_PASSIVE;
	pe->pp_first = pe->pp_size = 0;
	pe->pp_start_head = pe->pp_start_sec = pe->pp_start_cyl = 0;
	pe->pp_last_head = pe->pp_last_sec = pe->pp_last_cyl = 0;
	if (active_partition() == NULL)
	    printf("WARNING: deleted active partition.\n");
	printf("Partition %d deleted.\n", n);
	pclabel_modified = 1;
    } else
	printf("Partition slot %d is empty; nothing is deleted.\n", n);
}

/*
 * Display the partition table
 */
void
part_display()
{
    register struct pcpartition *pe;
    register int i;

    printf("(%s: %ld cylinders, %d heads, %d sectors/head)\n",
	diskname, geom.g_numcyl, geom.g_numhead, geom.g_numsect);
    printf("%s\n%s\n",
"                      ---first----   ----last----    -------sectors------",
"Part Active Type      Cyl Head Sec   Cyl Head Sec    Base    Last    Size");

    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	char *system_type();

	printf(" %d    %4s  %-8s %4d  %3d %3d  %4d  %3d %3d %7ld %7ld %7ld\n",
	    i,
	    pe->pp_bootind == PCP_ACTIVE ? "Yes" : "No ",
	    system_type((int)pe->pp_sysind),
	    pe->pp_start_cyl + ((pe->pp_start_sec & 0xC0) << 2) +
		((pe->pp_start_head & 0xC0) << 4),
	    pe->pp_start_head & 0x3F, pe->pp_start_sec & 0x3F,
	    pe->pp_last_cyl + ((pe->pp_last_sec & 0xC0) << 2) +
		((pe->pp_last_head & 0xC0) << 4),
	    pe->pp_last_head & 0x3F, pe->pp_last_sec & 0x3F,
	    pe->pp_first, pe->pp_first + pe->pp_size +
	    (pe->pp_size > 0 ? -1 : 0), pe->pp_size);
    }
}

char *
system_type(index)
    int index;
{
    register int i;
    static char b[10];

    for (i = 0; i < (sizeof(ostypes) / sizeof(struct ostypes)); i++)
	if (ostypes[i].ot_index == (uint8)index)
	    return ostypes[i].ot_name;
    (void) sprintf(b, "-%d-", index & 0xFF);
    return b;
}

/*
 * Map bad blocks on the disk to alternate (good) blocks.
 */
void
part_badblk()
{
    struct pcpartition *freeslot;	/* first free slot in pclabel */
    struct pcpartition *pe;		/* current partition entry */
    disk_addr lastblk;			/* last block on disk */
    disk_addr altblk;			/* alternate for bad block */
    errstat err;
    int manual = 0;
    int i, j;

    /*
     * Look for an existing bad block partition or an empty slot
     */
    freeslot = NULL;
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	if (pe->pp_sysind == PCP_BADBLK)
	    break;
	if (!freeslot && pe->pp_sysind == PCP_NOSYS)
	    freeslot = pe;
    }
    if (i == PCL_NPART) {
	if (freeslot == NULL) {
	    printf("No room in partition table for bad block partition.\n");
	    return;
	}
	pe = freeslot;
    }

    /* read bad block table from bad block partition */
    if (pe->pp_sysind == PCP_BADBLK) {
	if ((err = disk_read(&diskcap, D_PHYS_SHIFT,
	  (long)pe->pp_first, (long)BB_BBT_SIZE, (char *) &badblk)) != STD_OK) {
	    fprintf(stderr, "Cannot read bad block table: %s\n", err_why(err));
	    return; /* isn't this a major failure ? */
	}
    }

    /* be very cautious */
    printf("\nWARNING: Scanning the disk for bad sectors will DESTROY");
    printf(" all data on it.\n");
    if (!confirm("Do you really want to continue"))
	return;

    /*
     * If there is already a valid bad block table, ask the user to
     * retain this table.
     */
    if (badblk.bb_magic != BB_MAGIC) {
        badblk.bb_magic = BB_MAGIC;
	badblk.bb_count = 0;
	for (i = 0; i < BB_NBADMAP; i++)
	    badblk.bb_badmap[i].bb_bad = badblk.bb_badmap[i].bb_alt = 0;
    } else {
	/* clear bad block table if not retaining old values */
	if (!confirm("Retain old bad block information")) {
	    badblk.bb_count = 0;
	    for (i = 0; i < BB_NBADMAP; i++)
	        badblk.bb_badmap[i].bb_bad = badblk.bb_badmap[i].bb_alt = 0;
	}
    }

    /*
     * The disk can be scanned for bad blocks automatically,
     * or they can be entered manually.
     */
    if (confirm("Scan disk for bad blocks")) {
	register int cyl, head, sect;

	/*
	 * To speed up the automatic scanning of the disk we a track
	 * at once. When this fails, the individual sectors are tried
	 * to determine which is bad (there may be more though).
	 */
	printf("Disk has %d cylinders, %d head, and %d sectors/track\n",
	    geom.g_numcyl - PCL_PARKCYLS, geom.g_numhead, geom.g_numsect);
	for (cyl = 0; cyl < geom.g_numcyl - PCL_PARKCYLS; cyl++) {
	    for (head = 0; head < geom.g_numhead; head++) {
		printf("Scanning disk ... cyl %d, head %d     \r", cyl, head);
		if (bad_track(cyl, head)) {
		    for (sect = 0; sect < geom.g_numsect; sect++) {
			if (bad_sector(cyl, head, sect)) {
			    disk_addr blk = sect + head * geom.g_numsect +
				cyl * (geom.g_numhead * geom.g_numsect);
			    printf("Bad block (%d) at cyl %d, head %d, sector %d     \n",
				blk, cyl, head, sect+1);
			    if (cyl == 0 && head == 0 && sect == 0) {
				fprintf(stderr,
				    "%s: Sector 0 is bad, disk is unusable.\n",
				    program);
				exit(1);
			    }
			    if (badblk.bb_count < BB_NBADMAP)
				badblk.bb_badmap[badblk.bb_count++].bb_bad= blk;
			}
		    }
		}
		fflush(stdout);
	    }
	}
	printf("Scanning disk ... done.                         \n");
	if (badblk.bb_count == BB_NBADMAP)
	   printf("Warning: too many bad blocks; not all could be remapped.\n");
    } else if (confirm("Enter bad blocks manually")) {
	manual = 1;
	do {
	    printf("Enter the bad blocks. ");
	    printf("The list is terminated by entering block number 0.\n");
	    for (i = badblk.bb_count; i < BB_NBADMAP; i++) {
		getnumber("Bad block", (int *)&badblk.bb_badmap[i].bb_bad);
		if (badblk.bb_badmap[i].bb_bad == 0) break;
	    }
	    if ((badblk.bb_count = i) == BB_NBADMAP)
		printf("Sorry, bad block table is full.\n");
	    show_badblock_table(&badblk);
	} while (!confirm("Is the bad block table ok"));
    }

    /* It is pointless to resume if there aren't any bad blocks.
     * However, we do allow an empty bad block partition in case the
     * bad blocks were entered manually.  In that case the bad block
     * table might be zeroed in preparation of fresh bad block scan.
     */
    if (badblk.bb_count == 0 && !manual) {
	printf("No bad blocks found; bad block partition not created.\n");
	return;
    }

    /*
     * Remap bad blocks onto alternate blocks at the end of the
     * disk. During this remap we do not allocate alternate blocks
     * in the last PCL_PARKCYLS cylinders. These are used by the
     * diagnostic software as a parking place for the disk heads.
     * Also, subtract 1 to get the block number of the last block.
     */
    altblk = lastblk = ((geom.g_numcyl - PCL_PARKCYLS) *
			geom.g_numhead * geom.g_numsect) - 1;
    for (i = 0; i < badblk.bb_count; i++) {
nextalt:
	if (altblk <= 0) {
	    /* highly unlikely situation */
	    printf("Too many bad blocks; cannot replace all of them.\n");
	    break;
	}
	/* check for bad alternate blocks */
	for (j = 0; j < badblk.bb_count; j++) {
	    if (badblk.bb_badmap[j].bb_bad == altblk) {
		altblk--;
		goto nextalt;
	    }
	}
	badblk.bb_badmap[i].bb_alt = altblk--;
    }

    /* be sure this is what the user really wants */
    show_badblock_table(&badblk);
    if (!confirm("Do you want to write the bad block table")) {
	printf("Bad block table wasn't written.\n");
	badblk.bb_count = 0;
	return;
    }

    /* setup a new bad block partition */
    altblk -= BB_BBT_SIZE;
    /* round up to nearest cylinder */
    altblk -= altblk % (geom.g_numhead * geom.g_numsect);
    pe->pp_first = altblk;
    pe->pp_sysind = PCP_BADBLK;
    pe->pp_bootind = PCP_PASSIVE;
    pe->pp_size = lastblk - altblk + 1;
    blk_to_hst(pe->pp_first, &(pe->pp_start_head),
	&(pe->pp_start_sec), &(pe->pp_start_cyl));
    blk_to_hst(pe->pp_first + pe->pp_size - 1, &(pe->pp_last_head),
	&(pe->pp_last_sec), &(pe->pp_last_cyl));
    pclabel_modified = 1;

    /* conflicts should be resolved immediately */
    if (!partition_consistent()) {
	printf("Bad block partition overlaps with other partition.\n");
	printf("Remove conflicting partition and rescan for bad blocks.\n");
	pe->pp_sysind = PCP_NOSYS;
	pe->pp_bootind = PCP_PASSIVE;
	pe->pp_first = pe->pp_size = 0;
	pe->pp_start_head = pe->pp_start_sec = pe->pp_start_cyl = 0;
	pe->pp_last_head = pe->pp_last_sec = pe->pp_last_cyl = 0;
	return;
    }

    /* write the bad block table to disk */
    if ((err = disk_write(&diskcap, D_PHYS_SHIFT,
      (long)pe->pp_first, (long)BB_BBT_SIZE, (char *) &badblk)) != STD_OK) {
	fprintf(stderr, "Cannot write bad block table: %s\n", err_why(err));
	badblk.bb_count = 0;
	return; /* isn't this a major failure ? */
    }
    printf("Bad block table has been written.\n");
}

/*
 * Non destructive check for bad track
 */
int
bad_track(cyl, head)
    int cyl, head;
{
    register int n, bytes, nblks;
    disk_addr firstblk;

    firstblk = cyl * (geom.g_numhead * geom.g_numsect) + head * geom.g_numsect;
    for (n = geom.g_numsect * D_PHYSBLKSZ; n > 0; n -= D_REQBUFSZ) {
	bytes = n > D_REQBUFSZ ? D_REQBUFSZ : n;
	nblks = bytes / D_PHYSBLKSZ;
	if (disk_read(&diskcap,D_PHYS_SHIFT,firstblk,nblks,trackbuffer)!=STD_OK)
	    return 1;
	firstblk += nblks;
    }
    return 0;
}

/*
 * Non destructive check for bad sector
 */
int
bad_sector(cyl, head, sect)
    int cyl, head, sect;
{
    register disk_addr blk;

    blk = cyl * (geom.g_numhead * geom.g_numsect) +
	head * geom.g_numsect + sect;
    return disk_read(&diskcap, D_PHYS_SHIFT, blk, 1L, trackbuffer) != STD_OK;
}

/*
 * Prints a bad block table
 */
void
show_badblock_table(bbt)
    register struct badblk *bbt;
{
    register int i;

    printf("Bad Block Table Summary\n");
    printf("%d bad blocks.\n", bbt->bb_count);
    for (i = 0; i < 3; i++)
	printf("   Block  Alternate  ");
    printf("\n");
    for (i = 0; i < bbt->bb_count; i++) {
	printf(" %7ld", bbt->bb_badmap[i].bb_bad);
	if (bbt->bb_badmap[i].bb_alt == 0)
	    printf("  unassigned ");
	else
	    printf("    %7ld  ", bbt->bb_badmap[i].bb_alt);
	if ((i % 3) == 2) printf("\n");
    }
    printf("\n");
}

struct pcpartition *
active_partition()
{
    register struct pcpartition *pe;
    register int i;

    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++)
	if (pe->pp_sysind != PCP_NOSYS && pe->pp_bootind == PCP_ACTIVE)
	    return pe;
    return (struct pcpartition *) NULL;
}

int
confirm(s)
    char *s;
{
    char line[BUFSIZ];

    printf("%s ? ", s); fflush(stdout);
    return gets(line) != NULL && (line[0] == 'y' || line[0] == 'Y');
}

void
getnumber(s, n)
    char *s;
    int *n;
{
    char line[BUFSIZ];

    for (;;) {
	printf("%s: ", s, *n); fflush(stdout);
	if (gets(line) != NULL && isdigit(line[0])) {
	    *n = atoi(line);
	    return;
	}
	printf("Please specify a number.\n");
    }
}
