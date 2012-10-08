/*	@(#)installboot.c	1.5	96/02/27 10:05:13 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * installboot.c
 *
 * Install Amoeba boot program onto hard or floppy disk.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <stdio.h>
#include <amtools.h>
#include <byteorder.h>
#include <pclabel.h>
#include <out.h>
#include <module/disk.h>

/*
 * Fundamental constants
 */
#define	BLOCKSIZE	512	/* physical block size */
#define	LOG2BLOCKSIZE	9	/* log2(BLOCKSIZE) */

/* Amoeba (secondary) boot block */
char amoebaboot[AMOEBA_BOOT_SIZE * BLOCKSIZE];

char *program;			/* program name */
int floppy = 0;			/* floppy flag */
capability diskcap;		/* boot disk capability */

/* Forward */
void hdinstall();

extern int optind;
extern char *optarg;

#define MIN_NSECT	3	/* text, data, bss */
#define MAX_NSECT	5	/* text, rom, data, bss, debug */

main(argc, argv)
    int argc;
    char **argv;
{
    struct outhead outhead;
    struct outsect outsect[MAX_NSECT];
    int opt, fd;
    unsigned int i;
    errstat err;

    program = argv[0];
    while ((opt = getopt(argc, argv, "f")) != -1)
	if (opt == 'f') floppy++;
    argc -= optind; argv += optind;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s [-f] boot-binary capability\n", program);
	exit(1);
    }

    if ((err = name_lookup(argv[1], &diskcap)) != STD_OK) {
	fprintf(stderr, "%s: can't get capability for %s: %s\n",
		program, argv[1], err_why(err));
	exit(1);
    }

    /* read user specified amoeba boot */
    if ((fd = open(argv[0], 0)) < 0) {
	fprintf(stderr, "%s: can't open %s\n", program, argv[0]);
	exit(1);
    }
    read(fd, (char *)&outhead, sizeof outhead);
    enc_s_le(&outhead.oh_magic);
    if (outhead.oh_magic != O_MAGIC) {
	fprintf(stderr, "%s: bad magic (%o)\n", program, outhead.oh_magic);
	exit(1);
    }
    dec_s_le(&outhead.oh_nsect);
    if (outhead.oh_nsect < MIN_NSECT || outhead.oh_nsect > MAX_NSECT) {
	fprintf(stderr, "%s: invalid number of sections (%d)\n",
		program, outhead.oh_nsect);
	exit(1);
    }
    for (i = 0; i < outhead.oh_nsect; i++) {
	struct outsect *sect = &outsect[i];

	read(fd, (char *) sect, sizeof(struct outsect));
	dec_l_le(&sect->os_base);
	dec_l_le(&sect->os_size);
	dec_l_le(&sect->os_foff);
	dec_l_le(&sect->os_flen);
	dec_l_le(&sect->os_lign);
	printf("Section %d: base %ld, size %ld, foff %ld, flen %ld, align 0x%lx\n",
	       i, sect->os_base, sect->os_size, sect->os_foff,
	       sect->os_flen, sect->os_lign);
    }
    /* Read the sections separately.  We cannot do it in one chunk because
     * alignment bytes and trailing zeroes from segments may be missing from
     * the file.
     */
    for (i = 0; i < outhead.oh_nsect; i++) {
	struct outsect *sect = &outsect[i];
	long off;

	/* text segment is section 0, which should start at lowest address */
	off = sect->os_base - outsect[0].os_base;
	if (off < 0 || off + sect->os_flen > sizeof(amoebaboot)) {
	    fprintf(stderr, "%s: sect %d: invalid buffer offset %ld\n",
		    program, i, off);
	    exit(1);
	}
	if (sect->os_flen != 0) {
	    if (lseek(fd, sect->os_foff, 0) != sect->os_foff) {
		fprintf(stderr, "%s: sect %d: cannot seek to %ld\n",
			program, i, sect->os_foff);
		exit(1);
	    }
	    if (read(fd, &amoebaboot[off], sect->os_flen) != sect->os_flen) {
		fprintf(stderr, "%s: sect %d: cannot read %ld bytes at %ld\n",
			program, i, sect->os_flen, sect->os_foff);
		exit(1);
	    }
	} /* else it already is zero */
    }
    close(fd);

    /* put magic stamp on Amoeba boot (also works on a sparc :-) */
    amoebaboot[BOOT_MAGIC_OFFSET_0] = BOOT_MAGIC_BYTE_0;
    amoebaboot[BOOT_MAGIC_OFFSET_1] = BOOT_MAGIC_BYTE_1;

    if (!floppy)
	hdinstall();
    else
	flinstall();
    exit(0);
}

/*
 * Install boot program on hard disk. Determine the exact physical location
 * of the stage 2 boot program by examining the master boot block.
 */
void
hdinstall()
{
    register struct pcpartition *pe;
    char pclabel[PCL_BOOTSIZE];
    errstat err;
    int i;

    /* read boot block, which contains the partition table */
    if ((err = disk_read(&diskcap, LOG2BLOCKSIZE, 0, 1, pclabel))!=STD_OK) {
	fprintf(stderr, "%s: disk_read error: %s\n", program, err_why(err));
	exit(1);
    }

    /* check for valid partition table */
    if (!PCL_MAGIC(pclabel)) {
	fprintf(stderr, "%s: invalid partition table (%02x%02x)\n", program,
	    pclabel[BOOT_MAGIC_OFFSET_0]&0xFF, pclabel[BOOT_MAGIC_OFFSET_1]&0xFF);
	exit(1);
    }
    
    /* look for active Amoeba partition */
    for (i = 0, pe = PCL_PCP(pclabel); i < PCL_NPART; i++, pe++) {
	if (pe->pp_bootind == PCP_ACTIVE && pe->pp_sysind == PCP_AMOEBA) {
	    /* decode first & size to big endian */
	    dec_l_le(&pe->pp_first);
	    dec_l_le(&pe->pp_size);

	    /* there should be enough room */
	    if (pe->pp_size < AMOEBA_BOOT_SIZE) {
		fprintf(stderr, "%s: partition too small for Amoeba boot\n",
		    program);
	        exit(1);
	    }
	    printf("Boot at block %ld\n", pe->pp_first);

	    /* write amoeba boot program*/
	    err = disk_write(&diskcap, LOG2BLOCKSIZE,
		    pe->pp_first, AMOEBA_BOOT_SIZE, amoebaboot);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: disk write error: %s\n",
		    program, err_why(err));
		exit(1);
	    }
	    return;
	}
    }
    fprintf(stderr, "%s: no active Amoeba partition\n", program);
    exit(1);
}

/*
 * Floppy boot install consists only of writting a stripped
 * boot executable to disk. No sanity checks are performed.
 */
flinstall()
{
    errstat err;

    printf("Boot at block 0\n");
    err = disk_write(&diskcap, LOG2BLOCKSIZE, 0, AMOEBA_BOOT_SIZE, amoebaboot);
    if (err != STD_OK) {
	fprintf(stderr, "%s: disk write error: %s\n", program, err_why(err));
	exit(1);
    }
}
