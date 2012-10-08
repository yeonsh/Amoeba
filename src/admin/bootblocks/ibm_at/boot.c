/*	@(#)boot.c	1.13	96/02/27 10:03:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * boot.c
 *
 * Amoeba boot program for an ISA architecture based i80386 machine.
 * NOTE: this code should be compiled by a 8086 (!) C compiler.
 *
 * Author:
 *	Leendert van Doorn
 */
#include "boot.h"
#include <out.h>
#include <amoeba.h>
#include <disk/disk.h>
#include <vdisk/disklabel.h>
#include <pclabel.h>
#include <bootinfo.h>
#include <kbootdir.h>
#include <version.h>

#define	TIMEOUT		30	/* wait this much seconds */
#define LINESIZE	128	/* limit of input line */
#define	BLOCKSIZE	512L	/* physical block size */

#define	MIN(a,b)	((a) > (b) ? (b) : (a))
#define	BCD2DEC(n)	((((n) & 0xF0) >> 4) * 10 + ((n) & 0x0F))

/*
 * The volatile (its block gets overwritten) information
 * in the section structures is stored in the structure
 * below. These values are used for computing segment
 * size, padding, etc.
 */
struct kernelinfo {
    unsigned long ki_textsize;	/* text and rom size */
    unsigned long ki_textbase;	/* text and rom base */
    unsigned long ki_datasize;	/* data size */
    unsigned long ki_database;	/* data base */
    unsigned long ki_bsssize;	/* bss size */
    unsigned long ki_bssbase;	/* bss base */
} kernelinfo;

/*
 * The disk geometry of the disk we booted from
 */
struct disk {
    unsigned short dk_diskid;	/* physical disk id */
    unsigned short dk_cyl;	/* # of cylinders */
    unsigned char dk_heads;	/* # of heads */
    unsigned char dk_sectrk;	/* # of sectors per track */
} disk;

unsigned long part_off;		/* boot partition offset on disk */

char diskblock[512];		/* place holder for disk block */
long diskblk;			/* current disk block */
int  diskoff;			/* offset within diskblock */

char line[LINESIZE];		/* input line */
char pclabel[PCL_BOOTSIZE];	/* pc label (master boot block) */
struct bootinfo bootinfo;	/* boot information */
struct disklabel label;		/* Amoeba disk label */
struct kerneldir kerneldir;	/* kernel directory */
long kdir_sector;		/* kernel directory offset */

uint16 checksum();
uint32 convert32();
long dsseg();
void type3_check();
void readkerneldir(), loadkernel(), loadsegment();
void diskread(), ldiskread(), getline(), strcpy();

