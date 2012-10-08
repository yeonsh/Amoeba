/*	@(#)sd.c	1.12	96/02/27 13:49:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	sd.c
**
**	This file implements the standard scsi routines needed for Amoeba
**	to talk to scsi disks.  The scsi driver for a particular machine
**	must implement the following routines:
**
**	scsi_cmd -	start a scsi command in either polled or interrupt mode
**			and return when the command has completed.
**	scsi_ctlr_init - initialise the interrupt vector for a scsi controller
**			and initialise the scsi bus.
**
**	The driver is expected to handle and problems with multiple commands
**	on the scsi bus.
**	There are further routines common to the tape interface and this
**	interface defined in scsicommon.c
**
** Author: Greg Sharp, Aug 1990 & Feb 1991
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "disk/disk.h"
#include "disk/conf.h"
#include "scsi.h"
#include "module/mutex.h"
#include "stdlib.h"
#include "sys/proto.h"
#include "sd.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

extern scsi_ctlr	Scsi_ctlr_tab[];

int32 scsi_dev2ctlr _ARGS(( long ));

static errstat Sense_map[] =
{
	/* Disk sense mapping - see scsi.h for details */
	STD_OK, STD_SYSERR, STD_SYSERR, STD_SYSERR, STD_SYSERR, STD_OK,
	STD_NOMEDIUM, STD_SYSERR, STD_SYSERR, STD_SYSERR,
	SERR_AGAIN, STD_WRITEPROT, STD_SYSERR, STD_SYSERR, SERR_AGAIN,
	SERR_AGAIN, STD_OK, STD_SYSERR, STD_SYSERR, STD_SYSERR
};

/*
** There is an unfortunate limit on data transfer sizes imposed by dma
** controllers which for now we set by looking up info from the dma_controller
** table and the current block size
*/
unsigned int	Sd_Lim_blk_cnt;


static errstat
sd_docapacity(p, unit, buf, bufsz)
scsi_unit *	p;
int		unit;
char *		buf;
int		bufsz;
{
    int		lunit;

    lunit = unit & 07;

/* fill in command block */
    p->u_cmdsz = 10;
    p->u_cmd.c_10.command = READ_CAPACITY;
    p->u_cmd.c_10.lunrelb = (uint8) (lunit << LUNSHIFT);
    p->u_cmd.c_10.hhadr = 0;
    p->u_cmd.c_10.hladr = 0;
    p->u_cmd.c_10.lhadr = 0;
    p->u_cmd.c_10.lladr = 0;
    p->u_cmd.c_10.res = 0;
    p->u_cmd.c_10.tflenh = 0;
    p->u_cmd.c_10.tflenl = 0;
    p->u_cmd.c_10.cbyte = CBYTE;
    p->u_data = buf;
    p->u_datasz = bufsz;
    p->u_timeout = 5*SECONDS;
    p->u_flags &= ~SF_WRITE;

    scsi_cmd(p->u_ctlr, unit, POLLED);
    return p->u_errstat;
}


static errstat
disk_rdwr(op, p, unit, ptr, nblks, strtblk)
uint8		op;		/* command to execute */
scsi_unit *	p;		/* pointer to data for this command */
int		unit;		/* scsi target and logical unit number */
char *		ptr;		/* buffer to read from/write to */
disk_addr	nblks;		/* # blocks to r/w */
disk_addr	strtblk;	/* starting block on disk */
{
    int		lunit;		/* logical unit number on scsi target */

    lunit = unit & 07;

    if (strtblk < 0x200000 && nblks < 256)
    {
    /* six byte command */
	p->u_cmdsz = 6;
	p->u_cmd.c_6.command = op;
	p->u_cmd.c_6.lunhadr =
		    (uint8) ((lunit << LUNSHIFT) | ((strtblk>>16) & 0x1F));
	p->u_cmd.c_6.madr = (uint8) (strtblk >> 8);
	p->u_cmd.c_6.ladr = (uint8) strtblk;
	p->u_cmd.c_6.tflen = (uint8) nblks;
	p->u_cmd.c_6.cbyte = CBYTE;
    }
    else
    {
    /* ten byte command */
	p->u_cmdsz = 10;
	p->u_cmd.c_10.command = op + 0x20;
	p->u_cmd.c_10.lunrelb = (uint8) (lunit << LUNSHIFT);
	p->u_cmd.c_10.hhadr = (uint8) (strtblk >> 24);
	p->u_cmd.c_10.hladr = (uint8) (strtblk >> 16);
	p->u_cmd.c_10.lhadr = (uint8) (strtblk >> 8);
	p->u_cmd.c_10.lladr = (uint8) strtblk;
	p->u_cmd.c_10.res = 0;
	p->u_cmd.c_10.tflenh = (uint8) (nblks >> 8);
	p->u_cmd.c_10.tflenl = (uint8) nblks;
	p->u_cmd.c_10.cbyte = CBYTE;
    }
    if (op == WRITE)
	p->u_flags |= SF_WRITE;
    else
	p->u_flags &= ~SF_WRITE;
    p->u_timeout = 10*SECONDS;

    p->u_data = ptr;
    p->u_datasz = nblks * p->u_blksz;
    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    if (p->u_errstat != STD_OK)
	DPRINTF(-1, ("disk_rdwr: got error %d\n", p->u_errstat));
    return p->u_errstat;
}


