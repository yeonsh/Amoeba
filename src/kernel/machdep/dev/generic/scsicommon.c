/*	@(#)scsicommon.c	1.10	96/02/27 13:49:37 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	This file contains the routines needed for tape and disk interfaces
**	that are used for scsi devices.
**
**	The other initialisation routine scsi_ctlr_init is defined in the scsi
**	driver since it is driver specific.
**
** Author: Greg Sharp, Aug 1990 & Feb 1991
*/

#include "amoeba.h"
#include "debug.h"
#include "cmdreg.h"
#include "stderr.h"
#include "scsi.h"
#include "stdlib.h"
#include "disk/conf.h"
#include "module/mutex.h"
#include "ctype.h"
#include "sys/proto.h"
#include "scsicommon.h"

extern scsi_ctlr	Scsi_ctlr_tab[];
extern unsigned int	Num_scsi_ctlrs;


/*
**	scsi_dev2ctlr
**
** Converts the device address given by the various servers (tape, disk)
** into an index (ie, controller number) into the Scsi_ctlr_tab array.
** Returns -1 if not found.
*/

int32
scsi_dev2ctlr(devaddr)
long devaddr;
{
    int32	ctlr;
    scsi_ctlr *	c;

    ctlr = Num_scsi_ctlrs;
    c = &Scsi_ctlr_tab[ctlr - 1];
    while (--ctlr >= 0 && c->c_addr != devaddr)
	c--;
    return ctlr;
}


/*
**	scsi_inquiry
**
** Send an inquiry command to the specified unit.
** Before calling this routine you must have locked the mutex for the
** relevant Scsi_ctlr_tab entry!
*/

errstat
scsi_inquiry(ctlr, unit, buf, bufsz)
int32	ctlr;	/* scsi controller to use */
int	unit;	/* unit on scsi target to inquire into */
char *	buf;	/* buffer for reply */
char	bufsz;	/* size of the reply buffer */
{
    scsi_unit *	p;

    DPRINTF(0, ("scsi_inquiry(ctlr %d, unit %d)\n", ctlr, unit));
    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0)
	return STD_SYSERR;

/* build "inquiry" command */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = INQUIRY;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = bufsz;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = buf;
    p->u_datasz = bufsz;
    p->u_timeout = 4*SECONDS;
    p->u_flags &= ~SF_WRITE;

/* send "inquiry" command in polled mode */
    scsi_cmd(ctlr, unit, POLLED);
    return p->u_errstat;
}


/*
**	scsi_test_unit_ready
**
** Before calling this routine you must have locked the mutex for the
** relevant Scsi_ctlr_tab entry!
*/

errstat
scsi_test_unit_ready(ctlr, unit)
int32	ctlr;	/* scsi controller to use */
int	unit;	/* unit on scsi target to inquire into */
{
    scsi_unit *	p;

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0)
	return STD_SYSERR;

/* build "test unit ready" command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TEST_UNIT_READY;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = 0;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_timeout = 4*SECONDS;
    p->u_flags &= ~SF_WRITE;

/* send "test unit ready" command */
    scsi_cmd(ctlr, unit, POLLED);
    return p->u_errstat;
}


/*
**	scsi_start_stop_unit
*/

errstat
scsi_start_stop_unit(ctlr, unit, start, immed)
int32	ctlr;	/* scsi controller to use */
int	unit;	/* unit on scsi target to inquire into */
uint8	start;	/* set if unit should start, reset if it should stop */
uint8	immed;	/* set if command should return immediately */
{
    scsi_unit *	p;

    if ((start & ~1) || (immed & ~1))
	return STD_ARGBAD;

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0)
	return STD_SYSERR;

/* build "start/stop unit" command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = START_STOP_UNIT;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT) | immed;
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = start;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_timeout = 4*SECONDS;
    p->u_flags &= ~SF_WRITE;

/* send command */
    scsi_cmd(ctlr, unit, POLLED);
    return p->u_errstat;
}


/*
**	scsi_mode_select
**
** Tell the tape unit about the density and other characteristics we desire.
** NB:  We don't lock the mutex since the most probable caller is the
**      unit_init routine.
*/

