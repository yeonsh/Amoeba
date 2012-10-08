/*	@(#)82072.c	1.4	96/02/27 13:53:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Sparcstation (sun4c) floppy disk driver
 *
 *	The sparcstations have an Intel 82072 floppy controller with no
 *	DMA controller.  Lots of things happen at low-level interrupt
 *	level as a consequence.
 *	Sparcstations usually have a Sony MP-F17W-50D drive attached to the
 *	controller.  In principle you can hang up to four drives on the
 *	controller.  In fact the support hardware means that you won't
 *	have more than 1 physical drive.  These beasties use MFM encoding.
 *
 *	Two densities are supported: 720K and 1440K.
 *
 * Author: Gregory J. Sharp, June 1993
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "machdep.h"
#include "debug.h"
#include "string.h"
#include "stdlib.h"
#include "disk/disk.h"
#include "disk/conf.h"
#include "memaccess.h"
#include "82072.h"
#include "../sun4gen/sun4_floppy.h"
#include "sun4c_auxio.h"
#include "promid_sun4.h"
#include "sys/proto.h"
#include "mmuproto.h"
#include "module/mutex.h"
#include "assert.h"
INIT_ASSERT


extern void		microsec_delay _ARGS(( int ));

/* The default geometry for the Sony Drives typically found in sparcstations */
flop_geom default_geoms[] = {
#define	LOW_DENSITY	0
	{ 1440,  9, 2, 80, 512, 250, -1, 18 },	/*  720 Kb 3.5" floppy */
#define	HIGH_DENSITY	1
	{ 2880, 18, 2, 80, 512, 500, -1, 36 }	/* 1.44 Mb 3.5" floppy */
};

/* Per drive information - there is only one supported! */
static flop_unit	Fdisk;

/* Per controller information */
static fdc_info		Fdc_info;

/* The registers of the controller */
static struct fdc_reg *	Fdc;

#define	INVALID_UNIT(u)	((u) != 0 || !Fdisk.f_present)

#define	GETDENSITY \
    ((get_auxioreg() & AUXIO_FLOPPY_DENSITY) ? HIGH_DENSITY : LOW_DENSITY)

#define	FLOPPY_CHANGED	(get_auxioreg() & AUXIO_FLOPPY_CHANGED)


static errstat flop_exec _ARGS(( fl_crb *, int ));

/***************************************/
/* Stubs for executing floppy commands */
/***************************************/

/*
 * sense_intr_status
 *	This command is executed to terminate the SEEK, RELATIVE_SEEK and
 *	RECALIBRATE commands.  It is called from the low level interrupt
 *	so it had better not stall long on the busy wait loops!!
 *	It expects that the original seek command has provided a result
 *	buffer so it had better be there.
 */
static void
sense_intr_status(crbp)
fl_crb *	crbp;
{
    DPRINTF(1, ("sense_intr_status\n"));

    assert(crbp != 0);

    /* Wait for busy signal to go away */
    while (Fdc->fdc_status & FRS_BUSY)
	/* Do nothing */;

    /* Wait for RQM to be set */
    while (!(Fdc->fdc_status & FRS_RQM))
	/* Do nothing */;
    
    /* Send command */
    Fdc->fdc_data = F_SENSE_INTR_STATUS;

    /* Wait for RQM to be set */
    while (!(Fdc->fdc_status & FRS_RQM))
	/* Do nothing */;

    crbp->res_buf[0] = Fdc->fdc_data;

    /* Wait for RQM to be set */
    while (!(Fdc->fdc_status & FRS_RQM))
	/* Do nothing */;

    crbp->res_buf[1] = Fdc->fdc_data;
    crbp->res_nbytes = 2;
}


/*
 * sense_drive_status
 *	Find out what the current status of the drive is, including
 *	presence of the floppy and write protect.  Returns -1 on error.
 *	Otherwise it returns the status byte.
 */
static int
sense_drive_status()
{
    fl_crb	crb;
    errstat	err;

    DPRINTF(1, ("sense_drive_status\n"));

    crb.cmd_buf[0] = F_SENSE_DRIVE_STATUS;
    crb.cmd_buf[1] = 0; /* unit # */
    crb.cmd_nbytes = 2;
    crb.cmd_type = FST_IMMEDIATE;
    if ((err = flop_exec(&crb, NOCHECK)) != STD_OK)
    {
#ifdef lint
	printf("%s\n", err_why(err));
#endif
	DPRINTF(0, ("floppy: sense_drive_status failed: %s\n", err_why(err)));
	return -1;
    }
    assert(crb.res_nbytes == 1);
    DPRINTF(1, ("drive_status = %x\n", crb.res_buf[0]));
    return crb.res_buf[0] & 0xFF;
}


