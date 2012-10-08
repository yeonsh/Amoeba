/*	@(#)st.c	1.14	96/02/27 13:50:02 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	st.c
**
**	This file contains a generic SCSI tape interface for talking to
**	cartridge tapes, including DAT tapes and EXABYTE tapes.
**
**	There are two type of routine defined here.  The staticly defined
**	routines whose names begin with st_do do not lock mutexes and can
**	be called at any time.  The externally visible routines are those used
**	by the tape server and are responsible for locking the unit before
**	using it.  They are also responsible for passing only sensible
**	controller/unit combinations.
**
**	Author:   Greg Sharp, Aug 1990 & Feb 1991
**	Modified: Greg Sharp, Jul 1995 -
**			1. corrected problems with return value of st_rdwr
**			   after an error.
**			2. allowed for the UNIT_ATN error by a load.
**			3. if wrong block size is used to read from the tape
**			   then the correct error is returned and the amount
**			   actually read is correctly returned.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "tape/tape.h"
#include "disk/conf.h"
#include "scsi.h"
#include "module/mutex.h"
#include "stdlib.h"
#include "st.h"
#include "sys/debug.h"
#include "assert.h"
INIT_ASSERT


/*
** The following is set when we know that the medium initialization
** has been done and the medium parameters are valid.  It must be
** cleared if we get a unit attention error and set in scsi_medium_init
** if it completed successfully.
*/

static errstat	Sense_map[] =
{
	/* Tape sense mapping - see scsi.h for details */
	STD_OK, TAPE_CMD_ABORTED, TAPE_EOF, TAPE_EOT, TAPE_REC_DAT_TRUNC,
	STD_OK, STD_NOMEDIUM, TAPE_MEDIA_ERR, TAPE_CMD_ABORTED,
	TAPE_CMD_ABORTED, TAPE_POS_LOST, STD_WRITEPROT, TAPE_CMD_ABORTED,
	TAPE_CMD_ABORTED, SERR_AGAIN, STD_OK, TAPE_MEDIA_ERR,
	TAPE_CMD_ABORTED, TAPE_CMD_ABORTED
};

extern scsi_ctlr	Scsi_ctlr_tab[];
extern int32		scsi_dev2ctlr();


		/* Internal Routines */
		/*********************/


/*
**	st_dordwr
**
** Issue tape read/write commands.
** See sections 9.3 & 9.4 of the SCSI standard
**
** NOTE: the type bufsize is presently 16 bits.  When that is
**	 upgraded the test for size > 30000 should become size > 2**23
**	 and the cmd6.madr = 0 must change.
*/

