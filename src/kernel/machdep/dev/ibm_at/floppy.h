/*	@(#)floppy.h	1.5	96/02/27 13:51:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IBM_AT_FLOPPY_H__
#define __IBM_AT_FLOPPY_H__

/* Floppy manifest constants */
#define	FLP_NDISKS	2	/* maximum # of units per controller */
#define	FLP_IRQ		6	/* interrupt vector */
#define	FLP_BASEREG	0x3F2	/* base register */

/* Floppy basic constants */
#define	FLP_RETRIES	1000	/* retry limit */
#define	FLP_TIMEOUT	3000	/* timeout value (in msec) */
#define	FLP_SECTOR_SIZE	512	/* floppy's sector size */
#define	FLP_ERROR_RETRY	3	/* # of retries before giving up */
#define	FLP_MOTORTIME	120	/* motor on time */

/* Floppy controller registers */
#define	FLP_DOR		(FLP_BASEREG+0x00) /* digital output register */
#define	FLP_STATUS	(FLP_BASEREG+0x02) /* status register */
#define	FLP_DATA	(FLP_BASEREG+0x03) /* data register */
#define	FLP_FIXEDDISK	(FLP_BASEREG+0x04) /* fixed disk register */
#define	FLP_DIR		(FLP_BASEREG+0x05) /* digital input register */

/* Floppy controller command values */
#define	FLP_SPECIFY	0x03	/* specify command */
#define	FLP_READSTAT	0x04	/* read status register */
#define	FLP_SENSE	0x08	/* sense interrupt command */
#define	FLP_RECALIBRATE	0x07	/* reset drive to cylinder 0 */
#define	FLP_SEEK	0x0F	/* seek to given cylinder */
#define	FLP_READID	0x4A	/* read sector identity */
#define	FLP_FORMAT	0x4D	/* format command */
#define	FLP_WRITE	0xC5	/* write command */
#define	FLP_READ	0xE6	/* read command */

/* Floppy main status register values */
#define	FLP_SEEK0	0x01	/* device 0 seeking */
#define	FLP_SEEK1	0x02	/* device 1 seeking */
#define	FLP_CB		0x10	/* controller busy */
#define	FLP_NDM		0x20	/* non DMA mode */
#define	FLP_DIO		0x40	/* data input/output */
#define	FLP_RQM		0x80	/* request for master */

/* Floppy digital output register */
#define	FLP_MOTORMASK	0x30	/* motor control bits mask */
#define	FLP_MTRDRV1	0x20	/* drive 1 motor enable */
#define	FLP_MTRDRV0	0x10	/* drive 0 motor enable */
#define	FLP_ENABLEINTR	0x08	/* enable interrupts and DMA */
#define	FLP_RESET	0x04	/* controller function reset */
#define	FLP_DRVSELECT	0x01	/* drive select */

/* Floppy digital input register */
#define	FLP_CHANGED	0x80	/* diskette changed */
#define	FLP_WRITEGATE	0x40	/* write gate */
#define	FLP_HDSELECT3	0x20	/* head select 3 / reduced write current */
#define	FLP_HDSELECT2	0x10	/* head select 2 */
#define	FLP_HDSELECT1	0x08	/* head select 1 */
#define	FLP_HDSELECT0	0x04	/* head select 0 */
#define	FLP_DRVSELECT1	0x02	/* drive 1 select */
#define	FLP_DRVSELECT0	0x01	/* drive 0 select */

/* Floppy SR0 register */
#define	FLP_UNITMASK	0x03	/* unit select mask */
#define	FLP_HEADADDR	0x04	/* head address */
#define	FLP_NOTREADY	0x08	/* not ready */
#define	FLP_EQUIPMENT	0x10	/* equipment check */
#define	FLP_ENDSEEK	0x20	/* seek end */
#define	FLP_ICMASK	0xC0	/* interrupt code mask */
#define	FLP_NORMAL	0x00	/* normal termination */
#define	FLP_ABORT	0x40	/* abrupt termination of command */
#define	FLP_INVALID	0x80	/* invalid command issue */
#define	FLP_CHRDY	0xC0	/* ready changed during operation */

/* Floppy SR1 register */
#define	FLP_ENDCYL	0x80	/* end of cylinder */
#define	FLP_DATAERR	0x20	/* data error */
#define	FLP_OVERRUN	0x10	/* PIO overrun */
#define	FLP_NODATA	0x04	/* no data */
#define	FLP_NOWRITE	0x02	/* not writable */
#define	FLP_ADDRMASK	0x01	/* missing address mark */
#define	FLP_BADSECT	(FLP_NODATA|FLP_ADDRMASK)

/* Floppy SR2 register */
#define	FLP_BADCYL	0x1F	/* bad cylinder */

/* Floppy SR3 register */
#define	FLP_FAULT	0x80	/* fault signal */
#define	FLP_WRITEPROT	0x40	/* write protect signal */
#define	FLP_READY	0x20	/* ready signal */
#define	FLP_TRACK0	0x10	/* track 0 signal */
#define	FLP_TWOSIDE	0x08	/* two side signal */

/* Sector size parameters for I/O commands */
#define	FLP_SS128	0x0	/*  128 byte sectors */
#define	FLP_SS256	0x1	/*  256 byte sectors */
#define	FLP_SS512	0x2	/*  512 byte sectors */
#define	FLP_SS1024	0x3	/* 1024 byte sectors */

/* Max # results bytes a command can return */
#define	MAX_RESULTS	7

/*
 * Per drive data
 */
struct floppy {
    uint8	flp_cal;	/* states whether FDC is calibrated */
    uint8	flp_cyl;	/* current cylinder */
    short	flp_ntypes;	/* size of descriptor table */
    struct drivetype *flp_table;/* drive type table */
    struct drivetype *flp_dtype;/* current descriptor pointer */
};

/*
 * Drive type descriptor table
 */
struct drivetype {
    int16	dt_size;	/* total size in blocks */
    int16	dt_nsectrk;	/* # of sectors/track */
    int16	dt_nheads;	/* # of heads */
    int16	dt_ncyl;	/* # of cylinders */
    int8	dt_gap;		/* gap length */
    int8	dt_spec1;	/* step rate/head unload time */
    int8	dt_spec2;	/* step rate/head unload time */
    int8	dt_xfer;	/* transfer rate */
    int8	dt_spc;		/* step per cylinder */
    interval	dt_mtime;	/* motor time (in msec) */
    int8	dt_fmtgap;	/* gap used by fmt command */
};

/*
 * The following macros should, perhaps someday,
 * become part of a standard include file.
 */
#ifndef MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

/*
 * Prototypes for the floppy disk service functions.
 */


#include "_ARGS.h"
#include "disk/disk.h"


errstat flp_devinit _ARGS(( long devaddr, int unit ));
errstat flp_unitinit _ARGS(( long devaddr, int unit ));
int	flp_unittype _ARGS(( long devaddr, int unit ));
errstat flp_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat flp_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat flp_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat flp_control _ARGS(( long devaddr, int unit, int cmd, bufptr buf,
				bufsize size ));

#endif /* __IBM_AT_FLOPPY_H__ */