errstat
scsi_mode_select(ctlr, unit, buf, bufsz)
int32	ctlr;
int	unit;
char *	buf;	/* mode data to send */
int	bufsz;	/* size of buf */
{
    scsi_unit *	p;	/* pointer to SCSI unit's data */

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
/* The maxium allocation length is 255 bytes since length must fit in 8 bits */
    if (p == 0 || bufsz < 4 || bufsz > 255)
	return STD_ARGBAD;

/* Fill in the command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = MODE_SELECT;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = bufsz;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = buf;
    p->u_datasz = bufsz;
    p->u_flags |= SF_WRITE; /* we are writing data! */
    p->u_timeout = 4*SECONDS;

    scsi_cmd(ctlr, unit, IRQ_DISCONNECT);
    return p->u_errstat;
}


/*
**	scsi_mode_sense
**
** Find out what the tape unit can handle.
** NB:  We don't lock the mutex since the most probable caller is the
**      unit_init routine.
*/

errstat
scsi_mode_sense(ctlr, unit, buf, bufsz)
int32	ctlr;
int	unit;
char *	buf;	/* buffer to capture sense data */
int	bufsz;	/* size of buffer minimum is 12 bytes! */
{
    scsi_unit *	p;	/* pointer to SCSI unit's data */

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
/* The maxium allocation length is 255 bytes since length must fit in 8 bits */
    if (p == 0 || bufsz < 12 || bufsz > 255)
	return STD_ARGBAD;

/* Fill in the command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = MODE_SENSE;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = bufsz;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = buf;
    p->u_datasz = bufsz;
    p->u_flags &= ~SF_WRITE; /* we are reading data! */
    p->u_timeout = 4*SECONDS;

/* We do this as a sensing command since it returns variable length data */
    scsi_cmd(ctlr, unit, SENSING);

/*
** If the command succeeded we interpret the data for the customer.
** Otherwise we just leave the fields the way they were and let the
** customer fill in his own defaults.
*/
    if (p->u_errstat == STD_OK)
    {
	if (buf[2] & WRITE_PROTECTED)
	    p->u_flags |= SF_WRITE_PROT;
	else 
	    p->u_flags &= ~SF_WRITE_PROT;
	if (buf[0] >= 11 && buf[3] >= 8)
	{
#ifndef NDEBUG
	    uint32 numblks;

	    numblks = (uint32) ( ((buf[5] & 0xFF) << 16) |
				 ((buf[6] & 0xFF) << 8) |
				  (buf[7] & 0xFF) );
	    /*
	     * If there are more block descriptors and the number of
	     * blocks is non-zero then there is almost certainly more
	     * than one block size and we don't support that.
	     */
	    if (buf[0] >= 19 && numblks != 0)
	    {
		printf("scsi(%d, %d): mode sense:", ctlr, unit);
		printf("device has more than one block size.\n");
	    }
#endif /*NDEBUG*/

	    p->u_blksz = (int32)(buf[9] & 0xFF) << 16 |
			 (int32)(buf[10] & 0xFF) << 8 |
			 (int32)(buf[11] & 0xFF);
	}
    }

    return p->u_errstat;
}


/*
**	Request Sense
**
** If a scsi command returned the error status "check condition" then we
** can ask (in polled mode) for the "sense data".
** Although the standard isn't explicit it is possible that size must be
** at least 4 bytes.  For safety we enforce that.
**
** Before calling this routine you must have locked the mutex for the
** relevant Scsi_ctlr_tab entry!
*/

errstat
scsi_req_sense(ctlr, unit)
int32	ctlr;		/* address of scsi controller */
int	unit;		/* unit on scsi controller */
{
    scsi_unit *	p;

    DPRINTF(0, ("scsi_req_sense(ctlr %d, unit %d)\n", ctlr, unit));
    if ((p = Scsi_ctlr_tab[ctlr].c_unit[unit]) == 0)
	return STD_ARGBAD;

/* Build "request sense" command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = REQUEST_SENSE;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = (uint8) SCSI_MAX_SENSE_SIZE;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_timeout = 4*SECONDS;
    p->u_flags &= ~SF_WRITE;
/*
** We have to fill in the next two since the sense data is picked up in the
** data phase
*/
    p->u_data = p->u_sense;
    p->u_datasz = SCSI_MAX_SENSE_SIZE;

/* Send "request sense" command */
    scsi_cmd(ctlr, unit, SENSING);

/*
** At the end p->u_datasz tells how many bytes were still unread so the
** number of sense bytes read is calculated as follows
*/
    p->u_sensesz = SCSI_MAX_SENSE_SIZE - p->u_datasz;
    return p->u_errstat;
}