static errstat
st_dordwr(cmd, p, unit, size, buf)
uint8		cmd;		/* TP_READ or TP_WRITE */
scsi_unit *	p;		/* pointer to data for this command */
int		unit;		/* target/logical unit to use */
bufsize		size;		/* size in bytes of data to be read/written */
bufptr   	buf;		/* place to get/put the data */
{
    int		fixed_blksz;
    int		count;

    if (size > 30000 || (cmd != TP_READ && cmd != TP_WRITE))
	return STD_ARGBAD;

/* Check for fixed block size mode! (Assume block size is a power of 2!) */
    if (p->u_blksz != 0)
    {
    /* the size must be a multiple of the block size! */
	if ((size & (p->u_blksz - 1)) != 0)
	    return STD_ARGBAD;
    /* convert size in bytes to a block count */
	fixed_blksz = 1;
	count = (int) size / p->u_blksz;
    }
    else
    {
	fixed_blksz = 0;
	count = size;
    }

    if (cmd == TP_WRITE)
	p->u_flags |= SF_WRITE;
    else
	p->u_flags &= ~SF_WRITE;
    p->u_timeout = 2*60*SECONDS; /* 2 minutes */

/* fill in command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = cmd;
    p->u_cmd.c_6.lunhadr = (uint8) (((unit & 07) << LUNSHIFT) | fixed_blksz);
    p->u_cmd.c_6.madr = (int8) (count >> 16);
    p->u_cmd.c_6.ladr = (int8) (count >> 8);
    p->u_cmd.c_6.tflen = (int8) count;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = buf;
    p->u_datasz = size;

    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    return p->u_errstat;
}


/*
**	st_doload
**
** This command causes the medium to be (un)loaded.  It assumes that the
** mutex is locked and that the controller and unit are valid.
** See SCSI standard section 9.15
*/

static errstat
st_doload(p, unit, direction, retension, immediate)
scsi_unit *	p;		/* pointer to SCSI unit's data */
int		unit;
int		direction;	/* 1 = load, 0 = unload */
int		retension;	/* 0 = no retension operation */
int		immediate;	/* 1 = don't wait for completion, 0 = wait */
{
/* The direction, immediate & retension parameters must be 0 or 1 */
    if ((direction & ~1) || (retension & ~1) || (immediate & ~1))
	return STD_ARGBAD;

/* Fill in the command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_LOAD;
    p->u_cmd.c_6.lunhadr = (uint8) (((unit & 07) << LUNSHIFT) | immediate);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = (int8) ((retension << 1) || direction);
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_flags &= ~SF_WRITE;
    p->u_timeout = 5*60*SECONDS;

/* Even if the unload was executed we set the position to start of tape */
    p->u_recpos = 0;
    p->u_filepos = 0;
    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    return p->u_errstat;
}


/*
**	st_doerase
**
** This command erases the tape from the current position to the end.
** See SCSI standard section 9.13
*/

static errstat
st_doerase(p, unit, length)
scsi_unit *	p;	/* pointer to SCSI unit's data */
int		unit;
int		length;	/* amount of tape to erase */
{
/* The length parameter must be 0 (short piece) or 1 (rest of tape) */
    if (length & ~1)
	return STD_ARGBAD;

/* Fill in the command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_ERASE;
    p->u_cmd.c_6.lunhadr = (uint8) (((unit & 07) << LUNSHIFT) | length);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = 0;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_flags &= ~SF_WRITE;
    p->u_timeout = 8*60*SECONDS; /* 8 minute timeout */

/* The standard says that medium position is undefined after an erase so .. */
    p->u_recpos = -1;
    p->u_filepos = -1;
    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    return p->u_errstat;
}


/*
**	st_dospace
**
** used to skip blocks, filemarks, etc.
** See section 9.8 of the SCSI standard.
*/

static errstat
st_dospace(p, unit, count, type)
scsi_unit *	p;
int		unit;		/* target plus logical unit */
int32		count;		/* number of things to skip */
int		type;		/* type of things to skip */
{
    int32	info;

/* fill in command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_SPACE;
    p->u_cmd.c_6.lunhadr = (uint8) (((unit & 07) << LUNSHIFT) | type);
    p->u_cmd.c_6.madr = (int8) (count >> 16);
    p->u_cmd.c_6.ladr = (int8) (count >> 8);
    p->u_cmd.c_6.tflen = (int8) count;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_flags &= ~SF_WRITE;
    p->u_timeout = 5*60*SECONDS; /* 5 minutes */

    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
/*
** We need to pick up the status info from the check condition to see if
** it tells us how many records were not skipped, etc.
*/
    if (p->u_errstat == STD_OK)
    {
	if (type == SPACE_BLOCKS)
	    p->u_recpos += count;
	else
	    if (type == SPACE_FILEMARKS || type == SPACE_SEQFMARKS)
	    {
		p->u_filepos += count;
		p->u_recpos = 0;
	    }
    }
    else /* We are unable to determine the tape position now! */
    {
	if (p->u_errstat == SERR_CHECKCON)
	{
	    errstat err;

	    if ((err = scsi_req_sense(p->u_ctlr, unit)) != STD_OK)
	    {
		printf("st_dospace: couldn't get sense for unit %d", unit);
		printf(" on controller %lx (err = %d)\n", p->u_ctlr, err);
	    }
	    else
	    {
		int code;

		code = scsi_analyze_sense(p->u_ctlr, unit, &info);
		if (code != SENSE_ILLEGAL_REQ && code != SENSE_OK &&
						    code != SENSE_RECOVERED)
		{
		    p->u_recpos = -1;  /* unknown? */
		    p->u_filepos = -1;
		}
		p->u_errstat = Sense_map[code];
	    }
	}
	else /* we don't know where the tape stopped */
	{
	    p->u_recpos = -1;  /* unknown? */
	    p->u_filepos = -1;
	}
    }

    return p->u_errstat;
}


/*
**	st_read_blk_limits
**
** Get the minimum and maximum tape block sizes.
*/

static errstat
st_read_blk_limits(p, unit)
scsi_unit *	p;
int     	unit;
{
    char	data[6];

/* fill in command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_READ_BLOCK_LIMITS;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = 0;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = data;
    p->u_datasz = 6;
    p->u_flags &= ~SF_WRITE;
    p->u_timeout = 4*SECONDS;

    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    if (p->u_errstat == STD_OK)
    {
	p->u_minblksz = (data[4] << 8) | (data[5] & 0xFF);
	p->u_maxblksz = (data[1] << 16) | (data[2] << 8) | (data[3] & 0xFF);
    }
    return p->u_errstat;
}


/*
**	st_dorewind
**
** Rewinds the tape to the beginning-of-tape mark
*/

static errstat
st_dorewind(p, unit, imm_flag)
scsi_unit *	p;		/* pointer to data for this command */
int		unit;		/* target plus logical unit */
int		imm_flag;	/* set if should return immediately without
				** waiting for the rewind to complete */
{
    if (imm_flag)
	imm_flag = 1;
    p->u_timeout = 5*60*SECONDS; /* 5 minutes */

/* fill in command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_REWIND;
    p->u_cmd.c_6.lunhadr = (uint8) (((unit & 07) << LUNSHIFT) | imm_flag);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = 0;
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_flags &= ~SF_WRITE;

    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    if (p->u_errstat == STD_OK)
    {
	p->u_recpos = 0;
	p->u_filepos = 0;
    }
    else /* We are unable to determine the tape position now! */
    {
	p->u_recpos = -1;  /* unknown? */
	p->u_filepos = -1;
    }
    return p->u_errstat;
}


/*
**	st_medium_init
**
** This routine is used to check that a unit is ready (medium is loaded) and
** then get the block limits, block size and write protect information about
** the medium.  Must also rewind the little beastie.
*/

static errstat
st_medium_init(p, unit)
scsi_unit *	p;
int		unit;
{
    int		i;
    errstat	err;
    errstat	err2;
    char	buf[MODESENSE_SIZE];
    int32	info;

    DPRINTF(0, ("st_medium_init(ctlr %d, unit %d)\n", p->u_ctlr, unit));
/*
** See if it is on-line, active or whatever it needs to be.  Note that some
** devices need several test_unit_ready commands before they will talk
** to us.  We try a maximum of 5 times with a 4 second delay between
** attempts to give the device time to get ready.
*/
    p->u_flags &= ~SF_MEDIUM_INIT;
    for (i = 0; i < 5; i++)
    {
	err = scsi_test_unit_ready(p->u_ctlr, unit);
	if (err == SERR_CHECKCON)
	{
	    if ((err2 = scsi_req_sense(p->u_ctlr, unit)) != STD_OK)
	    {
		printf("st_medium_init: couldn't get sense for unit %d", unit);
		printf(" on controller %lx (err = %d)\n", p->u_ctlr, err2);
	    }
	    else
		err = Sense_map[scsi_analyze_sense(p->u_ctlr, unit, &info)];
	}
	if (err == STD_OK)
	    break;
	(void) await((event) 0, (interval) 4000);
    }

    if (err != STD_OK) /* then unit is not ready */
	return err;

/*
** Now we need to see whether the tape unit will accept variable sized
** blocks so we need to read the block limits.
*/
    if ((err = st_read_blk_limits(p, unit)) != STD_OK)
	return err;

/*
** See what the medium's block size is, if possible.  Otherwise we take
** a default block size and assume that it is not write protected.
** Not all scsi devices implement mode sense.
*/
    DPRINTF(2, ("st_medium_init: doing mode sense "));
    if (scsi_mode_sense(p->u_ctlr, unit, buf, MODESENSE_SIZE) != STD_OK)
    {
	p->u_blksz = BLOCKSIZE;
	p->u_flags &= ~SF_WRITE_PROT;
	(void) scsi_req_sense(p->u_ctlr, unit); /* clear the sense info */
    }
    DPRINTF(2, ("write prot = %d\n", p->u_flags & SF_WRITE_PROT));
/*
** It seems that some sporting types have made a fixed block size tape unit
** which then reports a blocksize of 0 (ie. variable) in the mode sense cmd.
** To simplify testing for variable block size we force u_blksz to be non-zero
** iff the device is in fixed block size mode.
*/
    if (p->u_minblksz == p->u_maxblksz)
	p->u_blksz = p->u_maxblksz;

    if (st_dorewind(p, unit, 0) != STD_OK)
    {
	printf("st_medium_init: rewind failed - continuing anyway\n");
	(void) scsi_req_sense(p->u_ctlr, unit); /* clear the sense info */
    }

    p->u_flags |= SF_MEDIUM_INIT;

    return STD_OK;
}


/*
**	st_check_sense
**
** If a scsi tape returns a unit attention error then if it has removable
** media we need to re-initialize our data about it in case it changed.
** After that we can remap the error to a well understood error code.
** NB. It is not safe to call this routine from sd_medium_init!
*/

static errstat
st_check_sense(p, unit, infobytes)
scsi_unit *	p;
int		unit;
int32 *		infobytes;
{
    errstat	err;
    int		sense_err;
    char	buf[MODESENSE_SIZE];

    err = scsi_req_sense(p->u_ctlr, unit);
    if (err != STD_OK)
	return err;
    sense_err = scsi_analyze_sense(p->u_ctlr, unit, infobytes);
    DPRINTF(2, ("st_check_sense: sense err = %d removable = %d\n",
		sense_err, p->u_remove));
    if (sense_err == SENSE_UNIT_ATN && p->u_remove == UNITTYPE_REMOVABLE)
    {
	/*
	 * Either the tape unit was reset or the medium was removed so
	 * we rewind and abort the whole business
	 */
	DPRINTF(2, ("st_check_sense: calling medium init "));
	if ((err = st_medium_init(p, unit)) != STD_OK)
	    return err;
	else
	    return SERR_AGAIN;
    }
    else
    {
	/* Make sure we have set the write-protect flag correctly! */
	if (sense_err == SENSE_DATA_PROT)
	    (void) scsi_mode_sense(p->u_ctlr, unit, buf, MODESENSE_SIZE);
    }
    return Sense_map[sense_err];
}


static errstat
st_rdwr(cmd, ctlr, unit, size, buf)
uint8		cmd;
int32		ctlr;
int		unit;
bufsize *	size;
bufptr   	buf;
{
    errstat	err;
    scsi_unit *	p;
    int32	info;
    bufsize	orig_size;
    int		maxtries;

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;

    orig_size = *size;
    mu_lock(&p->u_mtx);

    /* Iff medium is initialized do we try to do the I/O */
    if ((p->u_flags & SF_MEDIUM_INIT) ||
				    (err = st_medium_init(p, unit)) == STD_OK)
    {
	maxtries = 0;
	do
	{
	    info = -1;
	    err = st_dordwr(cmd, p, unit, orig_size, buf);
	    DPRINTF(2, ("st_rdwr: dordwr returned %d\n", err));
	    if (err == SERR_CHECKCON)
	    {
		err = st_check_sense(p, unit, &info);
		DPRINTF(2, ("st_rdwr: st_check_sense returned %d\n", err));
		DPRINTF(2, ("st_rdwr: write protect = %d, info = %d\n",
				(p->u_flags & SF_WRITE_PROT), info));
		if (err == SERR_CHECKCON)	/* Request sense failed */
		    err = STD_SYSERR;
		else
		{
		    /*
		     * If we got a write-protect error but the rewind
		     * showed that the write protect was now off then
		     * we try again.  Some devices return the wrong error.
		     * They say write-protect even when they should say
		     * unit-attention?!
		     */
		    if (err == TAPE_WRITE_PROT)
		    {
			if(!(p->u_flags & SF_WRITE_PROT))
			    err = SERR_AGAIN;
		    }
		}
	    }
	} while (++maxtries < 2 && err == SERR_AGAIN);

	/* Set the number of bytes actually written */
	if (err != STD_OK)
	{
	    if (info >= 0)
	    {
		if (p->u_blksz != 0)
		    *size -= info * p->u_blksz; /* info is in blocks */
		else
		    *size -= info;
	    }
	    else
		if (err != TAPE_EOT && err != TAPE_EOF)
		{
		    *size = 0; /* We got a bad error */
		    if (err != TAPE_WRITE_PROT)
		    {
			printf("st_rdwr: tape %s error (%d). Size = 0x%lx bytes.\n",
			    (cmd == TP_WRITE) ? "write" : "read", err, orig_size);
		    }
		}
		/* Else leave the size alone - the read/write was done */
	}

	/* Adjust the record/file position information */
	if (err == TAPE_EOF)
	{
	    p->u_filepos++;
	    p->u_recpos = 0;
	}
	else
	{
	    if (err == STD_OK || err == TAPE_EOT || err == TAPE_REC_DAT_TRUNC)
	    {
		if (p->u_blksz != 0)
		    p->u_recpos += (int) orig_size / p->u_blksz;
		else
		    p->u_recpos++;
	    }
	}
    }
    else
	*size = 0; /* st_medium_init failed so we did nothing */

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_dowrite_eof
**
** This command writes a single file mark at the current tape position.
*/

static errstat
st_dowrite_eof(p, unit)
scsi_unit *	p;	/* pointer to data for this command */
int		unit;	/* target and logical unit to address */
{
/* fill in command block */
    p->u_cmdsz = 6;
    p->u_cmd.c_6.command = TP_WR_FMARK;
    p->u_cmd.c_6.lunhadr = (uint8) ((unit & 07) << LUNSHIFT);
    p->u_cmd.c_6.madr = 0;
    p->u_cmd.c_6.ladr = 0;
    p->u_cmd.c_6.tflen = 1;	/* number of file marks to write */
    p->u_cmd.c_6.cbyte = CBYTE;
    p->u_data = (char *) 0;
    p->u_datasz = 0;
    p->u_flags |= SF_WRITE;

    p->u_timeout = 60*SECONDS;

    scsi_cmd(p->u_ctlr, unit, IRQ_DISCONNECT);
    if (p->u_errstat == STD_OK)
    {
	p->u_recpos = 0;
	p->u_filepos++; /* XXX need to look at the error status more closely */
    }
    return p->u_errstat;
}


		/* External Interface Routines */
		/*******************************/

/*
**	st_curr_file_pos
**
** Returns in pos the number of the file it is currently in on the tape.
** ie.  The number returned is the number of file marks between the current
**      position and the start of the tape.
*/

errstat
st_curr_file_pos(devaddr, unit, pos)
long	devaddr;
int	unit;
int32 *	pos;
{
    scsi_unit *	p;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    if (p->u_filepos < 0)
	return TAPE_POS_LOST;
    *pos = p->u_filepos;
    return STD_OK;
}


/*
**	st_curr_rec_pos
**
** Returns in pos the number of the tape blocks between the current position
** and the start of the tape.
*/

errstat
st_curr_rec_pos(devaddr, unit, pos)
long	devaddr;
int	unit;
int32 *	pos;
{
    scsi_unit *	p;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    if (p->u_recpos < 0)
	return TAPE_POS_LOST;
    *pos = p->u_recpos;
    return STD_OK;
}


/*
**	st_file_skip
**
** Causes a skip of N file marks.
** A positive count will skip forwards and leave the tape positioned just
** after the Nth mark.
** A negative count will skip backwards and leave the tape positioned just
** before the Nth previous mark.
*/

errstat
st_file_skip(devaddr, unit, count)
long	devaddr;
int	unit;
int32	count;
{
    scsi_unit *	p;
    errstat	err;
    int32	info;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    if (p->u_filepos < 0)
	return TAPE_POS_LOST;
    if (p->u_filepos + count < 0)
	return STD_ARGBAD;

    mu_lock(&p->u_mtx);
    err = st_dospace(p, unit, count, SPACE_FILEMARKS);
    if (err == SERR_CHECKCON &&
			(err = st_check_sense(p, unit, &info)) == SERR_AGAIN)
	err = st_dospace(p, unit, count, SPACE_FILEMARKS);
    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_record_skip
**
** Causes a skip of N tape blocks.
** A positive count will skip forward and leave the tape positioned just after
** the Nth block.
** A negative count will skip backwards and leave the tape positioned just
** before the Nth previous block.
*/

errstat
st_record_skip(devaddr, unit, count)
long	devaddr;
int	unit;
int32	count;
{
    scsi_unit *	p;
    errstat	err;
    int32	info;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    if (p->u_recpos < 0)
	return TAPE_POS_LOST;
    if (p->u_recpos + count < 0)
	return STD_ARGBAD;

    mu_lock(&p->u_mtx);
    err = st_dospace(p, unit, count, SPACE_BLOCKS);
    if (err == SERR_CHECKCON &&
			(err = st_check_sense(p, unit, &info)) == SERR_AGAIN)
	err = st_dospace(p, unit, count, SPACE_BLOCKS);
    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_rewind
**
** Rewinds the tape to the beginning-of-tape mark
*/

errstat
st_rewind(devaddr, unit, imm_flag)
long	devaddr;
int	unit;		/* target plus logical unit */
int	imm_flag;	/* set if should return immediately without
			** waiting for the rewind to complete */
{
    scsi_unit * p;		/* pointer to data for this command */
    errstat	err;
    int32	info;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];

/* The rewind command is only valid for sequential devices */
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;

    mu_lock(&p->u_mtx);
    err = st_dorewind(p, unit, imm_flag);
    if (err == SERR_CHECKCON)
	err = st_check_sense(p, unit, &info);
    if (err == SERR_AGAIN || err == TAPE_POS_LOST)
	err = st_dorewind(p, unit, imm_flag);
    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_write_eof
**
** This command writes a single file mark at the current tape position.
*/

errstat
st_write_eof(devaddr, unit)
long	devaddr;
int	unit;	/* target and logical unit to address */
{
    scsi_unit * p;		/* pointer to data for this command */
    errstat	err;
    int32	info;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;

    mu_lock(&p->u_mtx);
    err = st_dowrite_eof(p, unit);
    if (err == SERR_CHECKCON &&
			(err = st_check_sense(p, unit, &info)) == SERR_AGAIN)
	err = st_dowrite_eof(p, unit);

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_erase_tape
**
** This routine erases the entire tape and leaves the tape at the beginning
** of tape.
*/

errstat
st_erase_tape(devaddr, unit)
long	devaddr;
int	unit;
{
    errstat	err;
    scsi_unit *	p;
    int32	info;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;

    mu_lock(&p->u_mtx);
/*
** We catch a unit attention on the first rewind and try again but on
** subsequent errors we give up
*/
    err = st_dorewind(p, unit, 0);
    if (err == SERR_CHECKCON &&
			(err = st_check_sense(p, unit, &info)) == SERR_AGAIN)
	err = st_dorewind(p, unit, 0);
    if (err == STD_OK && (err = st_doerase(p, unit, ERASE_LONG)) == STD_OK)
	err = st_dorewind(p, unit, 0);

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_read
**
** Reads one block from the tape, and returns up to a maximum of 'size'
** bytes in 'buf'.  The number of bytes in 'buf' is returned in 'size'
** Since the drive reads 1 record at a time, if the record was larger
** than 'size' bytes the rest of the information in the record will be lost
** A subsequent read will begin at the start of the next tape block.
**
** We used variable length records!  So the tape unit had better be able to
** handle that.
*/

errstat
st_read(devaddr, unit, size, buf)
long		devaddr;
int             unit;
bufsize *       size;
bufptr          buf;
{
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);
    return st_rdwr(TP_READ, ctlr, unit, size, buf);
}


/*
**	st_write
**
** Writes one block of 'size' bytes from 'buf' to tape.
*/

errstat
st_write(devaddr, unit, size, buf)
long		devaddr;
int		unit;
bufsize *	size;
bufptr   	buf;
{
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);
    return st_rdwr(TP_WRITE, ctlr, unit, size, buf);
}


/*
**	st_load
**
** Do a SCSI load command - which should set the medium to beginning of tape
** Thereafter we do a test unit ready, get the block limits and check for
** write protection.  The chances of getting a check condition error (unit
** attention) are quite high so we work hard to catch it.
*/

errstat
st_load(devaddr, unit)
long	devaddr;
int	unit;
{
    errstat	err;
    scsi_unit *	p;
    int32	info;
    int32	ctlr;
    int		sense_err;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);

    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    mu_lock(&p->u_mtx);
    err = st_doload(p, unit, 1, 0, 0);
    /*
     * Since we are loading a tape there is a good chance that we get a unit
     * attention error, but perhaps we will get some other transitory error.
     */
    if (err == SERR_CHECKCON && (err = scsi_req_sense(ctlr, unit)) == STD_OK)
    {
	sense_err = scsi_analyze_sense(ctlr, unit, &info);
	if (sense_err == SENSE_UNIT_ATN ||
	    (err = Sense_map[sense_err]) == SERR_AGAIN)
	{
	    err = st_doload(p, unit, 1, 0, 0);
	}
    }
    if (err == STD_OK)
	err = st_medium_init(p, unit);

    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_unload
**
** Do a SCSI unload command.
*/

errstat
st_unload(devaddr, unit)
long	devaddr;
int	unit;
{
    errstat	err;
    scsi_unit *	p;
    int32	ctlr;

    ctlr = scsi_dev2ctlr(devaddr);
    assert(ctlr >= 0);
    p = Scsi_ctlr_tab[ctlr].c_unit[unit];
    if (p == 0 || p->u_devtype != INQ_SEQ_ACCESS)
	return STD_SYSERR;
    mu_lock(&p->u_mtx);
    err =  st_doload(p, unit, 0, 0, 0);
    mu_unlock(&p->u_mtx);
    return err;
}


/*
**	st_unit_init
**
** This tries to initialise a scsi unit and returns success only if the device
** was a sequential device of some sort (hopefully a tape).
*/

errstat
st_unit_init(devaddr, unit)
long	devaddr;	/* address of scsi controller */
int	unit;		/* unit on scsi controller */
{
    char	buf[INQUIRY_BUFSZ];	/* to receive inquiry answer */
    errstat	err;
    errstat	err2;
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
** Since tapes tend to be a deaf to requests for up to 20 seconds after a
** reset we try several times before giving up and concluding that nothing
** is there.  We sleep for 4 seconds between attempts.
*/
    for (i = 0; i < 10; i++)
    {
	err = scsi_inquiry(ctlr, unit, buf, INQUIRY_BUFSZ);
	if (err == SERR_SELFAIL)
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
	    if ((err2 = scsi_req_sense(ctlr, unit)) != STD_OK)
	    {
		printf(
"st_unit_init: couldn't get sense for unit %d on controller %lx (err = %d)\n",
							    unit, ctlr, err2);
	    }
	    else
	    {
		err = Sense_map[scsi_analyze_sense(ctlr, unit, &info)];
		if (err == STD_OK)
		    break;
	    }
	}
	(void) await((event) 0, (interval) 4000);
    }

    DPRINTF(0, ("st_unit_init: inquiry returned %s\n", err_why(err)));
    if (err != STD_OK || (buf[0] & 0xFF) == INQ_NO_DEVICE)
    {
	printf("st_unit_init: unit %d not present on controller %lx\n",
								    unit, ctlr);
	mu_unlock(&p->u_mtx);
	return SERR_NODEV;
    }
    p->u_devtype = buf[0];
    if (p->u_devtype != INQ_SEQ_ACCESS)
    {
	printf("st_unit_init: controller %lx, unit %d %s!\n",
				    ctlr, unit, scsi_devtype(p->u_devtype));
	printf("Expected %s\n", scsi_devtype(INQ_SEQ_ACCESS));
	mu_unlock(&p->u_mtx);
	return STD_SYSERR;
    }
    p->u_remove = (buf[1] & 0x80) ? UNITTYPE_REMOVABLE : UNITTYPE_FIXED;
    scsi_print_inquiry(ctlr, unit, buf, INQUIRY_BUFSZ);

#ifndef NO_DISCON
    /*
     * The disconnect capability will be removed by the driver if it
     * discovers that the unit can't do it
     */
    p->u_flags |= SF_CAN_DISCON | SF_NEGSYNC;
#endif

    if ((err = st_medium_init(p, unit)) != STD_OK)
    {
	DPRINTF(2, ("st_unit_init: medium init returned %s\n", err_why(err)));
	if (p->u_remove == UNITTYPE_REMOVABLE) /* medium is probably out! */
	    err = STD_OK;
	else
	    err = SERR_NODEV;
    }

    mu_unlock(&p->u_mtx);
    return err;
}
