/*	@(#)vdisk.c	1.6	96/02/27 10:11:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "monitor.h"
#include "byteorder.h"
#include "server/boot/bsvd.h"
#include "module/direct.h"
#include "module/disk.h"
#include "stdlib.h"
#include "string.h"
#include "svr.h"

#include "module/ar.h"
/*
 *	I/O primitives
 */

extern char *mymalloc();

static capability diskcap;	/* Capability for our stable storage */

union {
    struct bsvd_head head;
    char data[BLOCKSIZE];
} block;

extern obj_rep objs[MAX_OBJ];

static bool
TestBlock0()	/* Test all of it; except the magic string */
{
    return
	(block.head.conf.size != 0 && block.head.conf.block1 > 0 &&
	block.head.proccaps.size != 0 && block.head.proccaps.block1 > 0);
} /* TestBlock0 */


static bool
bootsvr_disk(rootdir, name, diskcap)
capability * rootdir;
char *name;
capability *diskcap;
{
    errstat	err;

    if ((err = dir_lookup(rootdir, name, diskcap)) != STD_OK) {
	prf("%nLookup(%s) glitch (%s)\n", name, err_why(err));
	return 0;
    }
    if ((err = disk_read(diskcap, L2BLOCK, (disk_addr) 0,
			    (disk_addr) 1, (bufptr) &block.head)) != STD_OK) {
	prf("%ncannot disk_read %s (%s)\n", name, err_why(err));
	return 0;
    }
    if (strcmp(BOOT_MAGIC, block.head.magic) == 0) {
	DEC_BSVDP(&block.head);
	if (!TestBlock0()) {
	    prf("%n%s: invalid config or proccap file\n", name);
	    return 0;
	}
	return 1;
    }
    return 0;
}


/*
 *	Find an appropiate disk, read block 0 - first try the disk suggested
 *	in the capability environment.
 */
static bool
FindDisk()
{
    static int got_diskcap;
    static capability scap; /* Supercap - hidden carefully */
    static char diskname[20];  /* Name of the disk we are using */
    static capability *homecap;
    struct dir_open *dp;
    errstat err;
    capability *dcap;
    char *name;

    if (diskname[19] != '\0') {
	prf("%nDiskname is too long!\n");
	abort();
    }
    if (!got_diskcap) {
	if ((homecap = getcap("ROOT")) == NULL) {
	    prf("%ngot no ROOT cap\n");
	    return 0;
	}

	/* See if someone passed us the disk in the cap environment */
	if ((name = getenv("BOOT_VDISK")) != NULL &&
					bootsvr_disk(homecap, name, &diskcap)) {
	    got_diskcap = 1;
	    (void) strncpy(diskname, name, 20);
	}
	else {
	    /* Look through available vdisks in ROOT directory */
	    dp = dir_open(homecap);
	    if (dp == NULL) {
		prf("%nCannot open directory containing vdisk cap\n");
		return 0;
	    }

	    while ((name = dir_next(dp)) != NULL) {
		if (strncmp(name, "vdisk:", 6) == 0 &&
				    bootsvr_disk(homecap, name, &diskcap)) {
			got_diskcap = 1;
			break;
		}
	    }
	    (void) strncpy(diskname, name, 20);
	    if ((err = dir_close(dp)) != STD_OK)
		prf("%nerror on dir_close (%s)\n", err_why(err));
	}

	if (!got_diskcap) {
	    prf("%nCould not find appropiate vdisk in <ROOT>\n");
	    return 0;
	}

	if (debugging || verbose)
	    prf("%nUsing disk %s - size %d\n", diskname, block.head.conf.size);
    }
    else {
	/* We have to re-read the header block in case it changed */
	if (homecap == (capability *) 0 || diskname[0] == '\0') {
	    prf("%nHELP: FindDisk has no diskname!\n");
	    return 0;
	}
	if (!bootsvr_disk(homecap, diskname, &diskcap))
	    return 0;
    }

    scap = block.head.supercap;
    get_capp = &scap;

    /* Compute putport */
    putcap.cap_priv = scap.cap_priv;
    priv2pub(&get_capp->cap_port, &putcap.cap_port);
    return 1;
} /* FindDisk */