/*
 * seek
 *	Seek to the specified cylinder.  Note that we always seek with
 *	checking off since we need to seek as part of the checking process
 *	and we can do checking later in the execution of a read/write cmd.
 */
static errstat
seek(cyl)
int	cyl;
{
    fl_crb	crb;
    errstat	err;

    DPRINTF(1, ("seek(%d)\n", cyl));

    crb.cmd_buf[0] = F_SEEK;
    crb.cmd_buf[1] = 0; /* unit # */
    crb.cmd_buf[2] = cyl;
    crb.cmd_nbytes = 3;
    crb.cmd_type = FST_SEEK;
    if ((err = flop_exec(&crb, NOCHECK)) == STD_OK)
    {
	assert(crb.res_nbytes == 2);
	if (!(crb.res_buf[0] & ST0_SEEKEND) || crb.res_buf[1] != cyl)
	{
	    DPRINTF(0, ("seek(%d) returned %x %d\n",
			 cyl, crb.res_buf[0], crb.res_buf[1]));
	    err = STD_SYSERR;
	}
    }
    return err;
}


/*
 * recalibrate
 *	Recalibrate the head if we seem to have lost position or a floppy has
 *	been inserted.
 */
static errstat
recalibrate()
{
    fl_crb	crb;
    errstat	err;
    uint8	status;

    DPRINTF(1, ("recalibrate\n"));

    crb.cmd_buf[0] = F_RECALIBRATE;
    crb.cmd_buf[1] = 0; /* unit # */
    crb.cmd_nbytes = 2;
    crb.cmd_type = FST_SEEK;
    if ((err = flop_exec(&crb, NOCHECK)) == STD_OK)
    {
	assert(crb.res_nbytes == 2); /* sense_intr_status was done! */
	status = crb.res_buf[0] & ~ST0_HEAD;
	if (status & ST0_EQUIP_CHK)
	{
	    DPRINTF(1, ("recalibrate: drive not present\n"));
	    err = FE_NODEV;
	}
	else
	{
	    if (status != ST0_SEEKEND)
	    {
		printf("recalibrate: failed status=%x, pcn=%x\n",
					       status, crb.res_buf[1]);
		err = STD_SYSERR;
	    }
	}
    }
    else
    {
	printf("recalibrate: flop_exec failed: %s\n", err_why(err));
    }
    return err;
}


/*
 * specify
 *	Set default for step-rate timer, head unload timer & head load timer.
 *	Also set DMA mode out.  The Suns don't have DMA for the floppy
 */
static void
specify()
{
    fl_crb	crb;

    DPRINTF(1, ("specify\n"));
    crb.cmd_buf[0] = F_SPECIFY;
    /* Step-rate time (bits 7 - 4) + Head unload time (bits 3 - 0) */
    crb.cmd_buf[1] = 0xC2;
    /* Head load time (bits 7 - 1) + no DMA (bit 0) */
    crb.cmd_buf[2] = 0x33;
    crb.cmd_nbytes = 3;
    crb.cmd_type = FST_NORESULT;
    (void) flop_exec(&crb, NOCHECK);
}


static void
configure(fifo_off)
int	fifo_off;	/* 1 if fifo should be disabled */
{
    fl_crb	crb;

    DPRINTF(1, ("configure(%d)\n", fifo_off));
    crb.cmd_buf[0] = F_CONFIGURE;
    crb.cmd_buf[1] = 0x64;	/* Motor timeout info? */
    if (fifo_off)
	crb.cmd_buf[2] = EFIFO | NOPOLL; /* Disable fifo & implied seek */
    else
	crb.cmd_buf[2] = EIS | NOPOLL | 0xF; /* Fifo threshold = 16 */
    crb.cmd_buf[3] = 0; /* Precompensation for entire disk? */
    crb.cmd_nbytes = 4;
    crb.cmd_type = FST_NORESULT;
    (void) flop_exec(&crb, NOCHECK);
}