main()
{
    printf("%s Standalone Boot Program\n\n", AMOEBA_VERSION);

    /*
     * Collect disk parameters from ROM BIOS
     */
    bios_diskreset();
    type3_check();
    bootinfo.bi_magic = BI_MAGIC;
    bios_diskparams(BI_HDPARAM0, &bootinfo.bi_hdinfo[0]);
    bios_diskparams(BI_HDPARAM1, &bootinfo.bi_hdinfo[1]);

#ifdef BOOT_DEBUG
    printf("Boot disk id %d: ncyl %d, nheads %d, sec/trk %d\n",
	disk.dk_diskid, disk.dk_cyl, disk.dk_heads&0xFF, disk.dk_sectrk&0xFF);
    printf("0: ncyl %d, nheads %d, precomp %x, ctrl %x, sec/trk %d\n",
	bootinfo.bi_hdinfo[0].hdi_ncyl, bootinfo.bi_hdinfo[0].hdi_nheads&0xFF,
	bootinfo.bi_hdinfo[0].hdi_precomp, bootinfo.bi_hdinfo[0].hdi_ctrl&0xFF,
	bootinfo.bi_hdinfo[0].hdi_nsectrk&0xFF);
    printf("1: ncyl %d, nheads %d, precomp %x, ctrl %x, sec/trk %d\n\n",
	bootinfo.bi_hdinfo[1].hdi_ncyl, bootinfo.bi_hdinfo[1].hdi_nheads&0xFF,
	bootinfo.bi_hdinfo[1].hdi_precomp, bootinfo.bi_hdinfo[1].hdi_ctrl&0xFF,
	bootinfo.bi_hdinfo[1].hdi_nsectrk&0xFF);
#endif

    /* read the kernel directory from disk */
    readkerneldir();
    if (kerneldir.kd_entry[0].kde_size == 0) {
	printf("No bootable kernel; halting system.\n");
	halt();
    }

    printf("Default kernel: %s (just press <ENTER> or wait ~%d seconds)\n\n",
	kerneldir.kd_entry[0].kde_name, TIMEOUT);

    /*
     * Main command loop. Note that gets will return a '\n' upon
     * a timeout, this will automatically start the first kernel
     * in the directory.
     */
    for (;;) {
	register int i;

	getline(line, LINESIZE);
	if (line[0] == '\0') {
	    /* load default kernel */
	    strcpy(line, kerneldir.kd_entry[0].kde_name);
	    loadkernel(&kerneldir.kd_entry[0]);
	} else if (line[0] == '?' && !line[1]) {
	    /* display list of bootable kernels */
	    printf("Bootable kernel(s):\n");
	    for (i = 0; kerneldir.kd_entry[i].kde_size!=0 && i<KD_NENTRIES; i++)
		printf("    %s (%ld:%ld)\n",
		    kerneldir.kd_entry[i].kde_name,
		    kerneldir.kd_entry[i].kde_secnum,
		    kerneldir.kd_entry[i].kde_size);
	} else  {
	    /* search for user specified kernel */
	    for (i = 0; kerneldir.kd_entry[i].kde_size!=0 && i<KD_NENTRIES; i++)
		if (!compare(line, kerneldir.kd_entry[i].kde_name)) {
		    loadkernel(&kerneldir.kd_entry[i]);
		    break;
		}
	    if (kerneldir.kd_entry[i].kde_size == 0 || i == KD_NENTRIES)
		printf("%s: not found\n", line);
	}
    }
}

/*
 * Check for a strange BIOS disk parameter translation that was used by a few
 * BIOS manufacturers, but is now obsolete.  This translation has bits 10-11
 * of the cylinder number in the high two bits of the head register.  There is
 * no foolproof way to test for this type, so we use the practical approach:
 * The type 3 BIOS we are looking for will have exactly 16 heads.  There are
 * no normal BIOSes that we know of that use a large number of heads that is
 * not a power of two or exactly 255.
 */
void
type3_check()
{
    if (disk.dk_heads < 0x40) return;		/* small disk */
    if ((disk.dk_heads & 0x3F) != 16) return;	/* #heads != 16 */

    /* Assume a type 3 BIOS; correct cyls & heads. */
    disk.dk_cyl = ((disk.dk_cyl - 1) | ((disk.dk_heads >> 6) << 10)) + 1;
    disk.dk_heads &= 0x3F;
}

/*
 * Read kernel directory from disk. On a harddisk this routine automatically
 * fetches the kernel directory from disk by examining the active partition
 * and subsequent Amoeba sub-partitions. On a floppy disk it just reads some
 * predefined disk block.
 */