/*
 *	Read the process capabilities
 *	They are stored as capability/name pairs.
 */
bool
ReadProcCaps()
{
    bufptr data;
    int size;
    disk_addr nblock;
    errstat err;

    if (!FindDisk()) return 0;
    MON_EVENT("Reading proccaps from disk");
    size = block.head.proccaps.size;
    nblock = (size + BLOCKSIZE - 1) / BLOCKSIZE;
    if ((data = mymalloc((int) nblock * BLOCKSIZE)) == NULL)
	return 0;
    err= disk_read(&diskcap, L2BLOCK, block.head.proccaps.block1, nblock, data);
    if (err != STD_OK) {
	prf("%nDiskread failed (%s)\n", err_why(err));
	return 0;
    }
    Get_data(data, data + size, objs, SIZEOFTABLE(objs));
    free((_VOIDSTAR) data);
    return 1;
} /* ReadProcCaps */

void
SaveProcCaps()
{
    int size;
    long nblock;
    errstat err;
    char *p;
    static mutex globmu;

    if (!FindDisk()) return;
    mu_lock(&globmu);

    /* Compute required size */
    size = Size_data(objs, SIZEOFTABLE(objs));
    if (debugging) prf("%nData takes %d bytes\n", size);
    nblock = (size + BLOCKSIZE) / BLOCKSIZE;
    p = mymalloc((int) nblock * BLOCKSIZE);
    if (p == NULL) {
	mu_unlock(&globmu);
	return;
    }
    p[size] = '\0';	/* Null terminate */
    if (!Put_data(p, p + size, objs, MAX_OBJ)) {
	prf("%nCannot Put_data?!\n");
	mu_unlock(&globmu);
	free((_VOIDSTAR) p);
	return;
    }

    /* Write data to disk */
    MON_EVENT("disk writes");
    err = disk_write(&diskcap, L2BLOCK, block.head.proccaps.block1, nblock, p);
    if (err != STD_OK)
	prf("%nCould not save proccaps (%s)\n", err_why(err));
    else
	SaveState = 0;
    mu_unlock(&globmu);
    free((_VOIDSTAR) p);
    if (debugging) prf("%nSaved state\n");
} /* SaveProcCaps */

static char *conf_text = NULL;
static unsigned conf_size = 0;
static unsigned long bf_pos;

/*
 *	Read the configuration table in core
 */
bool
ReadConfig()
{
    errstat err;
    long n_block;

    bf_pos = 0;
    if (!FindDisk()) return 0;
    n_block = (block.head.conf.size + BLOCKSIZE - 1) / BLOCKSIZE;
    if (conf_size < block.head.conf.size) {
	if (conf_text != NULL) free((_VOIDSTAR) conf_text);
	conf_size = block.head.conf.size;
	conf_text = (char *) malloc((size_t) n_block * BLOCKSIZE);
	if (conf_text == NULL) {
	    conf_size = 0;
	    prf("%nMalloc failure for reading\n");
	    return 0;
	}
    }

    /* Read data: */
    err = disk_read(&diskcap, L2BLOCK, block.head.conf.block1,
					n_block, conf_text);
    if (err != STD_OK) {
	prf("%ncould not read disk data (%s)\n", err_why(err));
	return 0;
    }
    return 1;
} /* ReadConfig */

/*
 *	I/O routines:
 */
int
f_get()
{
    int ch;
    if (bf_pos >= conf_size) {
	ch = 0;	/* Lex wants a null char instead of EOF here... */
    } else {
	ch = conf_text[bf_pos++] & 0xff;
    }
    return ch;
} /* f_get */

int
f_unget(ch)
    int ch;
{
    if (bf_pos == 0) return EOF;
    if (ch < 0) {
	return -1;
    }
    return conf_text[--bf_pos] = ch;
} /* f_unget */