/*
**	scsi_analyze_sense
**
** This routine checks to see if the error returned by the "request_sense"
** was truly serious or just a note to tell us that something unimportant
** happened.  It returns a code which the caller can map to an error code
** which its caller can understand.
*/

static struct astab
{
	uint16	add_sense_code;
	char *	add_sense_string;
} add_sense_table[] = {
	{ 0x0000, "No additional sense information" },
	{ 0x0001, "Filemark detected" },
	{ 0x0002, "End-of-partition/medium detected" },
	{ 0x0003, "Setmark detected" },
	{ 0x0004, "Beginning-of-partition/medium detected" },
	{ 0x0005, "End-of-data detected" },
	{ 0x0006, "I/O process terminated" },
	{ 0x0011, "Audio play operation in progress" },
	{ 0x0012, "Audio play operation paused" },
	{ 0x0013, "Audio play operation successfully completed" },
	{ 0x0014, "Audio play operation stopped due to error" },
	{ 0x0015, "No current audio status to return" },
	{ 0x0100, "No index/sector signal" },
	{ 0x0200, "No seek complete" },
	{ 0x0300, "Peripheral device write fault" },
	{ 0x0301, "No write current" },
	{ 0x0302, "Excessive write errors" },
	{ 0x0400, "Logical unit not ready, cause not reportable" },
	{ 0x0401, "Logical unit is in process of becoming ready" },
	{ 0x0402, "Logical unit not ready, initializing command required" },
	{ 0x0403, "Logical unit not ready, manual intervention required" },
	{ 0x0404, "Logical unit not ready, format in progress" },
	{ 0x0500, "Logical unit does not respond to selection" },
	{ 0x0600, "No reference position found" },
	{ 0x0700, "Multiple peripheral devices selected" },
	{ 0x0800, "Logical unit communication failure" },
	{ 0x0801, "Logical unit communication timeout" },
	{ 0x0802, "Logical unit communication parity error" },
	{ 0x0900, "Track following error" },
	{ 0x0901, "Track servo failure" },
	{ 0x0902, "Focus servo failure" },
	{ 0x0903, "Spindle servo failure" },
	{ 0x0A00, "Error log overflow" },
	{ 0x0C00, "Write error" },
	{ 0x0C01, "Write error recovered with auto reallocation" },
	{ 0x0C02, "Write error - auto reallocation failed" },
	{ 0x1000, "Id CRC or ECC error" },
	{ 0x1100, "Unrecovered read error" },
	{ 0x1101, "Read retries exhausted" },
	{ 0x1102, "Error too long to correct" },
	{ 0x1103, "Multiple read errors" },
	{ 0x1104, "Unrecovered read error - auto reallocate failed" },
	{ 0x1105, "L-EC uncorrectable error" },
	{ 0x1106, "Circ unrecovered error" },
	{ 0x1107, "Data resynchronization error" },
	{ 0x1108, "Incomplete block read" },
	{ 0x1109, "No gap found" },
	{ 0x110A, "Miscorrected error" },
	{ 0x110B, "Unrecovered read error - recommend reassignment" },
	{ 0x110C, "Unrecovered read error - recommend rewrite the data" },
	{ 0x1200, "Address mark not found for id field" },
	{ 0x1300, "Address mark not found for data field" },
	{ 0x1400, "Recorded entity  not found" },
	{ 0x1401, "Record not found" },
	{ 0x1402, "Filemark or setmark not found" },
	{ 0x1403, "End-of-data not found" },
	{ 0x1404, "Block sequence error" },
	{ 0x1500, "Random positioning error" },
	{ 0x1501, "Mechanical positioning error" },
	{ 0x1502, "Positioning error detected by read of medium" },
	{ 0x1600, "Data resynchronization mark error" },
	{ 0x1700, "Recovered data with no error correction applied" },
	{ 0x1701, "Recovered data with retries" },
	{ 0x1702, "Recovered data with positive head offset" },
	{ 0x1703, "Recovered data with negative head offset" },
	{ 0x1704, "Recovered data with retries and/or circ applied" },
	{ 0x1705, "Recovered data using previous sector id" },
	{ 0x1706, "Recovered data without ECC - data auto-reallocated" },
	{ 0x1707, "Recovered data without ECC - recommend reassignment" },
	{ 0x1800, "Recovered data with error correction applied" },
	{ 0x1801, "Recovered data with error correction and retries applied" },
	{ 0x1802, "Recovered data - data auto-reallocated" },
	{ 0x1803, "Recovered data with circ" },
	{ 0x1804, "Recovered data with lec" },
	{ 0x1805, "Recovered data - recommend reassignment" },
	{ 0x1900, "Defect list error" },
	{ 0x1901, "Defect list not available" },
	{ 0x1902, "Defect list error in primary list" },
	{ 0x1903, "Defect list error in grown list" },
	{ 0x1A00, "Parameter list length error" },
	{ 0x1B00, "Synchronous data transfer error" },
	{ 0x1C00, "Defect list not found" },
	{ 0x1C01, "Primary defect list not found" },
	{ 0x1C02, "Grown defect list not found" },
	{ 0x1D00, "Miscompare during verify operation" },
	{ 0x1E00, "Recovered id with ECC correction" },
	{ 0x2000, "Invalid command operation code" },
	{ 0x2100, "Logical block address out of range" },
	{ 0x2101, "Invalid element address" },
	{ 0x2200, "Illegal function" },
	{ 0x2400, "Invalid field in CDB" },
	{ 0x2500, "Logical unit not supported" },
	{ 0x2600, "Invalid field in parameter list" },
	{ 0x2601, "Parameter not supported" },
	{ 0x2602, "Parameter value invalid" },
	{ 0x2603, "Threshold parameters not supported" },
	{ 0x2700, "Write protected" },
	{ 0x2800, "Not ready to ready transition (medium may have changed)" },
	{ 0x2801, "Import or export element accessed" },
	{ 0x2900, "Power on, reset or bus device reset occurred" },
	{ 0x2A00, "Parameters changed" },
	{ 0x2A01, "Mode parameters changed" },
	{ 0x2A02, "Log parameters changed" },
	{ 0x2B00, "Copy cannot execute since host cannot disconnect" },
	{ 0x2C00, "Command sequence error" },
	{ 0x2C01, "Too many windows specified" },
	{ 0x2C02, "Invalid combination of windows specified" },
	{ 0x2D00, "Overwite error on update in place" },
	{ 0x2F00, "Commands cleared by another initiator" },
	{ 0x3000, "Incompatible medium installed" },
	{ 0x3001, "Cannot read medium - unknown format" },
	{ 0x3002, "Cannot read medium - incompatible format" },
	{ 0x3003, "Cleaning cartridge installed" },
	{ 0x3100, "Medium format corrupted" },
	{ 0x3101, "Format command failed" },
	{ 0x3200, "No defect spare location available" },
	{ 0x3201, "Defect list update failure" },
	{ 0x3300, "Tape length error" },
	{ 0x3600, "Ribbon, ink or toner failure" },
	{ 0x3700, "Rounded parameter" },
	{ 0x3900, "Saving parameters not supported" },
	{ 0x3A00, "Medium not present" },
	{ 0x3B00, "Sequential positioning error" },
	{ 0x3B01, "Tape position error at beginning-of-medium" },
	{ 0x3B02, "Tape position error at end-of-medium" },
	{ 0x3B03, "Tape or electronic vertical forms unit not ready" },
	{ 0x3B04, "Slew failure" },
	{ 0x3B05, "Paper jam" },
	{ 0x3B06, "Failed to sense top-of-form" },
	{ 0x3B07, "Failed to sense bottom-of-form" },
	{ 0x3B08, "Reposition error" },
	{ 0x3B09, "Read past end of medium" },
	{ 0x3B0A, "Read past beginning of medium" },
	{ 0x3B0B, "Position past end of medium" },
	{ 0x3B0C, "Position past beginning of medium" },
	{ 0x3B0D, "Medium destination element full" },
	{ 0x3B0E, "Medium source element empty" },
	{ 0x3D00, "Invalid bits in identify message " },
	{ 0x3E00, "Logical unit has not self-configured yet" },
	{ 0x3F00, "Target operating conditions have changed" },
	{ 0x3F01, "Microcode has changed" },
	{ 0x3F02, "Changed operating definition" },
	{ 0x3F03, "Inquiry data has changed" },
	{ 0x4000, "RAM failure" },
	{ 0x4100, "Data path failure" },
	{ 0x4200, "Power-on or self-test failure" },
	{ 0x4300, "Message error" },
	{ 0x4400, "Internal target failure" },
	{ 0x4500, "Select or reselect failure" },
	{ 0x4600, "Unsuccessful soft reset" },
	{ 0x4700, "SCSI parity error" },
	{ 0x4800, "Initiator detected error message received" },
	{ 0x4900, "Invalid message error" },
	{ 0x4A00, "Command phase error" },
	{ 0x4B00, "Data phase error" },
	{ 0x4C00, "Logical unit failed self-configuration" },
	{ 0x4E00, "Overlapped commands attempted" },
	{ 0x5000, "Write append error" },
	{ 0x5001, "Write append position error" },
	{ 0x5002, "Position error related to timing" },
	{ 0x5100, "Erase failure" },
	{ 0x5200, "Cartridge fault" },
	{ 0x5300, "Media load or eject failed" },
	{ 0x5301, "Unload tape failure" },
	{ 0x5302, "Medium removal prevented" },
	{ 0x5400, "SCSI to host system interface failure" },
	{ 0x5500, "System resource failure" },
	{ 0x5700, "Unable to recover table-of-contents" },
	{ 0x5800, "Generation does not exist" },
	{ 0x5900, "Updated block read" },
	{ 0x5A00, "Operator request or state change input (unspecified)" },
	{ 0x5A01, "Operator medium removal request" },
	{ 0x5A03, "Operator selected write permit" },
	{ 0x5B00, "Log exception" },
	{ 0x5B01, "Threshold condition met" },
	{ 0x5B02, "Log counter at maximum" },
	{ 0x5B03, "Log list codes exhausted" },
	{ 0x5C00, "RPL status change" },
	{ 0x5C01, "Spindles synchronized" },
	{ 0x5C02, "Spindles not synchronized" },
	{ 0x6000, "Lamp failure" },
	{ 0x6100, "Video acquisition error" },
	{ 0x6101, "Unable to acquire video" },
	{ 0x6102, "Out of focus"},
	{ 0x6200, "Scan head positioning error" },
	{ 0x6300, "End of user area encountered on this track" },
	{ 0x6400, "Illegal mode for this track" },
};