/*
**	sd_medium_init
**
** When we get a unit attention or when we first start up we need to
** collect up the parameters for the disk medium.  In addition we test if
** the unit is ready just for good measure.
*/
static errstat
sd_medium_init(ctlr, unit)
int32	ctlr;
int	unit;
{
    errstat	err;
    int		i;
    scsi_unit *	p;
    char	buf[MODESENSE_SIZE];
    int32	info;

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];

    /* Start the disk */
    i = 2;
    do
    {
	err = scsi_start_stop_unit(ctlr, unit, 1, 1);
	if (err == SERR_CHECKCON)
	{
	    if ((err = scsi_req_sense(ctlr, unit)) == STD_OK)
		err = Sense_map[scsi_analyze_sense(ctlr, unit, &info)];
	}
    } while (err == SERR_AGAIN && --i != 0);

    if (err != STD_OK)
    {
	printf("sd_medium_init: start_unit failed for unit %d on ctlr %d (%s) (not fatal)\n",
						    unit, ctlr, err_why(err));
	/* We press on here since old disks return and error for this */
    }

/*
** See if it is on-line, active or whatever it needs to be.  Note that some
** devices need two or three test_unit_ready commands before they will talk
** to us.  We try a maximum of ten times with a 3 second delay between
** attempts to give the device time to get ready.
*/
    for (i = 0; i < 10; i++)
    {
	err = scsi_test_unit_ready(ctlr, unit);
	if (err == SERR_CHECKCON)
	{
	    errstat err2;

	    err2 = scsi_req_sense(ctlr, unit);
	    if (err2 != STD_OK)
	    {
		printf("sd_medium_init: couldn't get sense for unit %d", unit);
		printf(" on controller %lx (err = %d)\n", ctlr, err2);
	    }
	    else
		err = Sense_map[scsi_analyze_sense(ctlr, unit, &info)];
	}
	if (err != STD_OK)
	    (void) await((event) 0, (interval) 3000);
    }

/* If the device isn't ready then we had better think twice about it */
    if (err != STD_OK)
    {
	printf("sd_medium_init: unit %d on controller %lx", unit, ctlr);
	printf(" not ready (err = %d)\n", err);
    /*
    ** It may be a floppy or an optical disk - in this case it is ok if there
    ** is no medium at boot time.  We can still publish a capability for the
    ** device.  On the other hand if it seems to be a fixed disk it should be
    ** ready and we assume it is defective.
    */
	if (p->u_remove == UNITTYPE_REMOVABLE)
	    err = STD_OK;
    }

/*
** See what the medium's block size is, if possible.  Otherwise we take
** a default block size and assume that it is not write protected.
** Not all scsi devices implement mode sense.
*/
    if (scsi_mode_sense(ctlr, unit, buf, MODESENSE_SIZE) != STD_OK)
    {
	p->u_blksz = BLOCKSIZE;
	p->u_flags &= ~SF_WRITE_PROT;
    }
    else
	if (p->u_blksz == 0)
	    p->u_blksz = BLOCKSIZE;