/*
 * flop_rdwr
 *	Actually start some I/O with the floppy.  Once the exec begins the
 *	interrupt handler actually moves the bytes to/from the disk.
 *	We have enabled implied seeks so we needn't worry about doing seeks.
 *	However we should start a new command for each cylinder boundary,
 *	so it can do the implied seek to the next cylinder :-(
 */
static errstat
flop_rdwr(op, unit, startblk, blkcnt, buf)
int		op;		/* Either read or write */
int		unit;		/* Drive to use */
disk_addr	startblk;	/* 1st block to read */
disk_addr	blkcnt;		/* # blocks to read */
char *		buf;		/* Buffer to read from or write to */
{
    flop_geom *	fgm;
    fl_crb	crb;
    int		head;
    int		remcyl;
    errstat	err;

    if (INVALID_UNIT(unit) || (op != FLOP_READ && op != FLOP_WRITE) || buf == 0)
    {
	DPRINTF(0, ("flop_rdwr: bad argument\n"));
	return STD_ARGBAD;
    }
    
    DPRINTF(1, ("flop_rdwr: op = %d, blk = %d, cnt = %d\n",
			op, startblk, blkcnt));

    /* Make sure the requested blocks are on the disk. */
    fgm = Fdisk.f_geomp = &default_geoms[GETDENSITY];
    if (startblk < 0 || startblk > fgm->fg_size ||
	(startblk + blkcnt) > fgm->fg_size || blkcnt <= 0 ||
	blkcnt > fgm->fg_size)
    {
	return STD_ARGBAD;
    }

    /*
     * Fill in the bits of the command that remain constant no
     * matter how many cylinder changes there are.
     */
    if (op == FLOP_READ)
	crb.cmd_buf[0] = F_MT | F_MFM | F_SK | F_READ_DATA;
    else
	crb.cmd_buf[0] = F_MT | F_MFM | F_WRITE_DATA;
    crb.cmd_buf[5] = SS_512;		/* 512 byte sectors */
    crb.cmd_buf[6] = fgm->fg_nsectrk;
    crb.cmd_buf[7] = GPL;
    crb.cmd_buf[8] = DTL;	/* no fancy sector sizes here! */
    crb.cmd_type = FST_DATA_XFER;
    crb.cmd_nbytes = 9;

    /*
     * Now we run through issuing read or write commands, a new one for
     * each cylinder boundary we encounter.
     */
    crb.exe_buf = buf;
    while (blkcnt != 0)
    {
	disk_addr	actual_cnt;

	/* Calculate the cylinder and head positions for start of next op */
	remcyl = (startblk % fgm->fg_sectpercyl);
	head = remcyl / fgm->fg_nsectrk;

	crb.cmd_buf[1] = head << 2;			/* unit == 0 */
	crb.cmd_buf[2] = startblk / fgm->fg_sectpercyl;	/* start cylinder */
	crb.cmd_buf[3] = head;				/* start head */
	crb.cmd_buf[4] = (startblk % fgm->fg_nsectrk) + 1;/* start sector */

	/* Does the op fit completely on this cylinder? */
	actual_cnt = fgm->fg_sectpercyl - remcyl;
	if (actual_cnt > blkcnt)
	    actual_cnt = blkcnt;

	/* convert block to byte count */
	crb.exe_nbytes = actual_cnt << SHIFT_512;

	crb.res_nbytes = 0;	/* Avoid bogus result packages */

	if ((err = flop_exec(&crb, CHECK)) != STD_OK)
	{
	    DPRINTF(0, ("flop_rdwr: flop exec failed: %s\n", err_why(err)));
	    return err;
	}

	/* Set up next section of the I/O */
	startblk += actual_cnt;
	blkcnt -= actual_cnt;
    }
    return STD_OK;
}