#define	ASTABSZ		(sizeof (add_sense_table) / sizeof (struct astab))

/*
 * For the SCSI-II devices and older devices which anticipated SCSI-II
 * we take a look at the additional sense.
 */

static void
analyze_additional_sense(sense, sz)
char *	sense;	/* Should point to the additional sense size field */
int	sz;	/* # bytes of sense we received */
{
    uint8	assize; /* additional sense size */
    uint16	ascode; /* additional sense code */
    int32	csi;	/* command specific information field */
    int		i;


    assize = *sense++;
    if ((int) assize > sz)
    {
	DPRINTF(0, ("additional sense truncated to %d bytes\n", sz));
	assize = sz;
    }

    if (assize > 0)
    {
	if (assize < 10)
	{
	    DPRINTF(0, ("minimum additional sense not present\n"));
	}
	else
	{
	    csi = *sense++ & 0xff;
	    csi <<= 8;
	    csi |= *sense++ & 0xff;
	    csi <<= 8;
	    csi |= *sense++ & 0xff;
	    csi <<= 8;
	    csi |= *sense++ & 0xff;
	    DPRINTF(0, ("Command specific info contains: %d\n", csi));

	    ascode = *sense++;
	    if (ascode  == 0x40)
	    {
		DPRINTF(0, ("Diagnostic failure on component 0x%x\n",
			*sense++));
	    }
	    else
	    {
		ascode <<= 8;
		ascode |= *sense++;
		/* Go through the table until we find an entry >= ascode */
		for (i = 0; i < ASTABSZ &&
			add_sense_table[i].add_sense_code < ascode; i++)
		    /* do nothing */;

		if (i < ASTABSZ && add_sense_table[i].add_sense_code == ascode)
		{
		    DPRINTF(0, ("%x - %s\n", ascode,
					add_sense_table[i].add_sense_string));
		}
		else
		{
		    DPRINTF(0, ("additional sense code = 0x%x\n", ascode));
		}
	    }
	}

    }
}