/*
 * Set the maximum number of blocks we can DMA at a time, if it isn't
 * already set by the ctlr_init function.  This default is usually safe.
 * ie, 32k xfers.
 */
    if (Sd_Lim_blk_cnt == 0)
	Sd_Lim_blk_cnt = (1 << 15) / p->u_blksz;

/*
** In the case of a read only direct access drive (such as an optical disk)
** we pretend it is a normal direct access device that is read protected
*/
    if (p->u_devtype == INQ_READ_ONLY)
    {
	p->u_devtype = INQ_DIR_ACCESS;
	p->u_flags |= SF_WRITE_PROT;
    }

    return err;
}


/*
**	sd_check_sense
**
** If a scsi disk returns a unit attention error then if it has removable
** media we need to re-initialize our data about it in case it changed.
** After that we can remap the error to a well understood error code.
** NB. It is not safe to call this routine from sd_medium_init!
*/

static errstat
sd_check_sense(ctlr, unit)
int32	ctlr;
int	unit;
{
    errstat	err;
    int		sense_err;
    int32	info;

    err = scsi_req_sense(ctlr, unit);
    if (err != STD_OK)
	return err;
    sense_err = scsi_analyze_sense(ctlr, unit, &info);
    if (sense_err == SENSE_UNIT_ATN &&
	    Scsi_ctlr_tab[ctlr].c_unit[unit]->u_remove == UNITTYPE_REMOVABLE)
	(void) sd_medium_init(ctlr, unit);
    DPRINTF(-1, ("sd_check_sense: sense_err = %d, maps to %d\n",
					sense_err, Sense_map[sense_err]));
    return Sense_map[sense_err];
}


/*
**	sd_rdwr
**
** This puts a loop around the disk_rdwr routine to handle reads larger
** a certain size.
*/

static errstat
sd_rdwr(op, ctlr, unit, strt_blk, blk_cnt, buf_ptr)
uint8		op;		/* read or write command */
int32		ctlr;		/* scsi controller */
int		unit;		/* scsi bus unit # (0 - 63) */
disk_addr	strt_blk;	/* first block to read */
disk_addr	blk_cnt;	/* total number of blocks to be read */
char *		buf_ptr;	/* destination for the read data */
{
    scsi_unit * p;		/* pointer to data for this command */
    disk_addr	numblks;	/* hack to handle big reads */
    errstat	err;		/* error status of the scsi command */

    if (blk_cnt <= 0 || strt_blk < 0)
	return STD_ARGBAD;

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_DIR_ACCESS)
	return STD_SYSERR;
/*
** At present we do a mu_lock to protect the data structure
** Later someone should try to figure out how to queue requests and
** run an elevator algorithm over the queue to get some performance
*/
    mu_lock(&p->u_mtx);

    /*
    ** Handle big reads - we set a maximum block count due to dma controllers
    ** that have smallish registers.  Those that can handle more can set
    ** Sd_Lim_blk_cnt higher.
    */
    do
    {
	numblks = MIN(Sd_Lim_blk_cnt, blk_cnt);
	err = disk_rdwr(op, p, unit, buf_ptr, numblks, strt_blk);
	/* If the error is recoverable we try to do so - twice */
	if (err == SERR_AGAIN || (err == SERR_CHECKCON &&
			    (err = sd_check_sense(ctlr, unit)) == SERR_AGAIN))
	{
	    err = disk_rdwr(op, p, unit, buf_ptr, numblks, strt_blk);
	    if (err == SERR_AGAIN || (err == SERR_CHECKCON &&
			    (err = sd_check_sense(ctlr, unit)) == SERR_AGAIN))
		err = disk_rdwr(op, p, unit, buf_ptr, numblks, strt_blk);
	}
	if (err != STD_OK)
	{
	    printf("sd_rdwr: ctlr %d, unit %d disk %s error (%d).\n",
			    ctlr, unit, (op == WRITE) ? "write" : "read", err);
	    printf("sd_rdwr: start block = %lx, num blocks = 0x%lx.\n",
							strt_blk, numblks);
	}
	strt_blk += numblks;
	buf_ptr += numblks * p->u_blksz;
    } while (err == STD_OK && (blk_cnt -= numblks) > 0);

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	External Interface Routines
**	***************************
**
**	There are four externally visible functions in this interface:
**		sd_unit_init, sd_read, sd_write, sd_capacity
**	In addition there are routines in common with the tape driver:
**		scsi_ctlr_init, scsi_cmd, scsi_mode_sense, scsi_mode_select,
**		scsi_inquiry, scsi_test_unit_ready, scsi_devtype,
**		scsi_print_inquiry
**	All return an errstat (error status) reflecting the success of the
**	operation.
**
*/