static errstat
flop_fmt_track(cyl, head, fmtbuf, bufsz)
int	cyl;
int	head;
char *	fmtbuf;	/* Buffer must be 4 * # sectors per track bytes */
size_t	bufsz;
{
    fl_crb	crb;
    int		i;
    int		nsectrk;

    /*
     * FEATURE ALERT: fifo length must be added
     */
    crb.exe_nbytes = bufsz;
    crb.exe_buf = fmtbuf;

    nsectrk = Fdisk.f_geomp->fg_nsectrk;
    assert(bufsz == nsectrk * 4);
    /* Sectors are numbered from 1 so our loop starts there */
    for (i = 1; i <= nsectrk; i++)
    {
	fmtbuf[0] = (char) cyl;
	fmtbuf[1] = (char) head;
	fmtbuf[2] = (char) i;		/* Sector number */
	fmtbuf[3] = SS_512;		/* 512 byte sectors */
	fmtbuf += 4;
    }

    /* Make the command */
    crb.cmd_buf[0] = F_FORMAT_TRACK | F_MFM;
    crb.cmd_buf[1] = head << 2; /* unit # is 0 */
    crb.cmd_buf[2] = SS_512;
    crb.cmd_buf[3] = nsectrk & 0xFF;
    crb.cmd_buf[4] = GAP3;
    crb.cmd_buf[5] = 0xAA; /* Data fields are filled with this */
    crb.cmd_nbytes = 6;
    crb.cmd_type = FST_DATA_XFER;

    return flop_exec(&crb, CHECK);
}


static errstat
flop_format()
{
    errstat	err;
    char *	fmtbuf;
    int		cyl;
    size_t	bufsz;

    /* Find out what density the drive thinks it has. */
    Fdisk.f_geomp = &default_geoms[GETDENSITY];

    if (recalibrate() != STD_OK)
	return STD_SYSERR;

    /*
     * Need to make a buffer which can be delivered like a write command
     * to the disk with for 4 bytes for each sector on the track.
     */
    bufsz = Fdisk.f_geomp->fg_nsectrk * 4;
    if ((fmtbuf = (char *) malloc(bufsz)) == 0)
	return STD_NOMEM;

    assert(Fdisk.f_geomp->fg_ncyl > 0);

    /*
     * FEATURE ALERT:
     *   Methinks when they added implied seek and the fifo they forgot
     *   about the format command.
     *   1. Implied seek doesn't work with format track.
     *   2. If the fifo is enabled and we set TC at the end of writing
     *      the format data the chip doesn't drain the fifo but
     *      just stops.  Ie. it still thinks fifo length is 1.
     *   So now we make it true.  We also disable implied seek.
     */
    configure(FIFO_OFF);

    for (cyl = 0; cyl < Fdisk.f_geomp->fg_ncyl; cyl++)
    {
	(void) seek(cyl);

	if ((err = flop_fmt_track(cyl, 0, fmtbuf, bufsz)) != STD_OK ||
	    (err = flop_fmt_track(cyl, 1, fmtbuf, bufsz)) != STD_OK)
	{
	    DPRINTF(0, ("floppy format: failed at cylinder %d (%s)\n",
							    cyl, err_why(err)));
	    break;
	}
    }

    /* Reenable fifo and implied seek. */
    configure(FIFO_ON);
    free((_VOIDSTAR) fmtbuf);
    return err;
}


static void
flop_reset()
{
    fl_crb	crb;

    DPRINTF(0, ("flop_reset\n"));

    /*
     * Do a soft reset and then set data rate to 500 Kbps - high density.
     * Because we will get an interrupt we have to hang a result buffer
     * in the Fdisk struct :-(
     */
    assert(get_auxioreg() & AUXIO_FLOPPY_SELECT);
    
    Fdisk.f_crbp = &crb;
    Fdc_info.fdc_state = FST_RESETTING;

    Fdc->fdc_status = FRS_SWR;
    microsec_delay(5);
    Fdc->fdc_status = 0;
    microsec_delay(5);

    /* Wait for the interrupt ... ?  */
    (void) await((event) &Fdc, (interval) 10000);

    specify();
    configure(FIFO_ON);
}


/********************/
/* Execution module */
/********************/

static int select_count;

static void
select_floppy()
{
    if (select_count++ == 0)
	set_auxioreg(AUXIO_FLOPPY_SELECT, 1);
}


static void
deselect_floppy()
{
    if (--select_count == 0)
	set_auxioreg(AUXIO_FLOPPY_SELECT, 0);
}


