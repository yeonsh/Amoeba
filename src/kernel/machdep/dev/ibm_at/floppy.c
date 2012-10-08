/*	@(#)floppy.c	1.9	96/02/27 13:51:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * floppy.c
 *
 * This file contains an implementation of a driver for a NEC PD765 floppy
 * disk controller, a controller often found in machines with a standard
 * AT bus architecture (AT 286/386). This driver is capable of driving two
 * floppy disks, although operations to either one can not be mixed. The
 * driver uses a very gross granularity that decreases the concurrency.
 * By disallowing most of the concurrency we solve a lot of, the otherwise
 * annoying, race conditions. Since this driver is mainly used to dump and
 * restore data this behaviour isn't very disturbing.
 *
 * Author: Leendert van Doorn
 * Modified: Gregory J. Sharp - Jan 94 added flp_control and formatting command
 *				and restructured the code slightly to cope.
 */

#include <stdlib.h>
#include <amoeba.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stderr.h>
#include <string.h>
#include <assert.h>
INIT_ASSERT
#include <disk/disk.h>
#include <disk/conf.h>
#include <bool.h>
#include <sys/proto.h>
#include <module/mutex.h>
#include "i386_proto.h"
#include "sys/proto.h"

#include "floppy.h"
#include "cmos.h"
#include "dma.h"

#ifndef FLP_DEBUG
#define	FLP_DEBUG	0
#endif

/*
 * This driver support the following combination of diskettes and drives:
 *
 * Drive   diskette    sectors    tracks  Comment
 * 360K	    360K	9	   40	  Standard PC diskette
 * 1.2M	    1.2M	15	   80	  Standard AT diskette
 * 1.2M     360K	9	   40	  Standard PC diskette in AT drive
 * 1.2M	    720K	9	   80	  Toshiba in AT drive
 * 720K	    720K	9	   80	  Toshiba et al.
 * 1.4M	    1.4M	18	   80	  Standard PS/2 diskette
 * 1.4M	    720M	9	   80	  PS/2 et al.
 */
static struct drivetype drivetype1[] = {	/* type 1 (360 Kb drive) */
    /* size sec hd cyl  gap  spc1  spc2 xfer stp mtime fmtgap */
    {  720,  9, 2, 40, 0x2A, 0xDF, 0x02, 2,   1, 500,  0x50 }	/* 360 Kb */
};
static struct drivetype drivetype2[] = {	/* type 2 (1.2 Mb drive) */
    /* size sec hd cyl  gap  spc1  spc2 xfer stp mtime fmtgap */
    { 2400, 15, 2, 80, 0x1B, 0xDF, 0x02, 0,   1, 500,  0x54 },	/* 1.2 Mb */
    {  720, 9,  2, 40, 0x23, 0xDF, 0x02, 1,   2, 500,  0x50 },	/* 360 Kb */
    { 1440, 9,  2, 80, 0x23, 0xDF, 0x02, 1,   1, 500,  0x50 }	/* 720 Kb */
};
static struct drivetype drivetype3[] = {	/* type 3 (720 Kb drive) */
    /* size sec hd cyl  gap  spc1  spc2 xfer stp mtime fmtgap */
    { 1440,  9, 2, 80, 0x2A, 0xDF, 0x02, 2,   1, 1000, 0x50 }	/* 720 Kb */
};
static struct drivetype drivetype4[] = {	/* type 4 (1.4 Mb drive) */
    /* size sec hd cyl  gap  spc1  spc2 xfer stp mtime fmtgap */
    { 2880, 18, 2, 80, 0x1B, 0xAF, 0x02, 0,   1, 1000, 0x54 },	/* 1.4 Mb */
    { 1440,  9, 2, 80, 0x2A, 0xDF, 0x02, 2,   1, 1000, 0x50 }	/* 720 Kb */
};

/* floppy disk information */
int flp_ndisks;			  /* # of floppy disks attached */
struct floppy floppy[FLP_NDISKS]; /* global floppy disk data */

#ifndef NDEBUG
static int flp_debug;		 /* current debug level */
#endif

/* timer to trigger watch dog event */
int motortime;			  /* time motor is on */

/* motor status variable */
static int motorbits;		  /* which drive motor is on */

/* prevent race conditions, general failure flag */
static mutex rdwr_lock;		  /* don't allow concurrent operations */
static int needreset;		  /* need to reset controller */


