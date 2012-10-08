/*	@(#)floppy_load.c	1.7	96/02/27 13:51:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * floppy_load.c
 *
 * Load ram disk images from floppy disk. This procedure is only used
 * for the coldstart kernel. The MIN_CLICKS define prevents the kernel
 * from running with less than 16 Mb of memory.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <machdep.h>
#include <disk/disk.h>
#include <module/direct.h>
#include <byteorder.h>
#include <vollabel.h>
#include <sys/proto.h>

#include "floppy.h"

/* ram disk image dumps volume name */
#define	VOLUME_NAME	"RAMDISK-IMAGE"

/* arbitrary retry limit */
#define	TTY_RETRIES	100

/* minimum number clicks needed (at least 15 Mb) */
#define	MIN_CLICKS	((15 * 1024 * 1024) / CLICKSIZE)

void cprintf();
char *cgetline();
errstat flp_read();
unsigned short updcrc();
phys_clicks lastclick();

/*
 * Compute the offset and volume of the ramdisk image on floppy and
 * try reading it in. All kinds of protective loops are used to prevent
 * the user from disasters.
 */
errstat
floppy_loader(membase, size, name, longpar)
    char *membase;
    int32 size;
    char *name;
    long longpar;
{
    union vollabel vollabel;
    capability ttycap;
    disk_addr nblks;
    errstat err;
    int32 bsize;
    int vol, i;

    /* save guard: to prevent the ignorant */
    if (lastclick() < MIN_CLICKS)
	panic("Not enough memory to run the Amoeba coldstart kernel");

    /* lookup /tty:00 for input device */
    for (i = 0; i < TTY_RETRIES; i++) {
	if ((err = dir_lookup((capability *) 0, "/tty:00", &ttycap)) == STD_OK)
	    break;
	pit_delay(3000);
    }
    if (i == TTY_RETRIES) return err;

    vol = 0;
    bsize = size >> D_PHYS_SHIFT;
    while (bsize > 0) {
	unsigned short crc = -1; /* compute CCITT CRC polynomial */

	/* query user for ram disk image dumps */
	cprintf(&ttycap,
	      "Please insert floppy disk volume %s-%ld, then press return ...",
	      VOLUME_NAME, vol);
	(void) cgetline(&ttycap);

	/* read volume label */
	err = flp_read(FLP_BASEREG, 0, (disk_addr)0, (disk_addr)1,
						(char *)&vollabel);
	if (err != STD_OK) return err;

	/* convert volume label */
	dec_l_le(&vollabel.vl_magic);
	dec_s_le(&vollabel.vl_crc);
	dec_l_le(&vollabel.vl_curvol);
	dec_l_le(&vollabel.vl_nextvol);
	dec_l_le(&vollabel.vl_mediasize);
	dec_l_le(&vollabel.vl_datasize);

	/* sanity checks */
	if (vollabel.vl_magic != VL_MAGIC) {
	    cprintf(&ttycap, "Incorrect volume magic\n");
	    continue;
	}
	if (strcmp(vollabel.vl_name, VOLUME_NAME)) {
	    cprintf(&ttycap, "Incorrect volume name '%s'\n", vollabel.vl_name);
	    continue;
	}
	if (vollabel.vl_curvol != vol) {
	    cprintf(&ttycap,
		"Incorrect volume number %d\n", vollabel.vl_curvol);
	    continue;
	}

	/* read data from disk (starts at block 1) */
	nblks = MIN(bsize, vollabel.vl_datasize);
	err = flp_read(FLP_BASEREG, 0, (disk_addr)1, nblks, membase);
	if (err != STD_OK) {
	    cprintf(&ttycap, "Floppy read failed\n");
	    return err;
	}

	/* check volume's CRC check sum */
	crc = volcrc(crc, membase, nblks << D_PHYS_SHIFT);
	if (vollabel.vl_crc != crc) 
	    cprintf(&ttycap, "WARNING: wrong CRC check sum\n");

	vol = vollabel.vl_nextvol;
	membase += (nblks << D_PHYS_SHIFT);
	bsize -= nblks;
    }
    return STD_OK;
}

#include "file.h"

/* VARARGS */
void
cprintf(ttycap, fmt, a0, a1)
    capability *ttycap;
    char *fmt;
{
    char buf[100];

    (void) bprintf(buf, &buf[100], fmt, a0, a1);
    fswrite(ttycap, 0L, buf, (long)strlen(buf));
}

char *
cgetline(ttycap)
    capability *ttycap;
{
    static char buf[100];

    fsread(ttycap, 0L, buf, (long)sizeof(buf));
    return buf;
}

