/*	@(#)installboot.c	1.3	96/02/27 10:10:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "stderr.h"
#include "stdio.h"
#include "bullet/bullet.h"
#include "module/name.h"
#include "module/disk.h"
#include "byteorder.h"

#define SUN4_MAGIC	0x01030107

#define SUN3_OFFSET	1		/* 0: branch, 1: storage */
#define SUN4_OFFSET	10		/* 0-7: hdr, 8-9: branch + delay */

long bootblock[7*128];	/* 7 sectors room */

char *bootp, *vdisk, *bootfile;
capability cap_bootp, cap_vdisk, cap_bootfile;

dk_info_data vdisk_info;

lookup_cap(c, s)
capability *c;
char *s;
{

	if (name_lookup(s, c) != STD_OK) {
		fprintf(stderr, "Couldn't lookup %s\n", s);
		exit(-1);
	}
}

main(argc, argv)
char **argv;
{
	b_fsize length, bytesread;
	int ninfo, offset;
	long word0;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s bootp vdisk bootfile\n", argv[0]);
		exit(-1);
	}
	bootp = argv[1];
	vdisk = argv[2];
	bootfile = argv[3];
	lookup_cap(&cap_bootp, bootp);
	lookup_cap(&cap_vdisk, vdisk);
	lookup_cap(&cap_bootfile, bootfile);
	if (b_size(&cap_bootfile, &length) != STD_OK) {
		fprintf(stderr, "Couldn't get size for %s\n", bootfile);
		exit(-1);
	}
	if (length > sizeof(bootblock)) {
		fprintf(stderr, "Boot block too large, maximum %d bytes\n",
						sizeof(bootblock));
		exit(-1);
	}
	if (b_read(&cap_bootfile, (b_fsize) 0, (char *) bootblock,
				length, &bytesread) != STD_OK ||
				bytesread != length) {
		fprintf(stderr, "Couldn't read bootblock\n");
		exit(-1);
	}
	if (disk_info(&cap_vdisk, &vdisk_info, 1, &ninfo) != STD_OK) {
		fprintf(stderr, "Couln't get vdisk info\n");
		exit(-1);
	}
	word0 = bootblock[0];
	dec_l_be(&word0);
	if (word0 == SUN4_MAGIC) {
		fprintf(stderr, "Assuming sun4 bootblock\n");
		offset = SUN4_OFFSET;
	} else {
		offset = SUN3_OFFSET;
	}
	bootblock[offset] = vdisk_info.d_firstblk;	/* patch bootblock */
	enc_l_be(&bootblock[offset]);
	if (disk_write(&cap_bootp, 9, (disk_addr) 1, (disk_addr) 7,
					(bufptr) bootblock) != STD_OK) {
		fprintf(stderr, "Couldn't write bootblock\n");
		exit(-1);
	}
}