static unsigned char fl_results[MAX_RESULTS];


/*
 * Read all result values from the controller and put them in fl_results.
 */
static errstat
flp_results()
{
    int retries;
    int nbytes;
    int status;

    nbytes = 0;
    retries = FLP_RETRIES;
    do {
	status = in_byte(FLP_STATUS) & (FLP_RQM|FLP_DIO|FLP_CB);
	if (status == (FLP_RQM|FLP_DIO|FLP_CB)) {
	    if (nbytes >= MAX_RESULTS) {
		/* More than max legal # results bytes */
		needreset = 1;
		break;
	    }
	    fl_results[nbytes++] = in_byte(FLP_DATA);
	    retries = FLP_RETRIES;
	}
	if (status == FLP_RQM) /* No more bytes to come */
	    return STD_OK;
    } while (--retries != 0);
    return STD_SYSERR;
}


/*
 * Send a command to the controller. You can only write to the
 * controller when it is listening. If it won't respond after
 * a limited number of retries we hit it on the head.
 */
static void
flp_command(cmd)
    int cmd;
{
    register int retry;

    for (retry = 0; retry < FLP_RETRIES; retry++) {
	int status = in_byte(FLP_STATUS);
	if ((status & (FLP_RQM|FLP_DIO)) == FLP_RQM) {
	    out_byte(FLP_DATA, cmd);
	    return;
	}
    }
    needreset = TRUE;
}


/*
 * If the motor is off we start it
 */
static void
flp_motoron(unit)
    int unit;
{
    int running;

#ifndef NDEBUG
    if (flp_debug > 2)
	printf("flp_motoron: unit %d\n", unit);
#endif
    disable();
    motortime = 0; /* stop watch dog timer */
    running = motorbits & (1 << (unit + 4));

    motorbits |= (1 << (unit + 4));
    out_byte(FLP_DOR, motorbits | FLP_ENABLEINTR | FLP_RESET | unit);
    enable();

    /*
     * In case the motor is just started,
     * wait for it's head to settle 
     */
    if (!running) pit_delay((int) floppy[unit].flp_dtype->dt_mtime);
}


/*
 * Hit the controller as hard as we can, and hope that
 * it will be more reasonable next time.
 */
static void
flp_reset()
{
    int i;

#ifndef NDEBUG
    if (flp_debug > 1)
	printf("flp_reset: reseting controller\n");
#endif

    /*
     * Reset the controler.
     * Don't ask, this is (mostly) magic
     */
    disable();
    out_byte(FLP_DOR, 0);
    out_byte(FLP_DOR, FLP_ENABLEINTR|FLP_RESET);
    motortime = motorbits = 0;
    enable();

    /* wait for its acknowledgement */
    (void) await((event) floppy, (interval) FLP_TIMEOUT);

    /* flush controller's status information - ignore the results */
    flp_command(FLP_SENSE);
    (void) flp_results();

    for (i = 0; i < flp_ndisks; i++)
	floppy[i].flp_cal = FALSE;
    needreset = 0;
}


/*
 * Seek to the correct cylinder by performing the
 * required number of step-ins or -outs.
 */
static errstat
flp_seek(unit, cyl, head)
    int unit;
    int cyl;
    int head;
{
    int nsteps;

#ifndef NDEBUG
    if (flp_debug > 2)
	printf("flp_seek: unit %d: cyl %d, head %d\n", unit, cyl, head);
#endif
    nsteps = cyl * floppy[unit].flp_dtype->dt_spc;

    /* seek to correct cylinder */
    flp_command(FLP_SEEK);
    flp_command((head << 2) + unit);
    flp_command(nsteps);
    if (needreset) return STD_SYSERR;

    /* wait for command to be processed */
    (void) await((event) floppy, (interval) FLP_TIMEOUT);

    /* probe controller for status */
    flp_command(FLP_SENSE);
    if (flp_results() != STD_OK || fl_results[1] != nsteps ||
	(fl_results[0] & ~(FLP_UNITMASK | FLP_HEADADDR)) != FLP_ENDSEEK)
    {
	return STD_SYSERR;
    }
    floppy[unit].flp_cyl = cyl;
    return STD_OK;
}


/*
 * After any catastrophe we will have to recalibrate
 * the controllers notion of its arm position.
 */