void
readkerneldir()
{
    register int entry;

#ifdef FLBOOT
    /* booting from floppy, just read predefined disk block */
    diskread((long)AMOEBA_BOOT_SIZE + 1, &kerneldir, 1);
    if (convert32(kerneldir.kd_magic) != KD_MAGIC) {
	printf("Malformed kernel directory; halting system.\n");
	halt();
    }
    kdir_sector = AMOEBA_BOOT_SIZE + 1;
#endif
#ifdef HDBOOT
    register struct pcpartition *pe;
    register struct disklabel *lp;
    register int i;

    /* read master boot block */
    diskread(0L, pclabel, 1);
    if (!PCL_MAGIC(pclabel)) { /* "cannot happen" */
	printf("Malformed master boot program; halting system.\n");
	halt();
    }

    /* scan through partition table for active Amoeba partition */
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	if (pe->pp_first == part_off && pe->pp_sysind == PCP_AMOEBA)
	    break;
    }
    if (i == PCL_NPART) { /* "cannot happen" */
	printf("No Amoeba boot partition found; halting system.\n");
	halt();
    }

    /* scan through Amoeba disk label to find kernel directory */
    diskread(pe->pp_first + AMOEBA_BOOT_SIZE, &label, 1);
    if (label.d_magic != AM_DISK_MAGIC || checksum((uint16 *) &label) != 0) {
	printf("Malformed Amoeba disk label; halting system.\n");
	halt();
    }
    for (i = 0; i < MAX_PARTNS_PDISK; i++) {
	if (label.d_ptab[i].p_numblks == 0) continue;
	diskread(label.d_ptab[i].p_firstblk + 1, &kerneldir, 1);
	if (convert32(kerneldir.kd_magic) == KD_MAGIC)
	    break;
    }
    if (i == MAX_PARTNS_PDISK) {
	printf("Malformed kernel directory; halting system.\n");
	halt();
    }
    kdir_sector = label.d_ptab[i].p_firstblk + 1;
#endif

    /*
     * The Amoeba kernel directory is stored on disk in big-endian format.
     * This means that we have to convert it to little-endian before we
     * can use it.
     */
    for (entry = 0; entry < KD_NENTRIES; entry++) {
	kerneldir.kd_entry[entry].kde_secnum =
	    convert32(kerneldir.kd_entry[entry].kde_secnum);
	kerneldir.kd_entry[entry].kde_size =
	    convert32(kerneldir.kd_entry[entry].kde_size);
    }
}

/*
 * Convert big-endian long to little-endian long
 */
uint32
convert32(v)
    uint32 v;
{
    uint32 r;

    r  = (v >> 24) & 0x000000FFL;
    r |= (v >>  8) & 0x0000FF00L;
    r |= (v <<  8) & 0x00FF0000L;
    r |= (v << 24) & 0xFF000000L;
    return r;
}

uint16
checksum(p)
    uint16 *p;
{
    register int i;
    uint16 sum;

    i = sizeof (disklabel) / sizeof (uint16);
    sum = 0;
    while (--i >= 0)
	sum = sum ^ *p++;
    return sum;
}

/*
 * Load the kernel into physical memory keeping in account
 * the padding needed. If everything was loaded succesfully
 * the kernel's DS value is calculated and the kernel is
 * executed.
 */
void
loadkernel(kdep)
    struct kerneldir_ent *kdep;
{
    register int i;
    struct outhead *ohptr;
    struct outsect *osptr;
    long loadaddr;
    long off, n;
    long nbytes;

    /* read kernel's first block */
    diskblk = kdep->kde_secnum + kdir_sector;
    diskread(diskblk++, diskblock, 1);
    ohptr = (struct outhead *) diskblock;
    if (ohptr->oh_magic != O_MAGIC) {
	printf("%s: bad magic\n", kdep->kde_name);
	return;
    }
    printf("%s: version stamp %o:\n", kdep->kde_name, ohptr->oh_stamp);

    kernelinfo.ki_textsize = kernelinfo.ki_textbase = 0;
    kernelinfo.ki_datasize = kernelinfo.ki_database = 0;
    kernelinfo.ki_bsssize = kernelinfo.ki_bssbase = 0;