int
scsi_analyze_sense(ctlr, unit, infobytes)
int32	ctlr;
int	unit;
int32 *	infobytes;
{
    scsi_unit *	p;
    errstat	error;
    uint8 *	sense;
    uint8	valid;
    uint8	errcode;
    int		sz;
    int		do_additional;

    DPRINTF(0, ("SCSI Sense Analysis: Controller %ld, Unit %d\n", ctlr, unit));
    /*
    ** We assume that this routine is only called by sensible people
    ** and that p is non-null.
    */
    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if ((sz = p->u_sensesz - 1) < 0)
    {
	printf("scsi (%d, %d): scsi_analyze_sense: No sense data returned.\n",
			ctlr, unit);
	return SENSE_HARDWARE_ERR;
    }
    sense = (uint8 *) p->u_sense;

    errcode = *sense++;
    valid = errcode & 0x80;
    errcode &= 0x7F;

    if (errcode < 0x70)
    {
	DPRINTF(0, ("Vendor unique sense: code = 0x%x ", errcode));
	if (valid)
	{
	    uint32	addr;

	    /* Some SCSI I analysis */
	    addr = *sense++ & 0x1F;
	    addr <<= 8;
	    addr |= *sense++;
	    addr <<= 8;
	    addr |= *sense++;
	    sz -= 3;
	    DPRINTF(0, ("logical block address = 0x%x\n", addr));
	}
	while (--sz >= 0)
	    DPRINTF(0, (" %x", (*sense++) & 0xff));
	DPRINTF(0, ("\n"));
	error = SENSE_VENDOR;
    }
    else /* Standard Extended Sense */
    {
	if (sz < 7)
	{
	    printf("scsi_analyze_sense: ctlr %d, unit %d, only received %d sense bytes! (min is 8)\nBytes: %x", ctlr, unit, sz + 1, errcode | valid);
	    *infobytes = -1;
	    while (--sz >= 0)
		DPRINTF(0, (" %x", (*sense++) & 0xff));
	    DPRINTF(0, ("\n"));
	    error = SENSE_VENDOR;
	}
	else
	{
	    uint8	segnum;
	    uint8	sense_key;
	    int32	count;

	    sz -= 7;
	    segnum = *sense++;
	    DPRINTF(0, (" Segment number = 0x%x\n", segnum));

	    sense_key = *sense++;

	    count = *sense++ & 0xff;
	    count <<= 8;
	    count |= *sense++ & 0xff;
	    count <<= 8;
	    count |= *sense++ & 0xff;
	    count <<= 8;
	    count |= *sense++ & 0xff;
	    DPRINTF(0, (" Info bytes contains: %d\n", count));
	    if (valid)
		*infobytes = count;
	    else
		*infobytes = -1;

	    do_additional = 0;

	    /*
	     * Take special action for illegal length error - there may be
	     * residue size info!
	     */
	    if (sense_key & 0x20)
	    {
		DPRINTF(0, ("Incorrect Length Indicator"));
		if (valid)
		    DPRINTF(0, (": residue = 0x%x bytes", count));
		DPRINTF(0, ("\n"));
		return SENSE_LENGTH;
	    }

	    switch (sense_key & 0xF) /* look at the sense key */
	    {
	    case 0:
		error = SENSE_OK;
		DPRINTF(0, ("No sense:"));
		if (sense_key & 0x80)
		{
		    DPRINTF(0, (" Filemark"));
		    error = SENSE_EOF;
		}
		if (sense_key & 0x40)
		{
		    DPRINTF(0, (" End of Medium"));
		    error = SENSE_EOM;
		}
		DPRINTF(0, ("\n"));
		break;

	    case 1:
		DPRINTF(0, ("Recovered Error\n"));
		error = SENSE_RECOVERED;
		break;
	
	    case 2:
		DPRINTF(0, ("Device Not Ready\n"));
		error = SENSE_NOT_READY;
		break;

	    case 3:
		DPRINTF(0, ("Medium Error\n"));
		/* by spacing you can get a medium error */
		if (sense_key & 0x40)
		{
		    DPRINTF(0, (" End of Medium"));
		    error = SENSE_EOM;
		}
		else
		    error = SENSE_MEDIUM_ERR;
		do_additional = 1;
		break;

	    case 4:
		DPRINTF(0, ("Hardware Error\n"));
		error = SENSE_HARDWARE_ERR;
		do_additional = 1;
		break;

	    case 5:
		DPRINTF(0, ("Illegal Request\n"));
		error = SENSE_ILLEGAL_REQ;
		do_additional = 1;
		break;

	    case 6:
		DPRINTF(0, ("Unit Attention - medium changed, or target reset\n"));
		error = SENSE_UNIT_ATN;
		break;

	    case 7:
		DPRINTF(0, ("Data Protect\n"));
		error = SENSE_DATA_PROT;
		break;

	    case 8:
		DPRINTF(0, ("Blank Check\n"));
		error = SENSE_BLANK_CHECK;
		break;
	    
	    case 9:
		DPRINTF(0, ("Vendor Unique\n"));
		error = SENSE_VENDOR;
		do_additional = 1;
		break;

	    case 0xA:
		DPRINTF(0, ("Copy Aborted\n"));
		error = SENSE_COPY_ABORTED;
		do_additional = 1;
		break;

	    case 0xB:
		DPRINTF(0, ("Aborted Command\n"));
		error = SENSE_ABORTED_CMD;
		do_additional = 1;
		break;
	    
	    case 0xC:
		DPRINTF(0, ("Equal\n"));
		error = SENSE_EQUAL;
		do_additional = 1;
		break;

	    case 0xD:
		DPRINTF(0, ("Volume Overflow\n"));
		error = SENSE_VOLUME_OVERFLOW;
		do_additional = 1;
		break;
	    
	    case 0xE:
		DPRINTF(0, ("Miscompare\n"));
		error = SENSE_MISCOMPARE;
		do_additional = 1;
		break;

	    case 0xF:
		DPRINTF(0, ("Reserved Sense Key!!\n"));
		error = SENSE_RESERVED;
		do_additional = 1;
		break;

	    default:
		panic("scsi_analyze_sense: compiler error");
		/*NOTREACHED*/
	    }
#ifndef PRINTF_LEVEL
	    if (do_additional)
#endif
		analyze_additional_sense(sense, sz);
	}

    }
    return error;
}