static errstat
flp_recalibrate(unit)
    int unit;
{
    flp_command(FLP_RECALIBRATE);
    flp_command(unit);
    if (needreset)
	return STD_SYSERR;

    /* wait for command to be processed */
    (void) await((event) floppy, (interval) FLP_TIMEOUT);

    /* probe controller for status */
    flp_command(FLP_SENSE);

    if (flp_results() != STD_OK)
	return STD_SYSERR;

#ifndef NDEBUG
    if (flp_debug > 2)
	printf("flp_recalibrate: unit %d, sr0 %x, pcn %x\n",
					unit, fl_results[0], fl_results[1]);
#endif
    if ((fl_results[0] & ~(FLP_UNITMASK|FLP_HEADADDR)) != FLP_ENDSEEK ||
							fl_results[1] != 0)
	return STD_SYSERR;

    /* wait for head to settle down */
    pit_delay((int) floppy[unit].flp_dtype->dt_mtime);
    floppy[unit].flp_cal = TRUE;
    floppy[unit].flp_cyl = -1; /* force seek */
    return STD_OK;
}


static errstat
fmt_track(unit, cyl, head, dtp, fmtbuf, bufsz)
    int			unit;
    int			cyl;
    int			head;
    struct drivetype *	dtp;
    char *		fmtbuf;
    int			bufsz;
{
    int		i;
    char *	p;

    assert(bufsz == dtp->dt_nsectrk * 4);
    assert(floppy[unit].flp_cyl == cyl);

    p = fmtbuf;
    /* Fill in the track data for the formatting */
    for (i = 1; i <= dtp->dt_nsectrk; i++) {
	*p++ = (char) cyl;
	*p++ = (char) head;
	*p++ = (char) i;		/* Sector number */
	*p++ = FLP_SS512;		/* 512 byte sectors */
    }

    /*
     * Setup the DMA chip.  Check it doesn't cross a 64K boundary.
     */
    assert(((((int) fmtbuf + FLP_SECTOR_SIZE - 1) >> 16) & 0xFF) ==
						(((int) fmtbuf >> 16) & 0xFF));
    if (dma_setup((DMA_SINGLE | DMA_READ), DMA_FLOPPY, fmtbuf, bufsz))
	return STD_SYSERR;
    /*
     * Feed the command to the controller
     */
    dma_start(DMA_FLOPPY);
    flp_command(FLP_FORMAT);
    flp_command(head << 2 | unit);
    flp_command(FLP_SS512);
    flp_command(dtp->dt_nsectrk & 0xFF);
    flp_command(dtp->dt_fmtgap);
    flp_command(0xAA);		/* Pick a value, any value :-) */
    if (needreset) {
	dma_done(DMA_FLOPPY);
	return STD_SYSERR;
    }
    
    /* wait for command to be processed */
    (void) await((event) floppy, (interval) FLP_TIMEOUT);

    /* disable DMA's floppy channel */
    dma_done(DMA_FLOPPY);

    if (flp_results() != STD_OK)
	return STD_SYSERR;
    if (fl_results[1] & FLP_NOWRITE)
	return STD_WRITEPROT;
    if ((fl_results[0] & FLP_ICMASK) != FLP_NORMAL)
	return STD_SYSERR;

    return STD_OK;
}


/*
 * Perform a basic I/O operation, including seeking
 * to the correct offset, setting up DMA and eventually
 * checking the error conditions.
 */
