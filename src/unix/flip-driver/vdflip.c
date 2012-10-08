/*	@(#)vdflip.c	1.4	94/04/07 14:03:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/uio.h>
#if defined(sun4c) || defined(sun4m)
#include <sun/openprom.h>
#else
#include <sundev/mbvar.h>
#endif
#include <sun/vddrv.h>

extern int flopen(),flclose(),flioctl();
extern int nodev();

static
struct cdevsw flcdev = {
	flopen,         flclose,        nodev,          nodev,
	flioctl,        nodev,          nodev,          0,
	0,              0,
};

#if defined(sun4c) || defined(sun4m)
static
struct vdldrv vd = {
	VDMAGIC_PSEUDO,
	"fldrv",
	NULL,
	NULL,
	&flcdev,
	0,
	0
};
#else
static
struct vdldrv vd = {
	VDMAGIC_PSEUDO,
	"fldrv",
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	&flcdev,
	0,
	0
};
#endif

/*ARGSUSED*/
vdflipinit(function_code, vdp, vdi, vds)
unsigned int function_code;
struct vddrv *vdp;
addr_t vdi;
struct vdstat *vds;
{

	switch (function_code) {
	case VDLOAD:
		printf("vdload\n");
		vdp->vdd_vdtab = (struct vdlinkage *) &vd;
		return 0;
	case VDUNLOAD:
		printf("vdunload\n");
		return -1;
	case VDSTAT:
		return 0;
	}
	return -1;
}
