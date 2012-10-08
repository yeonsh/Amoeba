/*	@(#)mkkdir.c	1.3	96/02/27 10:16:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * mkkdir.c
 *
 * Make kernel directory from an human readable format.
 */
#include <stdio.h>
#include <byteorder.h>
#include <kbootdir.h>

struct kerneldir kd;
char wrbuf[512];

main()
{
    char str[KDE_NAMELEN];
    char scanpat[40];
    long secnum, size;
    int entry;
    int lastmatch;

    sprintf(scanpat, "%%d %%d %%%ds\n", KDE_NAMELEN-1);

    kd.kd_magic = KD_MAGIC;
    enc_l_be(&kd.kd_magic);

    entry = 0;
    while (entry < KD_NENTRIES && (lastmatch=scanf(scanpat, &secnum, &size, str)) == 3) {
	kd.kd_entry[entry].kde_secnum = secnum;
	enc_l_be(&kd.kd_entry[entry].kde_secnum);
	kd.kd_entry[entry].kde_size = size;
	enc_l_be(&kd.kd_entry[entry].kde_size);
	strcpy(kd.kd_entry[entry].kde_name, str);
	entry++;
    }
    if (!feof(stdin)) {
	if (lastmatch!=3)
	    fprintf(stderr, "Line %d malformed\n", entry+1);
	else
	    fprintf(stderr, "Maximum of %d lines exceeded\n", entry);
	exit(-1);
    }
    (void) memcpy(wrbuf, (char *) &kd, sizeof(kd));
    write(1, wrbuf, 512);
    exit(0);
}
