/*	@(#)ramdisk.c	1.6	96/02/27 13:49:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Ram disk driver, allocates a chunk of memory and serves blocks.
*/
#include <amoeba.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stderr.h>
#include <string.h>
#include "disk/disk.h"
#include "disk/conf.h"
#include "vdisk/disklabel.h"
#include "ramdisk.h"
#include "module/rnd.h"
#include "module/prv.h"
#include "assert.h"
INIT_ASSERT
#include "sys/proto.h"

/* Global and external variables */
static struct ram_info *ram_info;

static objnum     Num_ram_disks;		/* Number of virtual disks */
extern struct ram_parameters ram_pars[];	/* Ram disks to make */

extern uint16      checksum();

/* ARGSUSED */
errstat
rd_devinit(devaddr, ivec)
	long devaddr;
	int ivec;
{
	register struct ram_parameters *ramp;

	for (ramp=ram_pars; ramp->ramp_nblocks;ramp++)
		Num_ram_disks++;
	ram_info = (struct ram_info *)
		aalloc((vir_bytes) (Num_ram_disks*sizeof(struct ram_info)), 0);
	return STD_OK;
}

/* ARGSUSED */
int
rd_unit_type(devaddr, unit)
	long devaddr;
	int unit;
{
	return UNITTYPE_FIXED;
}

rd_label(base, size, stringpar, longpar, nvdisks, vdisksizes)
char *base;
int32 size;
char *stringpar;
long longpar;
int nvdisks;
long *vdisksizes;
{
	register struct disklabel *dlp;
	disk_addr firstblk = 0;
	register int i;

	dlp = (struct disklabel *) base;
	(void) memset((_VOIDSTAR) base, 0, sizeof(*dlp));
	strncpy(dlp->d_disktype, stringpar, TYPELEN);
	dlp->d_geom.g_bpt = RAM_SECTSHIFT;
	dlp->d_geom.g_bps = RAM_SECTSHIFT;
	dlp->d_geom.g_numcyl = size>>RAM_SECTSHIFT;
	dlp->d_geom.g_altcyl = 0;
	dlp->d_geom.g_numhead = 0;
	dlp->d_geom.g_numsect = 0;

	for (i = 0; i < nvdisks; i++) {
		dlp->d_ptab[i].p_firstblk = firstblk;
		firstblk += vdisksizes[i];
		dlp->d_ptab[i].p_numblks = vdisksizes[i];
		dlp->d_ptab[i].p_piece = 0;
		dlp->d_ptab[i].p_many = 1;
		uniqport(&dlp->d_ptab[i].p_cap.cap_port);
		uniqport(&dlp->d_ptab[i].p_cap.cap_priv.prv_random);
		prv_encode(&dlp->d_ptab[i].p_cap.cap_priv, i + longpar,
			    PRV_ALL_RIGHTS,
			    &dlp->d_ptab[i].p_cap.cap_priv.prv_random);
	}

	compare((firstblk+1)<<RAM_SECTSHIFT, <=, size);

	dlp->d_magic = AM_DISK_MAGIC;
	dlp->d_chksum = checksum((uint16 *) dlp);
}

/* ARGSUSED */
errstat
rd_unit_init(devaddr, unit)
	long devaddr;
	int unit;
{
	register struct ram_parameters *ramp;
	errstat succes=STD_SYSERR;

	if (unit >= Num_ram_disks)
		return STD_SYSERR;
	ramp = ram_pars + unit;
	ram_info[unit].rami_base =
		    aalloc((vir_bytes) (ramp->ramp_nblocks<<RAM_SECTSHIFT), 0);
	if (ramp->ramp_init)
		succes= (*ramp->ramp_init)(ram_info[unit].rami_base,
			ramp->ramp_nblocks<<RAM_SECTSHIFT,
			ramp->ramp_charstarpar, ramp->ramp_longpar);
	if (succes != STD_OK)
		rd_label(ram_info[unit].rami_base,
			(int32) (ramp->ramp_nblocks<<RAM_SECTSHIFT),
			"Ram-disk", (long) 0x80,
			ramp->ramp_nvdisks, ramp->ramp_vdisksizes);
	return STD_OK;
}

/*ARGSUSED*/
errstat
rd_capacity(devaddr, unit, nblocks_ret, blocksize_ret)
	long devaddr; /*unused*/
	int unit;
	int32 *nblocks_ret;
	int32 *blocksize_ret;
{
	*blocksize_ret = RAM_SECTSIZE;
	*nblocks_ret   = ram_pars[unit].ramp_nblocks;
	return(STD_OK);
}

/*ARGSUSED*/
errstat
rd_read(devaddr, unit, block, nblocks, buf)
	long devaddr; /*unused*/
	int unit;
	disk_addr block, nblocks;
	char *buf;
{
	/* First some sanity checks */
	if (block + nblocks > ram_pars[unit].ramp_nblocks)
		return STD_SYSERR;
	(void)memcpy((_VOIDSTAR) buf,
	       (_VOIDSTAR) (ram_info[unit].rami_base + (block<<RAM_SECTSHIFT)),
	       (size_t) nblocks<<RAM_SECTSHIFT);
	return STD_OK;
}

/*ARGSUSED*/
errstat
rd_write(devaddr, unit, block, nblocks, buf)
	long devaddr; /*unused*/
	int unit;
	disk_addr block, nblocks;
	char *buf;
{
	/* First some sanity checks */
	if (block + nblocks > ram_pars[unit].ramp_nblocks)
		return STD_SYSERR;
	(void)memcpy((_VOIDSTAR) (ram_info[unit].rami_base +
				(block<<RAM_SECTSHIFT)),
		     (_VOIDSTAR) buf, (size_t) nblocks<<RAM_SECTSHIFT);
	return STD_OK;
}

