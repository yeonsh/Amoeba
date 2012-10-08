/*	@(#)encode.c	1.6	96/02/27 10:12:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "byteorder.h"
#include "cmdreg.h"
#include "stderr.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "module/disk.h"
#include "vdisk/disklabel.h"
#include "dsktab.h"
#include "vdisk/vdisk.h"
#include "host_dep.h"
#include "proto.h"

extern char *		Prog;
extern int		Host_os;
extern host_dep *	Host_p[];


/*ARGSUSED*/
static void
decode_geom_BE(p)
geometry * p;
{
    DEC_INT16_BE(&p->g_bpt);
    DEC_INT16_BE(&p->g_bps);
    DEC_INT16_BE(&p->g_numcyl);
    DEC_INT16_BE(&p->g_altcyl);
    DEC_INT16_BE(&p->g_numhead);
    DEC_INT16_BE(&p->g_numsect);
}


/*ARGSUSED*/
static void
decode_geom_LE(p)
geometry * p;
{
    DEC_INT16_LE(&p->g_bpt);
    DEC_INT16_LE(&p->g_bps);
    DEC_INT16_LE(&p->g_numcyl);
    DEC_INT16_LE(&p->g_altcyl);
    DEC_INT16_LE(&p->g_numhead);
    DEC_INT16_LE(&p->g_numsect);
}


/*ARGSUSED*/
static void
encode_geom_BE(p)
geometry * p;
{
    ENC_INT16_BE(&p->g_bpt);
    ENC_INT16_BE(&p->g_bps);
    ENC_INT16_BE(&p->g_numcyl);
    ENC_INT16_BE(&p->g_altcyl);
    ENC_INT16_BE(&p->g_numhead);
    ENC_INT16_BE(&p->g_numsect);
}


/*ARGSUSED*/
static void
encode_geom_LE(p)
geometry * p;
{
    ENC_INT16_LE(&p->g_bpt);
    ENC_INT16_LE(&p->g_bps);
    ENC_INT16_LE(&p->g_numcyl);
    ENC_INT16_LE(&p->g_altcyl);
    ENC_INT16_LE(&p->g_numhead);
    ENC_INT16_LE(&p->g_numsect);
}


static void
encode_part_tab_BE(p)
part_tab * p;
{
    int	i;

    for (i = 0; i < MAX_PARTNS_PDISK; p++, i++)
    {
	ENC_DISK_ADDR_BE(&p->p_firstblk);
	ENC_DISK_ADDR_BE(&p->p_numblks);
	ENC_INT16_BE(&p->p_piece);
	ENC_INT16_BE(&p->p_many);
    }
}


static void
encode_part_tab_LE(p)
part_tab * p;
{
    int	i;

    for (i = 0; i < MAX_PARTNS_PDISK; p++, i++)
    {
	ENC_DISK_ADDR_LE(&p->p_firstblk);
	ENC_DISK_ADDR_LE(&p->p_numblks);
	ENC_INT16_LE(&p->p_piece);
	ENC_INT16_LE(&p->p_many);
    }
}


static void
decode_part_tab_BE(p)
part_tab * p;
{
    int	i;

    for (i = 0; i < MAX_PARTNS_PDISK; p++, i++)
    {
	DEC_DISK_ADDR_BE(&p->p_firstblk);
	DEC_DISK_ADDR_BE(&p->p_numblks);
	DEC_INT16_BE(&p->p_piece);
	DEC_INT16_BE(&p->p_many);
    }
}


static void
decode_part_tab_LE(p)
part_tab * p;
{
    int	i;

    for (i = 0; i < MAX_PARTNS_PDISK; p++, i++)
    {
	DEC_DISK_ADDR_LE(&p->p_firstblk);
	DEC_DISK_ADDR_LE(&p->p_numblks);
	DEC_INT16_LE(&p->p_piece);
	DEC_INT16_LE(&p->p_many);
    }
}


static void
encode_dklab_BE(label)
disklabel *	label;
{
    encode_geom_BE(&label->d_geom);
    encode_part_tab_BE(label->d_ptab);
    ENC_INT32_BE(&label->d_magic);
    ENC_INT16_BE(&label->d_filler2);
    ENC_INT16_BE(&label->d_chksum);
}


static void
encode_dklab_LE(label)
disklabel *	label;
{
    encode_geom_LE(&label->d_geom);
    encode_part_tab_LE(label->d_ptab);
    ENC_INT32_LE(&label->d_magic);
    ENC_INT16_LE(&label->d_filler2);
    ENC_INT16_LE(&label->d_chksum);
}


static void
decode_dklab_BE(label)
disklabel *	label;
{
    decode_geom_BE(&label->d_geom);
    decode_part_tab_BE(label->d_ptab);
    DEC_INT32_BE(&label->d_magic);
    DEC_INT16_BE(&label->d_filler2);
    DEC_INT16_BE(&label->d_chksum);
}


static void
decode_dklab_LE(label)
disklabel *	label;
{
    decode_geom_LE(&label->d_geom);
    decode_part_tab_LE(label->d_ptab);
    DEC_INT32_LE(&label->d_magic);
    DEC_INT16_LE(&label->d_filler2);
    DEC_INT16_LE(&label->d_chksum);
}