    /* go through all the sections, extracting relevant info */
    for (i = 0, diskoff = SZ_HEAD; i < ohptr->oh_nsect; i++, diskoff+=SZ_SECT) {
	osptr = (struct outsect *) &diskblock[diskoff];
	printf("Section %d: base 0x%lx, size 0x%lx, align 0x%lx\n",
	    i, osptr->os_base, osptr->os_size, osptr->os_lign);
	switch (i) {
	case 0:	/* text section */
	    loadaddr = osptr->os_base;
	    kernelinfo.ki_textsize += osptr->os_size;
	    kernelinfo.ki_textbase += osptr->os_base;
	    break;
	case 1:	/* rom section */
	    kernelinfo.ki_textsize += osptr->os_size;
	    break;
	case 2: /* data section */
	    kernelinfo.ki_datasize += osptr->os_size;
	    kernelinfo.ki_database += osptr->os_base;
	    break;
	case 3:	/* bss section */
	    kernelinfo.ki_bsssize += osptr->os_size;
	    kernelinfo.ki_bssbase += osptr->os_base;
	    break;
	default:
	    break;
	}
    }
    kernelinfo.ki_bsssize = (kernelinfo.ki_bsssize + 0x0F) & ~0x0F;

    /*
     * Bound and sanity checks: There are three things we have to check,
     * first the load address should be at LOADADDR, second the kernel
     * text and data space must not overwrite this boot code. And, the
     * total kernel size should be less than 640 Kb.
     */
    if (loadaddr != LOADADDR) {
	printf("WARNING: Kernel should start at 0x%x, not at 0x%x.\n",
	    LOADADDR, loadaddr);
	loadaddr = LOADADDR;
    }
    if (kernelinfo.ki_datasize + kernelinfo.ki_database  >=  BOOTADDR) {
	printf("Kernel would overwrite boot program.\n");
	return;
    }
    if (kernelinfo.ki_bssbase + kernelinfo.ki_bsssize >= LOWMEMSIZE) {
	printf("Kernel (%d bytes) too large.\n",
	    kernelinfo.ki_bssbase + kernelinfo.ki_bsssize);
	return;
    }

    /*
     * Load text segment
     */
    loadsegment(&loadaddr, kernelinfo.ki_textsize,
    	kernelinfo.ki_database-kernelinfo.ki_textbase-kernelinfo.ki_textsize);

    /*
     * Load data segment
     */
    loadsegment(&loadaddr, kernelinfo.ki_datasize,
    	kernelinfo.ki_bssbase-kernelinfo.ki_database-kernelinfo.ki_datasize);

    /*
     * Pass command line along, and copy boot information to the place
     * where the kernel expects it.
     */
    strcpy(bootinfo.bi_line, line);
    phys_copy(((long)dsseg() << 4) + (long)&bootinfo,
	BI_ADDR, (long) sizeof(struct bootinfo));

    /*
     * Start running Amoeba kernel. Since the floppy disk motor isn't
     * always stopped correctly (e.g. with a pool kernel) we do it here.
     */
    stop_motor();
    startkernel();
    /* NOTREACHED */
}

/*
 * Load segment into memory taking care of padding
 */
void
loadsegment(loadaddr, nbytes, pad)
    long *loadaddr;
    long nbytes, pad;
{
    long n = MIN(nbytes, BLOCKSIZE - diskoff);
    int fragment;
    int nblks;

    if (n == 0) return;
    phys_copy((dsseg() << 4) + (long)diskblock + (long)diskoff, *loadaddr, n);
    *loadaddr += n;
    diskoff += n;
    nbytes -= n;

    if (nbytes == 0) return;

    /* Get most of the segment */
    fragment = nbytes % BLOCKSIZE;
    nblks = nbytes / BLOCKSIZE;
    ldiskread(diskblk, *loadaddr, nblks);
    diskblk += nblks;
    *loadaddr += nbytes - fragment;

    /* And the little fragment at the end */
    if (fragment == 0) {
	diskoff = BLOCKSIZE;
    } else {
	diskread(diskblk, diskblock, 1);
	phys_copy((dsseg() << 4) + (long)diskblock, *loadaddr, (long) fragment);
	diskblk++;
	*loadaddr += fragment;
	diskoff = fragment;
    }
    *loadaddr += pad;
}

