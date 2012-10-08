/*	@(#)sun4_floppy.h	1.2	96/02/27 13:55:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SUN4_FLOPPY_H__
#define	__SUN4_FLOPPY_H__

/*
 * Data structures and constants used by sun4 floppy drivers.
 *
 * Author:  Gregory J. Sharp, June 1993
 */

/*
 * Data structures for keeping track of the state of the floppy drive
 */

typedef struct flop_geom {
    int	fg_size;	/* total size in blocks */
    int	fg_nsectrk;	/* # of sectors/track */
    int	fg_nheads;	/* # of heads */
    int	fg_ncyl;	/* # of cylinders */
    int	fg_sectsz;	/* size of sector in bytes */
    int	fg_xfer;	/* transfer rate */
    int	fg_spc;		/* # steps per cylinder */
    int	fg_sectpercyl;	/* # sectors per cylinder = fg_nsectrk * fg_nheads */
} flop_geom;


#define	MAX_BUF_SIZE	12   /* 10 is enough but this simplifies alignment */

/*
 * A struct for command, client's data and results to be passed around in.
 * The execution fields are only relevant for I/O from/to the disk.
 * The stubs simply fill in one of these blocks and then pass it to the
 * flop_exec routine which does the rest.
 */
typedef struct fl_crb {
    int		cmd_type;		/* noresult, immediate, etc */
    int		cmd_nbytes;		/* Number bytes in c_buf */
    uint8	cmd_buf[MAX_BUF_SIZE];	/* Command */
    int		exe_nbytes;		/* # bytes to be read/written */
    char *	exe_buf;		/* Pointer to place to read/write */
    int		res_nbytes;		/* Number bytes in r_buf */
    uint8	res_buf[MAX_BUF_SIZE];	/* Results */
} fl_crb;


/*
 * This is a struct for use by the command execution code to
 * keep track of what it is doing.
 */
typedef struct flop_unit {
    mutex	f_lock;			/* Stop multiple users */
    int		f_present;		/* 1 if drive is present */
    flop_geom *	f_geomp;		/* Drive geometry info */
    fl_crb *	f_crbp;			/* Current command/results for unit */
} flop_unit;


typedef struct fdc_info {
    int			fdc_state;	/* the type of last cmd sent to fdc */
#ifdef STATISTICS
    int			fdc_ints;	/* # interrupts */
    int			fdc_spurious;	/* # spurious interrupts */
    int			fdc_writes;	/* # write commands */
    int			fdc_reads;	/* # read commands */
    int			fdc_capacity;	/* # capacity commands */
    int			fdc_eject;	/* # eject commands */
    int			fdc_format;	/* # format commands */
#endif
} fdc_info;


/* States of controller - reflects what should come next */
#define	FST_RESULT	0	/* Awaiting result phase */
#define	FST_NORESULT	1	/* Cmd issued with no result or data phase */
#define	FST_DATA_XFER	2	/* Cmd issued, awaiting data phase */
#define	FST_IDLE	3	/* No activity pending */
#define	FST_IMMEDIATE	4	/* Get results without interrupt? */
#define	FST_RESETTING	5	/* Just issued a reset command */
#define	FST_SEEK	6	/* Seek or recalibrate command */

#define	CHECK		1
#define	NOCHECK		0

#define	FIFO_ON		0
#define	FIFO_OFF	1

/* Timeouts - waiting for results bytes mainly */
#define	DATAREG_WAIT		1000000

/* Read/write commands */
#define	FLOP_READ		0
#define	FLOP_WRITE		1

/* Extra error codes */
#define	FE_NODEV		1


/*
 * Prototypes for the floppy disk service functions.
 */


#include "disk/disk.h"


errstat flop_ctlr_init _ARGS(( long devaddr, int unit ));
errstat flop_unit_init _ARGS(( long devaddr, int unit ));
int	flop_unittype _ARGS(( long devaddr, int unit ));
errstat flop_capacity _ARGS(( long devaddr, int unit, int32 * lastblk,
				int32 * blksz ));
errstat flop_read _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat flop_write _ARGS(( long devaddr, int unit, disk_addr strt_blk,
				disk_addr blk_cnt, char * buf ));
errstat flop_control _ARGS(( long devaddr, int unit, int cmd, bufptr buf,
				bufsize size ));

#endif /* __SUN4_FLOPPY_H__ */