/* Read the results from the results phase of a command */
static void
getresults(crbp)
fl_crb *	crbp;
{
    uint8	c;
    int		cnt;
    int		timeout;
    int		status;

    DPRINTF(1, ("getresults(%x)\n", crbp));
    assert(crbp);
    crbp->res_nbytes = 0;
    cnt = 0;
    timeout = DATAREG_WAIT;
    while ((status = Fdc->fdc_status) & FRS_BUSY)
    {
	if ((status & (FRS_RQM | FRS_DIO)) == (FRS_RQM | FRS_DIO))
	{
	    /* Get result byte.  Save it if there is room */
	    c = Fdc->fdc_data;
	    if (cnt < MAX_BUF_SIZE)
	    {
		crbp->res_buf[cnt++] = c;
		crbp->res_nbytes = cnt;
	    }
	    timeout = DATAREG_WAIT;
	}
	else
	{
	    /* Either busy will go away soon or it will become ready to send */
	    if (--timeout == 0)
	    {
		DPRINTF(0, ("getresults: timeout for byte %d\n", cnt+1));
		return;
	    }
	}
    }
#ifndef NDEBUG
    if (cnt >= MAX_BUF_SIZE)
	printf("floppy: got too many results bytes (%d)\n", cnt);
#endif /* NDEBUG */
    Fdc_info.fdc_state = FST_IDLE;
}


/*
 * check_change
 *	The function has to work out if there has been a floppy change.
 *	If not then it returns STD_OK.
 *	If so it must see if there is still a floppy in the drive.
 *	To do this we need to force a seek to take place.  If after the seek
 *	flop_changed() still returns true then there is no floppy since a real 
 *	seek would have cleared the changed bit in the auxio register.
 *	Therefore we check if we are at cylinder 0.  If so we seek away from
 *	it and if not we seek to it.
 *	If there is no floppy then we return STD_NOMEDIUM.
 *
 *	This routine is used by flop_exec so it must execute commands
 *	that use flop_exec with NOCHECK on.
 *	We can use the crb buffers for the unit in question since we
 *	haven't touched them yet in flop_exec.
 */
static errstat
check_change()
{
    int		cyl;
    int		stat;
    errstat	err;

    err = STD_OK;
    if (FLOPPY_CHANGED)
    {
	DPRINTF(0, ("floppy check_change: change detected\n"));
	/*
	 * In theory we should now wait 100 us but the sense_drive_status and
	 * the seek will take that long
	 */
	stat = sense_drive_status();
	if (stat == -1)
	    err = STD_SYSERR;
	else
	{
	    if (stat & ST3_TRACK0)
		cyl = 1; /* seek away from track 0 */
	    else
		cyl = 0;
	    if ((err = seek(cyl)) == STD_OK)
	    {
		/* After a successful seek the changed bit should be clear */
		if (FLOPPY_CHANGED)
		    err =  STD_NOMEDIUM;
	    }
	    else
	    {
		DPRINTF(0, ("floppy check_change: seek failed to cyl %d (%s)\n",
				    cyl, err_why(err)));
	    }
	}
    }
    return err;
}


/*
 * analyse_xfer_results
 *	After a data transfer we need to set the error status for the command
 *	in a consistent way so we do it here.  We can gather some statistics
 *	too later.
 */
static errstat
analyse_xfer_results(cmdp)
fl_crb *	cmdp;
{
    int	i;

    if (cmdp->res_nbytes != 7)
    {
	DPRINTF(0, ("flop_exec: got incorrect # result bytes %d\n",
			cmdp->res_nbytes));
	return STD_SYSERR;
    }
    if (cmdp->res_buf[0] & ST0_IC)
    {
	if (cmdp->res_buf[1] & ST1_NOT_WRITE)
	    return STD_WRITEPROT;

	DPRINTF(0, ("flop_exec: incorrect termination\n"));
	DPRINTF(0, ("Result bytes = "));
	for (i = 0; i < cmdp->res_nbytes; i++)
	    DPRINTF(0, ("%x ", cmdp->res_buf[i]));
	DPRINTF(0, ("\n"));
	return STD_SYSERR;
    }
    else
    {
	/* The command was supposed to have completed without error */
	assert(cmdp->res_buf[1] == 0);
	assert(cmdp->res_buf[2] == 0);
    }
    return STD_OK;
}