/*
**	scsi_unit_type
**
** This routine returns whether the device has removeable media or not.
*/

int
scsi_unit_type(devaddr, unit)
long	devaddr;
int	unit;	/* target & logical unit info */
{
    int32	ctlr;	/* SCSI controller */

    ctlr = scsi_dev2ctlr(devaddr);
    return Scsi_ctlr_tab[ctlr].c_unit[unit]->u_remove;
}


/*
**	scsi_devtype
**
** Returns a pointer to a string describing the specified device type.
** It is used by the various unnit_init routines.
*/

char *
scsi_devtype(devtype)
uint8	devtype;
{
    switch (devtype)
    {
    case INQ_DIR_ACCESS:
	return "is a direct access device";

    case INQ_SEQ_ACCESS:
	return "is a sequential access device";
    
    case INQ_PRINTER:
	return "is a printer device";

    case INQ_PROCESSOR:
	return "is a processor device";
    
    case INQ_WRITE_ONCE:
	return "is a write-once, read many device";
    
    case INQ_READ_ONLY:
	return "is a read-only direct access";
    
    default:
	if (devtype > INQ_READ_ONLY && devtype < INQ_NO_DEVICE)
	    return "has an illegal device type";
	if (devtype == INQ_NO_DEVICE)
	    return "has no logical unit present";
	else /* devtype > INQ_NO_DEVICE */
	    return "has a vendor unique device type";
    }
}