static errstat
flp_io(unit, dtp, cyl, sector, head, ptr, opcode)
    int unit;
    struct drivetype *dtp;
    int cyl;
    int head;
    char *ptr;
    int opcode;
{
    int sr0, sr1, sr2; /* floppy result registers */

    /* seek to correct position, if necessary */
    if (floppy[unit].flp_cyl != cyl && flp_seek(unit, cyl, head) != STD_OK) {
	printf("flp_io: unit %d: seek to cylinder %d, head %d failed\n",
	    unit, cyl, head);
	return STD_SYSERR;
    }

    /*
     * Setup the DMA chip.
     * The read/write operation depends on the DMA chip's viewpoint !
     */
    assert(((((int)ptr+FLP_SECTOR_SIZE-1)>>16)&0xFF)==(((int)ptr>>16)&0xFF));
    if (dma_setup(DMA_SINGLE | (opcode == FLP_READ ? DMA_WRITE : DMA_READ),
      DMA_FLOPPY, ptr, FLP_SECTOR_SIZE))
	return STD_SYSERR;

    /*
     * Issue a command to the NEC controller by sending nine
     * bytes to the chip. The motor must be running when we
     * attempt to transfer data.
     */
    dma_start(DMA_FLOPPY);
    flp_command(opcode);
    flp_command((head << 2) | unit);
    flp_command((int)cyl & 0xFF);
    flp_command(head);
    flp_command(sector);
    flp_command(FLP_SS512); /* sector size */
    flp_command(dtp->dt_nsectrk);
    flp_command(dtp->dt_gap);
    flp_command(0xFF); /* dtl is maximum */
    if (needreset) {
	dma_done(DMA_FLOPPY);
	return STD_SYSERR;
    }
    
    /* wait for command to be processed */
    if (await((event) floppy, (interval) FLP_TIMEOUT) != 0) {
	/* There is almost certainly no floppy in the drive */
	flp_reset();
	dma_done(DMA_FLOPPY);
	return STD_NOMEDIUM;
    }

    /* disable DMA's floppy channel */
    dma_done(DMA_FLOPPY);

    if (flp_results() != STD_OK)
	return STD_SYSERR;

    sr0 = fl_results[0];
    sr1 = fl_results[1];
    sr2 = fl_results[2];

    /* check for errors */
    if (opcode == FLP_WRITE && (sr1 & FLP_NOWRITE)) {
	printf("Floppy unit %d: diskette is write protected\n", unit);
	return STD_WRITEPROT;
    }
    if (sr1 & FLP_BADSECT || sr2 & FLP_BADCYL) {
	floppy[unit].flp_cal = FALSE;
	return STD_SYSERR;
    }
    if ((sr0 & FLP_ICMASK) != FLP_NORMAL)
	return STD_SYSERR;
    if (sr1 | sr2) {
	floppy[unit].flp_cal = FALSE;
	return STD_SYSERR;
    }
    return STD_OK;
}


/*
 * For now we only format using the default density for the drive.
 * At some point in the future an extra parameter could be added to
 * specify an alternate density.
 */
static errstat
flp_format(unit)
    int	unit;		/* drive to use */
{
    struct drivetype * dtp;
    errstat	err;
    int		cyl;
    int		hd;
    char *	fmtbuf;
    size_t	bufsz;
    char	buffer[FLP_SECTOR_SIZE];

    mu_lock(&rdwr_lock);

    /* Use default parameters */
    dtp = floppy[unit].flp_table;

    /* Allocate a buffer for the track information for the format cmd */
    bufsz = dtp->dt_nsectrk * 4;
    if ((fmtbuf = (char *) malloc(bufsz)) == 0)
	err = STD_NOMEM;
    else {
	flp_motoron(unit);

	/* Make sure we are calibrated - we've probably got a new floppy */
	if (needreset)
	    flp_reset();
	if (floppy[unit].flp_cal == FALSE)
	    err = flp_recalibrate(unit);
	else
	    err = STD_OK;
	
	if (err == STD_OK) {
	    /* See if there is a floppy in the drive */
	    flp_command(FLP_SPECIFY);
	    flp_command(dtp->dt_spec1);
	    flp_command(dtp->dt_spec2);
	    out_byte(FLP_DIR, dtp->dt_xfer);

	    err = flp_io(unit, dtp, 0, 1, 0, buffer, FLP_READ);
	    if (err != STD_NOMEDIUM)
		err = STD_OK;
	}

	for (cyl = 0; err == STD_OK && cyl < dtp->dt_ncyl; cyl++) {
	    err = flp_seek(unit, cyl, 0);
	    for (hd = 0; err == STD_OK && hd < dtp->dt_nheads; hd++)
		err = fmt_track(unit, cyl, hd, dtp, fmtbuf, (int) bufsz);
	}

	free((_VOIDSTAR) fmtbuf);

	/* make sure the motor turns off eventually */
	motortime = FLP_MOTORTIME;
    }
    mu_unlock(&rdwr_lock);
    return err;
}


/*
 * Recalibrate the drive and try to determine the density
 * of the floppy. Exceptions like disk not available, or
 * unformated floppies are handled here.
 */