static errstat
flop_exec(cmdp, checking)
fl_crb *	cmdp;
int		checking;	/* 1 if we should check for floppy change */
{
    int		i;
    errstat	err;
    int		state;

    DPRINTF(1, ("flop_exec(%d)\n", checking));
    assert(cmdp != 0);

    assert(get_auxioreg() & AUXIO_FLOPPY_SELECT);

    /*
     * See if floppy is still there.  The "checking" flag is to stop
     * recursion when commands are issued to check the change.
     */
    if ((checking == CHECK) && (err = check_change()) != STD_OK)
    {
	DPRINTF(0, ("flop_exec: floppy has been removed.\n"));
	return err;
    }

    /* Make sure we are using the correct transfer rate for the operation */
    if (cmdp->cmd_type == FST_DATA_XFER)
    {
	assert(Fdisk.f_geomp);
	if (Fdisk.f_geomp->fg_xfer == 500)
	    Fdc->fdc_status = 0;	/* 500 Kbs - 1440 Kb floppy */
	else
	    Fdc->fdc_status = 2;	/* 250 Kbs - 720 Kb floppy */
	microsec_delay(2);
    }

    /* Look to see if the controller is busy */
    if (Fdc->fdc_status & FRS_BUSY)
    {
	DPRINTF(0, ("flop_exec: tried starting cmd while floppy was active\n"));
	return STD_SYSERR;
    }

    Fdisk.f_crbp = cmdp;
    Fdc_info.fdc_state = cmdp->cmd_type;

    /* Put the command bytes into the fifo */
    for (i = 0; i < cmdp->cmd_nbytes; i++)
    {
	int waittime;

	/* Wait for data register to be ready to receive the command */
	for (waittime = DATAREG_WAIT;;)
	{
	    if (Fdc->fdc_status & (FRS_DIO | FRS_RQM))
		break;
	    if (--waittime == 0)
	    {
		DPRINTF(0,
		  ("flop_exec: timed out waiting for RQM in cmd phase (%x)\n",
			Fdc->fdc_status));
		DPRINTF(0, ("%d bytes already sent\n", i));
		return STD_SYSERR;
	    }
	}
	Fdc->fdc_data = cmdp->cmd_buf[i];
    }

    err = STD_OK;

    switch (state = Fdc_info.fdc_state)
    {
    case FST_NORESULT:
	break; /* Nothing more to do ... */

    case FST_IMMEDIATE:
	getresults(cmdp);
	break;

    case FST_RESULT:
    case FST_DATA_XFER:
    case FST_SEEK:
	/* Wait for interrupt handler to signal completion */
	if (await((event) &Fdc, (interval) 30000) != 0)
	{
	    DPRINTF(0, ("flop_exec: await failed, status reg = %x\n",
			 Fdc->fdc_status));
	    err = STD_SYSERR;
	}
	if (state == FST_DATA_XFER)
	    err = analyse_xfer_results(cmdp);
	if (err != STD_OK)
	{
	    flop_reset();
	    (void) recalibrate();
	}
	break;

    default:
	DPRINTF(0, ("flop_exec: floppy in state %d\n", Fdc_info.fdc_state));
	panic("flop_exec: unexpected command state\n");
	/*NOTREACHED*/
    }
    return err;
}


/*ARGSUSED*/
static void
flop_intr(vecinfo)
int	vecinfo;
{
    int		len;
    char *	buf;
    uint8	tmp;

#ifdef STATISTICS
    Fdc_info.fdc_ints++;
#endif

    switch (Fdc_info.fdc_state)
    {
    case FST_DATA_XFER:
	len = Fdisk.f_crbp->exe_nbytes;
	assert(len > 0);
	buf = Fdisk.f_crbp->exe_buf;
	while ((tmp = Fdc->fdc_status) & FRS_RQM)
	{
	    if (!(tmp & FRS_EXM))
	    {
		/*
		 * If we are no longer in non-DMA mode then it wants to
		 * give the results!  Something went wrong.
		 */
		getresults(Fdisk.f_crbp);
		enqueue((void (*)()) wakeup, (long) &Fdc);
		return;
	    }
	    if (tmp & FRS_DIO)
		*buf = Fdc->fdc_data; /* Do a read */
	    else
		Fdc->fdc_data = *buf; /* Do write */
	    if (--len <= 0)
		break;
	    buf++;
	    /*
	     * GRRR:  We can't handle the every 12 us interrupts fast enough
	     * so we just have to busy-wait for the bytes to arrive!
	     * We ought to time this but I figure if something is wrong the
	     * busy signal will go away quite quickly or it will try to
	     * tell us about it and assert RQM.
	     */
	    do
	    {
		tmp = Fdc->fdc_status;
	    } while ((tmp & FRS_BUSY) && !(tmp & FRS_RQM));
	}
	/* Save current position */
	Fdisk.f_crbp->exe_nbytes = len;
	Fdisk.f_crbp->exe_buf = buf;
	if (len == 0) {
	    /* We're done: send terminal count to the chip */
	    set_auxioreg(AUXIO_FLOPPY_TC, 1);
	    Fdc_info.fdc_state = FST_RESULT;
	    set_auxioreg(AUXIO_FLOPPY_TC, 0);
	}
	/*
	 * We now wait a short time for the interrupt for the next batch
	 * of data or the results phase.
	 */
	return;

    case FST_RESULT:
	/* Read out the result bytes until the busy signal is gone */
	getresults(Fdisk.f_crbp);
	break;
    
    case FST_SEEK:
    case FST_RESETTING:
	/* Must do a sense interrupt status */
	sense_intr_status(Fdisk.f_crbp);
	break;

    case FST_NORESULT:
    default:
	/* Spurious interrupt? */
#ifdef STATISTICS
	Fdc_info.fdc_spurious++;
#endif
	break;
    }
    enqueue((void (*)()) wakeup, (long) &Fdc);
    return;
}