/*
**	scsi_status
**
** This provides the sort of data that is requested by the STD_STATUS
** command.  This consists of human readable text returned in the given
** buffer.  The tape driver currently makes use of it although the disk
** driver may eventually have some use for it.
*/

errstat
scsi_status(devaddr, unit, start, end)
long	devaddr;
int	unit;	/* target & logical unit info */
char *	start;
char *	end;
{
    int		target;
    scsi_unit * p;	/* pointer to data for this command */
    char *	bufp;	/* pointer to current position in buffer */
    char	sensebuf[MODESENSE_SIZE];
    int32	info;
    int32	ctlr;	/* SCSI controller */

    ctlr = scsi_dev2ctlr(devaddr);
    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    target = unit >> 3;
    bufp = start;

    if (p == 0 || target < 0 || target > 8)
    {
	bufp = bprintf(bufp, end, "Invalid unit number: %d\n", unit);
	*bufp = '\0';
    }
    else
    {
	mu_lock(&p->u_mtx);
	bufp = bprintf(bufp, end, "Unit %s\n", scsi_devtype(p->u_devtype));
	if (p->u_remove == UNITTYPE_REMOVABLE)
	    bufp = bprintf(bufp, end, "Unit has removable media\n");

	if (scsi_test_unit_ready(ctlr, unit) == STD_OK)
	    bufp = bprintf(bufp, end, "Unit is on line\n");
	else
	{
	    bufp = bprintf(bufp, end, "Unit is off line\n");
	    (void) scsi_req_sense(ctlr, unit);
	    if (scsi_analyze_sense(ctlr, unit, &info) == SENSE_UNIT_ATN)
	    {
		bufp = bprintf(bufp, end,
   "WARNING: medium changed and will be rewound at next read/write attempt!\n");
	    }
	}

	if (scsi_mode_sense(ctlr, unit, sensebuf, MODESENSE_SIZE) != STD_OK)
	    p->u_errstat = STD_OK;

	if (p->u_flags & SF_WRITE_PROT)
	    bufp = bprintf(bufp, end, "Unit is write protected\n");

	if (p->u_devtype == INQ_SEQ_ACCESS)
	{
	    bufp = bprintf(bufp, end, "Current file = %d\n", p->u_filepos);
	    bufp = bprintf(bufp, end, "Current record in file = %d\n",
								p->u_recpos);
	}
	mu_unlock(&p->u_mtx);
    }

/* NB: The tape server requires that the data be null-terminated. */
    *bufp = '\0';

    return STD_OK;
}