static errstat
flp_autodensity(unit)
    int unit;
{
    int i, retry;
    errstat err;

#ifndef NDEBUG
    if (flp_debug > 1)
	printf("flp_autodensity: unit %d\n", unit);
#endif

    for (i = 0; i < floppy[unit].flp_ntypes; i++) {
	register struct drivetype *dtp;
	char buffer[FLP_SECTOR_SIZE];

	/*
	 * To determine the density of the floppy in the drive
	 * we first try to read track 0, sector 0 followed
	 * by an attempt to read the last sector on track 0.
	 */
	dtp = &floppy[unit].flp_table[i];
	for (retry = 0; retry < FLP_ERROR_RETRY; retry ++) {
	    /*
	     * Specify stepping rate, head unload/load time,
	     * and non-data mode, and data rate.
	     */
	    flp_command(FLP_SPECIFY);
	    flp_command(dtp->dt_spec1);
	    flp_command(dtp->dt_spec2);
	    out_byte(FLP_DIR, dtp->dt_xfer);

	    err = flp_io(unit, dtp, 0, 1, 0, buffer, FLP_READ);
	    if (err == STD_NOMEDIUM)
		return err;
	    if (err != STD_OK)
		continue;

	    err = flp_io(unit, dtp, 0, dtp->dt_nsectrk, 0, buffer, FLP_READ);
	    if (err == STD_NOMEDIUM)
		return err;
	    if (err != STD_OK)
		continue;
#ifndef NDEBUG
	    if (flp_debug > 1)
		printf("flp_autodensity: unit %d: density %d blocks\n",
		    unit, dtp->dt_size);
#endif
	    floppy[unit].flp_cal = TRUE;
	    floppy[unit].flp_dtype = dtp;
	    return STD_OK;
	}
    }
    return STD_SYSERR;
}


/*
 * Read or write a block to the floppy.
 */
static errstat
flp_rdwr(unit, strtblk, base, opcode)
    int unit;			/* unit to use */
    disk_addr strtblk;		/* starting block */
    char *base;			/* place to read from/write to */
    int opcode;
{
    struct drivetype *dtp;	/* drive type */
    int cyl;			/* physical cylinder on disk */
    int head;			/* physical head on disk */
    errstat err;		/* error status */
    int retry;			/* retry counter */
    char *ptr = base;		/* place to read from/write to */
    static char tmpbuf[FLP_SECTOR_SIZE]; /* in case of 64Kb boundaries */

    /*
     * Due to some bizare design flaw (see dma.c) we have to remap the buffer
     * if it crosses a physical 64Kb boundary. Another awkward feature is that
     * DMA cannot be done from area above the 16 Mb limit.
     */
    if ((uint32) ptr >= 0x1000000 ||
	((((int)ptr+FLP_SECTOR_SIZE-1)>>16)&0xFF) != (((int)ptr>>16)&0xFF)) {
	ptr = tmpbuf;
#ifndef NDEBUG
	if (flp_debug > 2)
	    printf("flp_rdwr: remapping buffer (0x%x to 0x%x)\n", base, ptr);
#endif
	if (opcode == FLP_WRITE)
	    (void) memmove((_VOIDSTAR) ptr, (_VOIDSTAR) base, FLP_SECTOR_SIZE);
    }
    assert((uint32) ptr < 0x1000000);

    /*
     * The user may have changed disks in between two disk I/O operations.
     * If this happens and the second disk has a different density than the
     * previous disk the first I/O attempt will fail. This isn't really a
     * problem since a retry wil recalibrate the disk and succeed.
     */
    err = STD_SYSERR;
    mu_lock(&rdwr_lock);
    for (retry = 0; retry < FLP_ERROR_RETRY; retry++) {
	/* reset and recalibrate controller */
	if (needreset) flp_reset();
	if (floppy[unit].flp_cal == FALSE) {
	    if (flp_recalibrate(unit) != STD_OK)
		continue;
	    err = flp_autodensity(unit);
	    if (err == STD_NOMEDIUM)
		break;
	    if (err != STD_OK)
		continue;
	}

	/* compute the physical sector/cyl/head */
	dtp = floppy[unit].flp_dtype;
	cyl = strtblk / (dtp->dt_nheads * dtp->dt_nsectrk);
	head = (strtblk % (dtp->dt_nheads * dtp->dt_nsectrk)) / dtp->dt_nsectrk;

	/* check the disk bounds */
	if (strtblk >= dtp->dt_size) {
	    mu_unlock(&rdwr_lock);
	    return STD_ARGBAD;
	}

#ifndef NDEBUG
	if (flp_debug > 2) {
	    if (opcode == FLP_READ)
		printf("flp_rdwr: reading");
	    else
		printf("flp_rdwr: writing");
	    printf(" unit %d, blk %ld, cyl %d, head %d, sec %d\n",
		unit, strtblk, cyl, head, (strtblk % dtp->dt_nsectrk) + 1);
	}
#endif
	err = flp_io(unit, dtp, cyl,
	    (int) (strtblk % dtp->dt_nsectrk) + 1, head, ptr,opcode);
	if (err == STD_OK) {
	    mu_unlock(&rdwr_lock);
	    if (base != ptr && opcode == FLP_READ) {
		(void) memmove((_VOIDSTAR) base, (_VOIDSTAR) ptr,
							       FLP_SECTOR_SIZE);
	    }
	    return STD_OK;
	}
    }
    mu_unlock(&rdwr_lock);
    return err;
}