/*
**	sd_unit_init
**
** The following attempts to initialise a direct access scsi unit (disk).
** It should be called only once at boot time.
** In the case of devices with removable media it will return success
** even if the device is not ready since all that is probably wrong is
** that there is no disk in the device.
** It returns the error status of the initialization.
*/

errstat
sd_unit_init(devaddr, unit)
long	devaddr;	/* address of scsi controller */
int	unit;		/* unit on scsi controller */
{
    char	buf[INQUIRY_BUFSZ];	/* to receive inquiry answer */
    errstat	err;
    scsi_unit *	p;
    int		i;
    int32	info;
    int32	ctlr;

/*
 * If there are devices which are not embedded SCSI devices then the following
 * code should be disabled.  If we only expect embedded SCSI devices then it
 * saves time looking for devices that won't be there.
 */
#if !defined(NOT_JUST_EMBEDDED_SCSI)
    if (unit & 07)
	return SERR_NODEV;
#endif /* NOT_JUST_EMBEDDED_SCSI */

    ctlr = scsi_dev2ctlr(devaddr);
    DPRINTF(0, ("sd_unit_init(ctlr %d, unit %d)\n", ctlr, unit));
    assert(ctlr >= 0);
/*
** If there is no struct allocated for this device it has not previously
** been initialised (successfully).
*/
    if ((p = Scsi_ctlr_tab[ctlr].c_unit[unit]) == 0)
    {
	if ((p = (scsi_unit *) calloc(sizeof(scsi_unit), 1)) == 0)
	    return STD_NOMEM;
	Scsi_ctlr_tab[ctlr].c_unit[unit] = p;
	mu_init(&p->u_mtx);
    }
    mu_lock(&p->u_mtx);
    p->u_ctlr = ctlr;
    p->u_unit = unit;

/*
** Begin with an inquiry to find out what sort of device it is
** Since old disks tend to be a deaf to requests for several seconds after a
** reset we try several times before giving up and concluding that nothing
** is there.  We sleep for 0.5 seconds between attempts.
*/
    for (i = 0; i < 10; i++)
    {
	err = scsi_inquiry(ctlr, unit, buf, INQUIRY_BUFSZ);
	if (err == SERR_SELFAIL) /* there is no target there! */
	{
	    free((_VOIDSTAR) p);
	    Scsi_ctlr_tab[ctlr].c_unit[unit] = 0;
	    mu_unlock(&p->u_mtx);
	    return SERR_NODEV;
	}
	if (err == STD_OK || err == STD_SYSERR)
	    break;
	if (err == SERR_CHECKCON)
	{
	    errstat	err2;

	    err2 = scsi_req_sense(ctlr, unit);
	    if (err2 != STD_OK)
	    {
		printf(
"sd_unit_init: couldn't get sense for unit %d on controller %lx (err = %d)\n",
							    unit, ctlr, err2);
	    }
	    else
	    {
		err = Sense_map[scsi_analyze_sense(ctlr, unit, &info)];
		if (err == STD_OK)
		    break;
	    }
	}
	(void) await((event) 0, (interval) 500);
    }

    if (err == STD_OK)
    {
	if ((buf[0] & 0xFF) == INQ_NO_DEVICE)
	{
#ifndef NDEBUG
	    printf("sd_unit_init: unit %d not present on controller %lx\n",
								    unit, ctlr);
#endif /* NDEBUG */
	    return SERR_NODEV;
	}
	p->u_devtype = buf[0];
	if (p->u_devtype != INQ_DIR_ACCESS && p->u_devtype != INQ_READ_ONLY)
	{
	    printf("sd_unit_init: controller %lx, unit %d %s!\n",
					ctlr, unit, scsi_devtype(p->u_devtype));
	    printf("Expected %s\n", scsi_devtype(INQ_DIR_ACCESS));
	    return STD_SYSERR;
	}
	p->u_remove = (buf[1] & 0x80) ? UNITTYPE_REMOVABLE: UNITTYPE_FIXED;
	scsi_print_inquiry(ctlr, unit, buf, INQUIRY_BUFSZ);
    }
    else
    {
	printf("sd_unit_init: inquiry failed for unit %d on controller %lx ",
								unit, ctlr);
	printf("(err = %d)\nassuming it is an adaptec 400 board\n", err);
    /* best guess at the device type */
	p->u_devtype = INQ_DIR_ACCESS;
	p->u_remove = UNITTYPE_FIXED;
    }

    if ((err = sd_medium_init(ctlr, unit)) != STD_OK)
    {
	printf("sd_unit_init: medium_init failed (err = %d)\n", err);
	return SERR_NODEV;
    }

#ifndef NO_DISCON
    /*
     * The disconnect capability will be removed by the driver if it
     * discovers that the unit can't do it
     */
    p->u_flags |= SF_CAN_DISCON | SF_NEGSYNC;
#endif
    if ((err = scsi_test_unit_ready(ctlr, unit)) != STD_OK)
    {
	printf("sd_unit_init: test_unit_ready failed (err = %d)\n", err);
    }

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	Read blocks from disk
*/

errstat
sd_read(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
long		devaddr;	/* Device address to use */
int		unit;		/* scsi bus unit # (0 - 63) */
disk_addr	strt_blk;	/* first block to read */
disk_addr	blk_cnt;	/* total number of blocks to read */
char *		buf_ptr;	/* destination for the read data */
{
    int32		ctlr;		/* scsi controller */

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);
    return sd_rdwr(READ, ctlr, unit, strt_blk, blk_cnt, buf_ptr);
}


/*
**	Write blocks to scsi disk
*/

errstat
sd_write(devaddr, unit, strt_blk, blk_cnt, buf_ptr)
long		devaddr;	/* Device address to use */
int		unit;		/* scsi bus unit # (0 - 63) */
disk_addr	strt_blk;	/* first block to write */
disk_addr	blk_cnt;	/* total number of blocks to be written */
char *		buf_ptr;	/* source for the write */
{
    int32		ctlr;		/* scsi controller */

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);
    return sd_rdwr(WRITE, ctlr, unit, strt_blk, blk_cnt, buf_ptr);
}


