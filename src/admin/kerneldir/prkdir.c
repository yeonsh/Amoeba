/*	@(#)prkdir.c	1.2	94/04/06 11:47:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * prkdir.c
 *
 * Print kernel directory in an human readable form.
 */
#include <stdio.h>
#include <kbootdir.h>
#include <byteorder.h>

main()
{
    struct kerneldir kd;
    int entry;

    if (fread((char *)&kd, sizeof(kd), 1, stdin) != 1) {
	fprintf(stderr, "Couldn't read kerneldir\n");
	exit(1);
    }

    dec_l_be(&kd.kd_magic);
    if (kd.kd_magic != KD_MAGIC) {
	fprintf(stderr, "Wrong magic number, check input\n");
	exit(1);
    }

    for (entry = 0; entry < KD_NENTRIES; entry++) {
	dec_l_be(&kd.kd_entry[entry].kde_secnum);
	dec_l_be(&kd.kd_entry[entry].kde_size);
	if (kd.kd_entry[entry].kde_size) {
	    printf("%ld\t%ld\t%s\n", kd.kd_entry[entry].kde_secnum,
		kd.kd_entry[entry].kde_size, kd.kd_entry[entry].kde_name);
	}
    }

    exit(0);
}