/*
 * Floppy driver interrupt
 */
/* ARGSUSED */
static void
flp_intr(irq)
    int irq;
{
    enqueue((void (*) _ARGS((long))) wakeup, /*(event)*/ (long) floppy);
}


/****************************************/
/*	External Interface		*/
/****************************************/


/*
 * Turn off the motor - called from pit interrupt routine.
 */
void
flp_motoroff()
{
    out_byte(FLP_DOR, FLP_ENABLEINTR | FLP_RESET);
    motorbits = 0;
}


/*
 * Determine number of floppies and their type by examining
 * the diskette disk type byte in CMOS ram. Note that the
 * initialization process differs from that of the winchester
 * driver since the floppy driver may be called (floppy_loader)
 * before the vdisk thread is running.
 */
void
flp_startup()
{
    int ddt, unit, type;

    mu_init(&rdwr_lock);

#ifndef NDEBUG
    if ((flp_debug = kernel_option("flp")) == 0)
	flp_debug = FLP_DEBUG;
#endif

    setirq(FLP_IRQ, flp_intr);
    pic_enable(FLP_IRQ);
    ddt = cmos_read(CMOS_DDTB);
    for (unit=0, type=(ddt>>4)&0x0F; unit < FLP_NDISKS; unit++, type=ddt&0x0F) {
	if (type == CMOS_NOFLP) continue;
	switch (type) {
	case CMOS_T40S9:
	    printf("Floppy %d: 40 cylinders, 2 heads, 9 sectors (360 KB)\n",
		unit);
	    floppy[unit].flp_table = drivetype1;
	    floppy[unit].flp_ntypes =
		sizeof(drivetype1) / sizeof(struct drivetype);
	    break;
	case CMOS_T80S15:
	    printf("Floppy %d: 80 cylinders, 2 heads, 15 sectors (1.2 MB)\n",
		unit);
	    floppy[unit].flp_table = drivetype2;
	    floppy[unit].flp_ntypes =
		sizeof(drivetype2) / sizeof(struct drivetype);
	    break;
	case CMOS_T80S9:
	    printf("Floppy %d: 80 cylinders, 2 heads, 9 sectors (720 KB)\n",
		unit);
	    floppy[unit].flp_table = drivetype3;
	    floppy[unit].flp_ntypes =
		sizeof(drivetype3) / sizeof(struct drivetype);
	    break;
	default:
	    printf("flp_devinit: unit %d: unknown type %x\n", unit, type);
	    printf("flp_devinit: WARNING: assuming it is a 1.44MB floppy\n");
	    /* Fall through */
	case CMOS_T80S18:
	    printf("Floppy %d: 80 cylinders, 2 heads, 18 sectors (1.44 MB)\n",
		unit);
	    floppy[unit].flp_table = drivetype4;
	    floppy[unit].flp_ntypes =
		sizeof(drivetype4) / sizeof(struct drivetype);
	    break;
	}
	floppy[unit].flp_dtype = floppy[unit].flp_table;
	flp_ndisks++;
    }
}


/*
 * Initialize the floppy disk controller
 */
/* ARGSUSED */
errstat
flp_devinit(devaddr, irq)
    long devaddr;		/* don't care */
    int irq;			/* interrupt request level */
{
    mu_lock(&rdwr_lock);
    flp_reset();
    mu_unlock(&rdwr_lock);
    return STD_OK;
}