/*
**	Capacity
**
**	Gets capacity of the specified unit - only works for direct
**	dma devices.  Returns 0 for success and -1 for failure
*/

errstat
sd_capacity(devaddr, unit, lastblk, blksz)
long	devaddr;	/* Device address to use */
int	unit;		/* scsi unit number of device */
int32 *	lastblk;	/* out: pointer to # blocks on device */
int32 *	blksz;		/* out: pointer to size of logical block */
{
    char	buf[8];		/* space for return message */
    scsi_unit * p;		/* pointer to data for this command */
    errstat	err;
    int32		ctlr;		/* scsi controller */

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0)
	return STD_ARGBAD;

    if (p->u_devtype != INQ_DIR_ACCESS)
	return STD_SYSERR;

    mu_lock(&p->u_mtx);
    err = sd_docapacity(p, unit, buf, 8);
/*
** If we get a check condition then we analyze the sense data in case we can
** recover.
*/
    if (err == SERR_CHECKCON &&
			(err = sd_check_sense(p->u_ctlr, unit)) == SERR_AGAIN)
	err = sd_docapacity(p, unit, buf, 8);

    if (err == STD_OK)
    {
	*lastblk = ((buf[0] & 0xff) << 24) |
		   ((buf[1] & 0xff) << 16) |
		   ((buf[2] & 0xff) << 8)  |
		   (buf[3] & 0xff);
	*blksz = ((buf[4] & 0xff) << 24) |
		 ((buf[5] & 0xff) << 16) |
		 ((buf[6] & 0xff) << 8)  |
		 (buf[7] & 0xff);
    }

    mu_unlock(&p->u_mtx);
    return err;
}