/*
 *********************************************
 * External interface to virtual disk server *
 *********************************************
 */

/*ARGSUSED*/
errstat
flop_ctlr_init(devaddr, intr)
long	devaddr;
int	intr;
{
    /* An SLC or ELC won't have a floppy but looks like it does to software! */
    if (machine.mi_id == MACH_SUN4C_20 || machine.mi_id == MACH_SUN4C_30)
	return STD_NOTFOUND;

    if (Fdc != 0)
	return STD_OK;	/* already initialised */

    Fdc = (struct fdc_reg *) mmu_virtaddr("/fd");
    DPRINTF(0, ("flop_ctlr_init: mapped in floppy at address 0x%x\n", Fdc));
    if (Fdc == 0)
	return STD_SYSERR;

    /*
     * It takes about 100 us between the select and the density bit of the
     * auxio register to reflect the reality of the disk if it is in there.
     * Since this is an unacceptable delay we switch on the bit now and
     * we only turn it off if we detect that no drive is present in unit init.
     * Otherwise the floppy is permanently selected so we can attack at will.
     */
    select_floppy();

    setvec((unsigned) intr, flop_intr);

    mu_init(&Fdisk.f_lock);
    mu_lock(&Fdisk.f_lock);

    /* Make sure that the eject line is set */
    set_auxioreg(AUXIO_FLOPPY_EJECT, 1);

    /* Now reset the hardware */
    flop_reset();

    mu_unlock(&Fdisk.f_lock);
    return STD_OK;
}


/*
 * flop_unit_init
 *	The real work is done when the floppy is inserted.  We do very little
 *	at this point.
 */
/*ARGSUSED*/
errstat
flop_unit_init(devaddr, unit)
long	devaddr;	/* unused */
int	unit;		/* in: drive to use; must be 0 */
{
    errstat	err;

    if (unit != 0)
	return STD_ARGBAD;

    mu_lock(&Fdisk.f_lock);

    /* If there is no drive then the unit_init should fail */
    Fdisk.f_present = (recalibrate() == FE_NODEV) ? 0 : 1;
    if (Fdisk.f_present == 0)
    {
	deselect_floppy();
	err = STD_NOTFOUND;
    }
    else
	err = STD_OK;
    mu_unlock(&Fdisk.f_lock);
    return err;
}


/*ARGSUSED*/
int
flop_unittype(devaddr, unit)
long	devaddr;	/* unused */
int	unit;		/* unused */
{
    return UNITTYPE_REMOVABLE;
}