/*
 * Read an absolute block from boot disk into a local buffer
 */
void
diskread(blk, ptr, nblks)
    long blk;
    char *ptr;
    int nblks;
{
    ldiskread(blk, (dsseg() << 4) + (long)ptr, nblks);
}

/*
 * Read an absolute block from boot disk to an absolute address
 */
void
ldiskread(blk, addr, nblks)
    long blk;
    long addr;
    int nblks;
{
    int stat, retry, cyl, head, sec;
    unsigned count, dma_count;

    while (nblks > 0) {
	cyl = blk / (disk.dk_heads * disk.dk_sectrk);
	sec = blk % disk.dk_sectrk;
	head = (blk % (disk.dk_heads * disk.dk_sectrk)) / disk.dk_sectrk;

	/* Number of bytes left before we hit a 64K boundary: */
	dma_count = 0x10000L - (addr & 0xFFFF);

	if (dma_count < BLOCKSIZE) {
	    /* Double buffer one block */
	    ldiskread(blk, (dsseg() << 4) + (long)diskblock, 1);
	    phys_copy((dsseg() << 4) + (long)diskblock, addr, (long) BLOCKSIZE);
	    count = 1;
	} else {
	    /* Number of sectors we want to read */
	    count = nblks;

	    /* Read the blocks before the boundary */
	    dma_count /= BLOCKSIZE;
	    if (count > dma_count)
		count = dma_count;

	    /* Not more than what's left on the track */
	    if (count > disk.dk_sectrk - sec)
		count = disk.dk_sectrk - sec;

#ifdef BOOT_DEBUG
	    printf("diskread(): cyl %d, head %d, sec = %d, nblks %d\n",
		cyl, head, sec+1, count);
#endif
	    for (retry = 0; retry < 3; retry++) {
		stat = bios_diskread(disk.dk_diskid,
			cyl & 0xFF,
			head + ((cyl >> 4) & 0xC0),	/* allow cyl > 1024 */
			sec + 1 + ((cyl >> 2) & 0xC0),
			(int) (addr & 0xF),		/* offset */
			(int) (addr >> 4),		/* segment */
			count);
		if (stat == 0)
		    break;
		bios_diskreset(disk.dk_diskid);
	    }
	    if (retry == 3) {
		printf("Disk read failed (%x): block %ld, nblocks %d: ",
		    stat, blk, count);
		printf("head %d, cylinder %d, sector %d\n", head, cyl, sec+1);
		printf("Halting system.\n");
		halt();
	    }
	}
	addr += count * BLOCKSIZE;
	blk += count;
	nblks -= count;
    }
}

unsigned long
time()
{
    int sec, min, hour;

    /* BIOS time is stored in bcd */
    bios_getrtc(&sec, &min, &hour);
    sec = BCD2DEC(sec);
    min = BCD2DEC(min);
    hour = BCD2DEC(hour);
    return ((unsigned long)hour * 3600L)
      + ((unsigned long )min * 60L) + (unsigned long )sec;
}

int
getchar()
{
    unsigned long t0;
    int ch;

    for (t0 = time(); (ch = bios_getchar()) == -1; ) {
	if (time() > t0 + TIMEOUT) {
	    printf("\n");
	    return '\n';
	}
    }
    return ch;
}

void
getline(line, limit)
    char *line;
    int limit;
{
    char *p, ch;

    printf("boot: ");
    for (p = line; (ch = getchar()) != '\r' && ch != '\n' && (line-p) < limit;){
	if (ch == '\b') {
	     if (p > line) {
		p--;
	     	printf(" \b");
	     }
	} else
	    *p++ = ch;
    }
    *p = '\0';
}

int
compare(s, t)
    register char *s, *t;
{
    while (*s == *t++)
	if (*s++ == '\0' || *s == ' ') return 0;
    return *s - *--t;
}

void
strcpy(s, t)
    register char *s, *t;
{
    while (*s++ = *t++) /* nothing */;
}
