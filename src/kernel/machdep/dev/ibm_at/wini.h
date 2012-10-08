/*	@(#)wini.h	1.9	96/02/27 13:52:37 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IBM_AT_WINI_H__
#define	__IBM_AT_WINI_H__

/* Winchester manifest constants */
#define	WIN_NDISKS	2	/* # disks per controller */
#define	WIN_IRQ		14	/* interrupt vector */
#define	WIN_BASEREG	0x1F0	/* base register */

/* Winchester basic constants */
#define	WIN_TIMEOUT	1000	/* arbitrary timeout limit (in msecs) */
#define WIN_RETRY	100000	/* arbitrary retry limit */
#define WIN_READY_RETRY	1000	/* arbitrary ready retry limit */
#define WIN_ERROR_RETRY	3	/* arbitrary error retry limit */
#define	WIN_SECTOR_SIZE	512	/* winchester's physical block size */
#define	WIN_MAX_SECTORS	256	/* maximum size of multisector I/O */

/* Winchester controller registers */
#define	WIN_DATA	(WIN_BASEREG+0)	/* read/write data */
#define	WIN_ERROR	(WIN_BASEREG+1)	/* error register */
#define	WIN_PRECOMP	(WIN_BASEREG+1)	/* precomp cylinder */
#define	WIN_COUNT	(WIN_BASEREG+2)	/* sector count */
#define	WIN_SECTOR	(WIN_BASEREG+3)	/* sector number */
#define	WIN_CYLLSB	(WIN_BASEREG+4)	/* cylinder LSB */
#define	WIN_CYLMSB	(WIN_BASEREG+5)	/* cylinder MSB */
#define	WIN_SDH		(WIN_BASEREG+6)	/* size/drive/head */
#define	WIN_STATUS	(WIN_BASEREG+7)	/* status register */
#define	WIN_CMD		(WIN_BASEREG+7)	/* command register */
#define	WIN_HF		(WIN_BASEREG+518) /* HF register */

/* Winchester disk controller commands */
#define	WIN_RESTORE	0x10	/* restore (recalibrate) command */
#define	WIN_SEEK	0x70	/* seek command */
#define	WIN_READ	0x20	/* read command */
#define	WIN_WRITE	0x30	/* write command */
#define	WIN_FORMAT	0x50	/* format a track */
#define	WIN_SETPARAM	0x91	/* set parameters command */
#define	ATA_IDENTIFY	0xEC	/* ATA identify drive */

/* Winchester status bits */
#define	WIN_BUSY	0x80	/* controller busy */
#define	WIN_READY	0x40	/* drive ready */
#define WIN_WRFLT	0x20	/* write fault */
#define	WIN_DONE	0x10	/* seek done */
#define	WIN_DRQ		0x08	/* data request */
#define WIN_ERR		0x01	/* error - read error register */
#define	WIN_IMASK	(WIN_READY|WIN_WRFLT|WIN_DONE|WIN_ERR)

/* Winchester disk errors */
#define	WIN_ADDRMARK	0x01	/* missing data address mark */
#define	WIN_TRACK0	0x02	/* track 0 error */
#define	WIN_ABORT	0x04	/* command aborted */
#define	WIN_IDNF	0x10	/* ID not found */
#define	WIN_ECC		0x40	/* data ECC error */
#define	WIN_BADBLK	0x80	/* bad block detected */

/* Winchester control byte */
#define	WIN_HEAD8	0x04	/* disk has more than eight heads */
#define	WIN_DEFMAP	0x20	/* manufacturer's defect map on max cyl+1 */
#define	WIN_DISECC	0x40	/* disable ECC retries */
#define	WIN_DISACC	0x80	/* disable access retries */

/*
 * This structure describes the disk geometry.
 */
struct wini {
    uint8	   w_lnheads;	/* logical # of heads */
    uint8	   w_lnsectrk;	/* logical # of sectors / track */
    uint32	   w_lncyl;	/* logical # of cylinders */
    uint8	   w_pnheads;	/* physical # of heads */
    uint8	   w_pnsectrk;	/* physical # of sectors / track */
    uint32	   w_pncyl;	/* physical # of cylinders */
    uint8	   w_precomp;	/* write precompensation register */
    uint8	   w_ctrl;	/* control byte */
    disk_addr	   w_size;	/* disk size in blocks */
};

/*
 * The following macros should, perhaps someday,
 * become part of a standard include file.
 */
#ifndef MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

/*
 * Prototypes for the winchester disk service functions.
 */


#include "_ARGS.h"
#include "disk/disk.h"
#include "vdisk/disklabel.h"


errstat win_devinit _ARGS(( long devaddr, int unit ));
errstat win_unitinit _ARGS(( long devaddr, int unit ));
int	win_unittype _ARGS(( long devaddr, int unit ));
errstat win_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat win_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat win_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat win_control _ARGS(( long devaddr, int unit, int cmd, bufptr buf,
				bufsize size ));
errstat win_geometry _ARGS(( long devaddr, int unit, geometry * geom ));

#endif /* __IBM_AT_WINI_H__ */