/*ARGSUSED*/
errstat
flop_read(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
long		devaddr;	/* unused */
int		unit;		/* unit to use */
disk_addr	strt_blk;	/* first block to read */
disk_addr	blk_cnt;	/* total number of blocks to be read */
char *		buf_ptr;	/* destination for the read data */
{
    errstat	err;

#ifdef STATISTICS
    Fdc_info.fdc_reads++;
#endif
    mu_lock(&Fdisk.f_lock);
    err = flop_rdwr(FLOP_READ, unit, strt_blk, blk_cnt, buf_ptr);
    mu_unlock(&Fdisk.f_lock);
    return err;
}


/*ARGSUSED*/
errstat
flop_write(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
long		devaddr;	/* unused */
int		unit;		/* unit to use */
disk_addr	strt_blk;	/* first block to write */
disk_addr	blk_cnt;	/* total number of blocks to be written */
char *		buf_ptr;	/* source for the write */
{
    errstat	err;

#ifdef STATISTICS
    Fdc_info.fdc_writes++;
#endif
    mu_lock(&Fdisk.f_lock);
    err = flop_rdwr(FLOP_WRITE, unit, strt_blk, blk_cnt, buf_ptr);
    mu_unlock(&Fdisk.f_lock);
    return err;
}


/*ARGSUSED*/
errstat
flop_capacity(devaddr, unit, lastblk, blksz)
long	devaddr;	/* unused */
int	unit;		/* in: drive to use; must be 0 */
int32 *	lastblk;	/* out: pointer to # blocks on device */
int32 *	blksz;		/* out: pointer to size of logical block */
{
    flop_geom *	fgp;

#ifdef STATISTICS
    Fdc_info.fdc_capacity++;
#endif

    /* Check if unit is not currently initialised or non-existent */
    if (INVALID_UNIT(unit))
	return STD_ARGBAD;

    mu_lock(&Fdisk.f_lock);
    assert(get_auxioreg() & AUXIO_FLOPPY_SELECT);
    /* Note that we report the maximum supported density. */
    fgp = Fdisk.f_geomp = &default_geoms[HIGH_DENSITY];
    *lastblk = fgp->fg_size;
    *blksz = fgp->fg_sectsz;
    mu_unlock(&Fdisk.f_lock);
    return STD_OK;
}


/*ARGSUSED*/
#ifdef __STDC__
errstat
flop_control(long devaddr, int unit, int cmd, bufptr buf, bufsize size)
#else
errstat
flop_control(devaddr, unit, cmd, buf, size)
long	devaddr;	/* unused */
int	unit;		/* in: drive to use; must be 0 */
int	cmd;		/* which control command to execute */
bufptr	buf;		/* possible data for command */
bufsize	size;		/* #bytes in buf */
#endif
{
    errstat	err;

    if (INVALID_UNIT(unit))
	return STD_ARGBAD;

    mu_lock(&Fdisk.f_lock);
    assert(get_auxioreg() & AUXIO_FLOPPY_SELECT);
    switch (cmd)
    {
    case DSC_EJECT:
	/*
	 * If all is well we will have had a 2 microsecond delay in the return
	 * from select_floppy plus the switch!  The eject line was already 1.
	 * So we drive it low for the eject and then restore it to 1.
	 */
	set_auxioreg(AUXIO_FLOPPY_EJECT, 0);
	microsec_delay(2);
	set_auxioreg(AUXIO_FLOPPY_EJECT, 1);
	err =  STD_OK;
#ifdef STATISTICS
	Fdc_info.fdc_eject++;
#endif
	break;

    case DSC_FORMAT:
	err = flop_format();
#ifdef STATISTICS
	Fdc_info.fdc_format++;
#endif
	break;
	
    default:
	printf("flop_control: unknown command %d\n", cmd);
	err = STD_COMBAD;
	break;
    }
    mu_unlock(&Fdisk.f_lock);
    return err;
}


#if defined(STATISTICS) && !defined(SMALL_KERNEL)

int
flop_dump_stats(begin, end)
char *	begin;
char *	end;
{
    char * p;

    if (Fdisk.f_present)
    {
	p = bprintf(begin, end, "Floppy Disk Driver Statistics\n\n");
	p = bprintf(p, end, "  Reads\t\t%d\tWrites\t%d\t",
			    Fdc_info.fdc_reads, Fdc_info.fdc_writes);
	p = bprintf(p, end, "Capacity\t%d\n", Fdc_info.fdc_capacity);
	p = bprintf(p, end, "  Ejects\t%d\tFormats\t%d\t",
			    Fdc_info.fdc_eject, Fdc_info.fdc_format);
	p = bprintf(p, end, "Interrupts\t%d (%d spurious)\n",
			    Fdc_info.fdc_ints, Fdc_info.fdc_spurious);
    }
    else
	p = bprintf(begin, end, "Floppy Disk Drive Not Present\n\n");
    return p - begin;
}

#endif /* defined(STATISTICS) && !defined(SMALL_KERNEL) */