/*
 * Initialize an individual unit. Since all drives are already
 * initialized (well, sort of) there isn't much we can do here.
 * The real work is done when we perform an I/O operation, at
 * that time the density is determined.
 */
/* ARGSUSED */
errstat
flp_unitinit(devaddr, unit)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
{
    /* some standard sanity checks */
    if (unit < 0 || unit >= flp_ndisks)
	return STD_SYSERR;
    return STD_OK;
}


/*
 * Since all floppy disks are removable,
 * it isn't very hard to figure out its
 * unit type.
 */
/* ARGSUSED */
int
flp_unittype(devaddr, unit)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
{
    return UNITTYPE_REMOVABLE;
}


/*
 * Read block from floppy
 */
/* ARGSUSED */
errstat
flp_read(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    disk_addr strt_blk;		/* first block to read */
    disk_addr blk_cnt;		/* total number of blocks to be read */
    char *buf_ptr;		/* destination for the read data */
{
    errstat err;		/* error status of the flp_rdwr command */

    /* some standard sanity checks */
    if (unit < 0 || unit >= flp_ndisks || strt_blk < 0 || blk_cnt < 0)
	return STD_ARGBAD;

    /* turn on floppy motor */
    flp_motoron(unit);

    /* handle big reads - reading one block at the time */
    do {
	err = flp_rdwr(unit, strt_blk, buf_ptr, FLP_READ);
	buf_ptr += FLP_SECTOR_SIZE;
	strt_blk++;
    } while (err == STD_OK && --blk_cnt > 0);

    /* make sure the motor turns off eventually */
    motortime = FLP_MOTORTIME;
    return err;
}


/*
 * Write block to floppy
 */
/* ARGSUSED */
errstat
flp_write(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    disk_addr strt_blk;		/* first block to write */
    disk_addr blk_cnt;		/* total number of blocks to be written */
    char *buf_ptr;		/* source for the write */
{
    errstat err;		/* error status of the flp_rdwr command */

    /* some standard sanity checks */
    if (unit < 0 || unit >= flp_ndisks || strt_blk < 0 || blk_cnt < 0)
	return STD_ARGBAD;

    /* turn on floppy motor */
    flp_motoron(unit);

    /* handle big writes - writing one block at the time */
    do {
	err = flp_rdwr(unit, strt_blk, buf_ptr, FLP_WRITE);
	buf_ptr += FLP_SECTOR_SIZE;
	strt_blk++;
    } while (err == STD_OK && --blk_cnt > 0);

    /* make sure the motor turns off eventually */
    motortime = FLP_MOTORTIME;
    return err;
}


/*
 * Get capacity of the specified unit. We return the actual capacity
 * of the unit and not of the physical disk (you can insert a 360 Kb
 * diskette into a 1.2 Mb disk drive).
 */
/* ARGSUSED */
errstat
flp_capacity(devaddr, unit, lastblk, blksz)
    long devaddr;		/* don't care */
    int unit;			/* unit to use */
    int32 *lastblk;		/* out: pointer to # blocks on device */
    int32 *blksz;		/* out: pointer to size of logical block */
{
    if (unit < 0 || unit >= flp_ndisks)
	return STD_ARGBAD;
    *blksz = (int32) FLP_SECTOR_SIZE;
    *lastblk = (int32) floppy[unit].flp_table->dt_size;
    return STD_OK;
}


/*
 * Perform activities such as formatting floppies.  Most floppies of this
 * type have manual eject so we can ignore the eject command for now.
 */
/*ARGSUSED*/
#ifdef __STDC__
errstat flp_control(long devaddr, int unit, int	cmd, bufptr buf, bufsize size)
#else
errstat
flp_control(devaddr, unit, cmd, buf, size)
    long	devaddr;	/* unused */
    int		unit;		/* drive to use */
    int		cmd;		/* which control command to execute */
    bufptr	buf;		/* possible data for command */
    bufsize	size;		/* #bytes in buf */
#endif
{
    errstat err;

    if (unit < 0 || unit >= flp_ndisks)
	return STD_ARGBAD;

    switch (cmd) {
    case DSC_FORMAT:
	err = flp_format(unit);
	break;

    default:
	printf("flp_control: unsupported command 0x%x\n", cmd);
	err = STD_COMBAD;
	break;
    }
    return err;
}