/*
 * read_amlabel
 *
 *	Reads the 512 byte amoeba-partition label from the disk specified by
 *	diskcap at block 'blocknum'.
 */

errstat
read_amlabel(diskcap, diskname, blocknum, label)
capability *	diskcap;	/* disk controller to write to */
char *		diskname;	/* name of disk to be written to */
disk_addr	blocknum;	/* where to write it on the disk */
disklabel *	label;		/* block of info to write */
{
    errstat	err;

    err = disk_read(diskcap, D_PHYS_SHIFT, blocknum, (disk_addr) 1,
							(char *) label);
    if (label->d_magic == AM_DISK_MAGIC && checksum((uint16 *) label) != 0)
    {
	printf("read_amlabel: warning: bad checksum on amoeba label");
	printf(" at block 0x%x\n", blocknum);
	/*
	 * We continue anyway on the grounds that someone will have to make
	 * sense of it later.
	 */
    }

    if (err != STD_OK)
    {
	fprintf(stderr,
		"%s: read_amlabel: disk_read of %s block 0x%x failed (%s)\n",
		Prog, diskname, blocknum, err_why(err));
    }
    else
    {
	/* Convert the label to the correct endian for the target system */
	switch (Host_p[Host_os]->f_endian)
	{
	case BIG_E:
	    decode_dklab_BE(label);
	    break;

	case LITTLE_E:
	    decode_dklab_LE(label);
	    break;

	default:
	    fprintf(stderr, "%s: read_amlabel: unknown endian %d\n",
		    Host_p[Host_os]->f_endian);
	    break;
	}
    }
    /* We have to set the checksum since it is not endian independent */
    label->d_chksum = 0;
    label->d_chksum = checksum((uint16 *) label);

    return err;
}


/*
 * write_amlabel
 *
 *	Writes the 512 byte amoeba-partition label to the disk specified by
 *	diskcap at block 'blocknum'.
 */

errstat
write_amlabel(diskcap, diskname, blocknum, label)
capability *	diskcap;	/* disk controller to write to */
char *		diskname;	/* name of disk to be written to */
disk_addr	blocknum;	/* where to write it on the disk */
disklabel *	label;		/* block of info to write */
{
    errstat	err;
    disklabel	encoded;

    /* Convert the label to the correct endian for the target system */
    (void) memcpy(&encoded, label, sizeof (disklabel));
    switch (Host_p[Host_os]->f_endian)
    {
    case BIG_E:
	encode_geom_BE(&encoded.d_geom);
	encode_part_tab_BE(encoded.d_ptab);
	ENC_INT32_BE(&encoded.d_magic);
	ENC_INT16_BE(&encoded.d_filler2);
	/* Now calculate the checksum */
	encoded.d_chksum = 0;
	encoded.d_chksum = checksum((uint16 *) &encoded);
    	break;

    case LITTLE_E:
	encode_geom_LE(&encoded.d_geom);
	encode_part_tab_LE(encoded.d_ptab);
	ENC_INT32_LE(&encoded.d_magic);
	ENC_INT16_LE(&encoded.d_filler2);
	/* Now calculate the checksum */
	encoded.d_chksum = 0;
	encoded.d_chksum = checksum((uint16 *) &encoded);
	break;

    default:
	fprintf(stderr, "%s: write_amlabel: unknown endian %d\n",
		Host_p[Host_os]->f_endian);
	break;
    }

    /* Now do the write */
    err = disk_write(diskcap, D_PHYS_SHIFT, blocknum, (disk_addr) 1,
							    (char *) &encoded);
    if (err != STD_OK)
    {
	fprintf(stderr,
		"%s: write_amlabel: disk_write to %s block 0x%x failed (%s)\n",
		Prog, diskname, blocknum, err_why(err));
    }
    return err;
}


/*
 * writeblock
 *
 *	Writes the 512 byte 'label' to the disk specified by diskcap
 *	at block 'blocknum'.  No encoding is performed.
 */

errstat
writeblock(diskcap, diskname, blocknum, label)
capability *	diskcap;	/* disk controller to write to */
char *		diskname;	/* name of disk to be written to */
disk_addr	blocknum;	/* where to write it on the disk */
char *		label;		/* block of info to write */
{
    errstat	result;

    /* Now do the write */
    result = disk_write(diskcap, D_PHYS_SHIFT, blocknum, (disk_addr) 1, label);
    if (result != STD_OK)
    {
	fprintf(stderr,
		"%s: writeblock: disk_write to %s block 0x%x failed (%s)\n",
				    Prog, diskname, blocknum, err_why(result));
    }
    return result;
}


/*
 * am_partn_label
 *
 *	Returns 1 if the disk has a reasonable looking amoeba partition label
 *	and 0 otherwise.  Label must be current endian of machine we are
 *	running on.
 */

int
am_partn_label(label)
disklabel *	label;
{
    return (label->d_magic == AM_DISK_MAGIC && checksum((uint16 *) label) == 0);
}