/*
**	scsi_info
**
** This provides the sort of data that is requested by the STD_INFO
** command.  This consists of human readable text returned in the given
** buffer.  The tape driver currently makes use of it although the disk
** driver may eventually have some use for it.

It can no doubt be expanded to what is needed at a later date

*/

errstat
scsi_info(devaddr, unit, start, end)
long	devaddr;
int	unit;	/* target & logical unit info */
char *	start;
char *	end;
{
    int		target;
    scsi_unit * p;	/* pointer to data for this command */
    char *	bufp;	/* pointer to current position in buffer */
    int32	ctlr;	/* SCSI controller */

    ctlr = scsi_dev2ctlr(devaddr);
    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    target = unit >> 3;
    bufp = start;

    if (p == 0 || target < 0 || target > 8)
	bufp = bprintf(bufp, end, "Invalid unit number: %d", unit);
    else
    {
	switch (p->u_devtype)
	{
	case INQ_SEQ_ACCESS: /* print tape information */
	    bufp = bprintf(bufp, end, "tape drive", unit);
	    break;

	case INQ_DIR_ACCESS: /* some sort of disk */
	    bufp = bprintf(bufp, end, "disk drive", unit);
	    break;
	}
    }

/* NB: The tape server requires that the data be null-terminated. */
    *bufp = '\0';

    return STD_OK;
}


/*
**	scsi_print_inquiry
**
** This routine is used by the unit_init routines to print out the vendor
** dependent device identification information returned by the inquiry command.
** It seems that the first three bytes of data are not for printing.
** Thereafter only about 28 bytes worth is interesting.
*/

void
scsi_print_inquiry(ctlr, unit, buf, sz)
int32	ctlr;
int	unit;
char *	buf;
int	sz;
{
    int	i;
    int	maxbytes;


    maxbytes = (buf[4] & 0xff) + 5;
    if (maxbytes <= 0)
	return;

    if (maxbytes > sz)
	maxbytes = sz;

    printf("scsi %2ld, unit %2d: ", ctlr, unit);
/*
** Because some ancient units are less than helpful in the vendor id field
** we try to say what we can find out about them.  If they do have data we
** just print it.
*/
    if (maxbytes <= 5)
    {
	switch (buf[0] & 0xff)
	{
	case 0:
	    printf("Direct access device");
	    break;
	case 1:
	    printf("Sequential access device");
	    break;
	case 2:
	    printf("Printer device");
	    break;
	case 3:
	    printf("Processor device");
	    break;
	case 4:
	    printf("Write once - read many device");
	    break;
	case 5:
	    printf("Read only direct access device");
	    break;
	default:
	    printf("Illegal device type\n");
	    return;
	}
	if (buf[1] & 0x80)
	    printf(" - removable medium");
	printf("\n");
	return;
    }

/*
** The generic case - they bothered to put an intelligent string in the
** vendor id.
*/
    for(i = 7; i < maxbytes; i++)
	if (isprint(buf[i] & 0xff))
	    printf("%c", buf[i] & 0xff);
    printf("\n");
}
