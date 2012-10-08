/*	@(#)scsi.h	1.11	96/02/27 10:31:03 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_H__
#define __SCSI_H__
/*
**	scsi.h
**
**	This file contains the structures, command codes and error codes
**	used by the scsi interface code - both the disk and tape interfaces
**	plus the low level driver (which is written per machine).
*/

#include "memaccess.h"

/* For disk devices we use a block size of 512 bytes */
#define	BLOCKSIZE	512

#define LUNSHIFT	5
#define CBYTE		0

struct cdb6
{
	uint8	command;
	uint8	lunhadr;
	uint8	madr;
	uint8	ladr;
	uint8	tflen;
	uint8	cbyte;
};

struct cdb10
{
	uint8	command;
	uint8	lunrelb;
	uint8	hhadr;
	uint8	hladr;
	uint8	lhadr;
	uint8	lladr;
	uint8	res;
	uint8	tflenh;
	uint8	tflenl;
	uint8	cbyte;
};

struct cdb12
{
	uint8	command;
	uint8	lunrelb;
	uint8	hhadr;
	uint8	hladr;
	uint8	lhadr;
	uint8	lladr;
	uint8	res0;
	uint8	res1;
	uint8	res2;
	uint8	tflenh;
	uint8	tflenl;
	uint8	cbyte;
};

typedef union Cmd_blk	Cmd_blk;
union Cmd_blk
{
	struct cdb6	c_6;
	struct cdb10	c_10;
	struct cdb12	c_12;
};


/*
** The structure of mode sense/select data blocks is as follows:
**	a struct mode_data followed by 0 or more block descriptors
**	the number of block descriptors is mode_data.bdl / 8.
**	after the block descriptors come optional vendor unique parameters
**	up to (mode_data.length - mode_data.bdl - 1) bytes.
** It doesn't make much sense for a tape to have more than one block descriptor
** but you never can tell. (See sections 9.11 and 9.14 of the standard.)
*/
struct mode_data
{
	uint8	length;
	uint8	type;
	uint8	wbs;	/* write protect/buffer mode/speed */
	uint8	bdl;	/* block descriptor length */
};

#define	WRITE_PROTECTED		0x80
#define	BUFFER_MODE		0x70
#define	SPEED			0x0F

struct block_descriptor
{
	uint8	density;
	uint8	h_numblks; /* most significant byte of numblocks */
	uint8	m_numblks; /* middle significant byte of numblocks */
	uint8	l_numblks; /* least significant byte of numblocks */
	uint8	reserved;
	uint8	h_blk_len; /* most significant byte of block length */
	uint8	m_blk_len; /* middle significant byte of block length */
	uint8	l_blk_len; /* least significant byte of block length */
};

#define	MODESENSE_SIZE		(sizeof (struct block_descriptor) + sizeof (struct mode_data))

/* Tape Density Codes */
#define	DEFAULT			0x00
#define	X3_22_1983		0x01
#define	X3_39_1973		0x02
#define	X3_54_1976		0x03
#define	QIC_11			0x04
#define	X3_136_198X		0x05
#define	X3B5_85_98		0x06
#define	X3_116_198X		0x07
#define	X3B5_85_77		0x08
#define	X3B5_85_76		0x09
#define	AAAAAA			0x0A
#define	X3_56_198X		0x0B
#define	CCCCCC			0x0C
#define	DDDDDD			0x0D


/*
** The next two structures define information that is needed for each
** scsi controller and for each unit on the controller.
** We do not support extended identify and the possibilities for mayhem
** that this provides.  (See section 5.5.6 of the SCSI standard for more
** misunderstanding.)
*/

/*
** To save effort you might like to set the following define to much less than
** the true limit of 64.  Just make sure that your SCSI unit numbers do not
** go past the limit set here.
**  NB. The maximum unit number = target-number * 8 + logical-unit-number
**	so if you have two targets you still need 16 units per controller
**	even though each target may have only one logical unit attached.
*/
#define MAX_UNITS_PER_CTLR	64

/*
** The maximum amount of data obtainable from a request sense command is
** theoretically 256 bytes.  Replies can be bigger (264 bytes) but the command
** block limits the buffer size to 255 bytes.
*/
#define	SCSI_MAX_SENSE_SIZE	255

#define	SCSI_MAX_MESG_SIZE	260

/*
** u_mtx      -	Each logical unit can handle at most one command at a time
**		so we need a mutex to prevent doubling up.
** u_ctlr     -	Index in controller table  of this unit's scsi controller.
** u_unit     -	The number of this unit.
** u_filepos  -	For tapes we keep track of the number of file marks between
**		the start of the tape and the current position.
** u_recpos   -	For tapes we keep track of the number of blocks (= records)
**		between the current position and the start of the tape.
** u_flags    -	Records things like whether the device is write protected
**		and whether a command is to be a read or a write.
**	The following four are the pointers described in section 5.4 of
**	the standard.  Instead of saving and restoring them we just keep
**	them per unit and let only one command be outstanding per unit.
** u_cmd      -	The actual command block sent for this command.
** u_cmdsz    -	Size of command block (6, 10 or 12)
** u_data     -	The data pointer
** u_datasz   -	Number of bytes data still to send.
** u_initsz   - Initial size of data transfer.  This field is not used by
**		the low level SCSI driver but is only filled in by the high
**		level routines (eg. disk_rdwr in sd.c) that need this value
**		after the command is complete.
** u_errstat  -	This gets set to the error status of the command.  This
**		will typically be set in an interrupt routine.
** u_sense    -	An array to hold sense data obtained from a device.
**		(We round it up to 256 bytes for alignment purposes.)
** u_sensesz  -	# bytes of sense data returned in sense array
** u_devtype  - This is the value returned by the INQUIRY command at unit
**		initialisation.  Some commands are only valid for certain
**		types of device.
** u_remove   - Records whether the device has removable media.
** u_minblksz -	Records the minimum block size.
** u_maxblksz -	Records the maximum block size.
** u_blksz    -	Records the actual block size currently in use.
** u_timeout  -	The maximum time (in decis) to wait for the command to execute.
**		If it doesn't finish within this time then it is aborted.
** u_nxtdcon  - The next unit in the list of disconnected units.  The list head
**		is in the controller data structure.
** u_syn_tp
** u_syn_off  -	Transfer period and REQ/ACK offset for synchronous SCSI.
**
**	Note that some things are volatile since they can be changed at
**	interrupt level under the nose of the routines using them.
*/

typedef struct scsi_unit	scsi_unit;
struct scsi_unit
{
		mutex		u_mtx;
		int32		u_ctlr;
		int32		u_unit;
		int32		u_filepos;
		int32		u_recpos;
    volatile	int32		u_flags;
		Cmd_blk		u_cmd;
		int		u_cmdsz;
    volatile	char * volatile	u_data;
    volatile	int32		u_datasz;
		int32		u_initsz;
    volatile	errstat		u_errstat;
		int8 volatile	u_sense[SCSI_MAX_SENSE_SIZE+1];
		int8		u_sensesz;
		uint8		u_devtype;
		uint8		u_remove;
		int32		u_minblksz;
		int32		u_maxblksz;
		int32		u_blksz;
		interval	u_timeout;
    volatile	scsi_unit *	u_nxtdcon;
		uint8		u_syn_tp;
		uint8		u_syn_off;
};


/* Since timeouts are in milli-seconds and we want seconds */
#define	SECONDS		1000

/* various flags per unit */
#define	SF_BUSY		0x0001	/* set if there is a command active */
#define	SF_DISCON	0x0002	/* set if an active command is disconnected */
#define	SF_CAN_DISCON	0x0004	/* set if unit can do disconnects */
#define	SF_UNIT_AVAIL	0x0008	/* set if the unit is available */
#define	SF_MEDIUM_INIT	0x0010	/* set if medium is initialized */
#define	SF_WRITE	0x0020	/* set if I/O is a write to scsi bus */
#define	SF_DMA_WANTED	0x0040	/* set if DMA has been started */
#define	SF_WRITE_PROT	0x0080	/* set if the device is write protected */
#define	SF_DISCONMESG	0x0100	/* set if a disconnect mesg came in */
#define	SF_DONE		0x0200	/* set if the command has had a wakeup */
#define	SF_NEGSYNC	0x0400	/* set if synchronous negotiation required */
#define	SF_SELECTING	0x0800	/* set if selecting */

/*
** c_mtx      - A lock to prevent concurrent update to the rest of this
**		data structure or to the hardware registers.
** c_type     -	The type of controller device.
** c_addr     -	This is the device address of the SCSI controller.  It is
**		normally only useful for machines with multiple controllers.
** c_initted  -	Set when the controller is initialised.
** c_vecinfo  -	This holds the return of setvec so that we can see in the
**		interrupt routine which controller interrupted.
** c_lbolt    - This holds the time after which the next operation on the
**		SCSI bus may occur.  The reason for this is that sometimes
**		the bus must be reset at interrupt level and we cannot sleep
**		for the required delay (of 4 seconds) at interrupt level.
** c_current  -	Points to the unit that is currently being addressed by the
**		SCSI device. (Only relevant for interrupt driven commands.)
** c_unit     - Information about the various units attached to the controller.
**		If the pointer is NULL the device has not been successfully
**		initialised.
** c_discon   -	Linked list of disconnected units waiting for a reconnect.
** c_state    - Current state of SCSI finite state machine.
** c_minperiod-	Minimum period for synchronous transfer.
** c_max_so   -	Maximum synchronous offset this controller can manage.
** c_lmibuf   -	Buffer containing the last message in from a target.
** c_lmioff   -	Current offset in buffer.
**
**	Note that some things are volatile since they can be changed at
**	interrupt level under the nose of the routines using them.
*/

typedef struct scsi_ctlr	scsi_ctlr;
struct scsi_ctlr
{
    volatile	int			c_mtx;
		int			c_type;
		int32			c_addr;
		int			c_initted;
		int			c_vecinfo;
    volatile	interval		c_lbolt;
    volatile	scsi_unit * volatile	c_current;
		scsi_unit *		c_unit[MAX_UNITS_PER_CTLR];
    volatile	scsi_unit * volatile	c_discon;
    volatile	int			c_state;
		int			c_minperiod;
		int			c_max_so;
    volatile	uint8 *			c_lmibuf;
    volatile	int			c_lmioff;
};

/* SCSI Command Codes */
/**********************/

/* Group 0 command codes - generic */
#define	TEST_UNIT_READY		0x00
#define	REQUEST_SENSE		0x03
#define	INQUIRY			0x12
#define MODE_SELECT		0x15
#define	MODE_SENSE		0x1A
#define	REC_DIAG		0x1C
#define	SEND_DIAG		0x1D

/* Group 0 command codes for direct access devices */
#define REZERO			0x01
#define FORMAT			0x04
#define REASSIGN_BLKS		0x07
#define	READ			0x08
#define WRITE			0x0A
#define	SEEK			0x0B
#define	RESERVE			0x16
#define	RELEASE			0x17
#define	START_STOP_UNIT		0x1B

/* Group 0 command codes for sequential access devices */
#define	TP_REWIND		0x01
#define	TP_READ_BLOCK_LIMITS	0x05
#define	TP_READ			0x08
#define	TP_WRITE		0x0A
#define	TP_TRACK_SELECT		0x0B
#define	TP_READ_REVERSE		0x0F
#define	TP_WR_FMARK		0x10
#define	TP_SPACE		0x11
#define	TP_VERIFY		0x13
#define	TP_REC_BUFF_DATA	0x14
#define	TP_RESERVE_UNIT		0x16
#define	TP_RELEASE_UNIT		0x17
#define	TP_ERASE		0x19
#define	TP_LOAD			0x1B
#define	TP_MEDIUM_REMOVAL	0x1E

/* Group 1 command codes for direct access devices */
#define	READ_CAPACITY		0x25
#define READEXT			0x28
#define WRITEXT			0x2A
#define SEEKEXT			0x2B
#define VERIFY			0x2F
#define READ_DEF_DATA		0x37
#define	WRITE_DATA_BUF		0x3B
#define READ_DATA_BUF		0x3C


/* Status byte */
#define STAT_GOOD		0x00
#define STAT_CHECK_CON		0x02
#define STAT_BUSY		0x08
#define STAT_RES_CONFL		0x18

/* SCSI message types */
#define SC_MESS_CC		0x00
#define SC_MESS_EXT		0x01
#define SC_MESS_SDP		0x02
#define SC_MESS_RDP		0x03
#define SC_MESS_DIS		0x04
#define SC_MESS_IDE		0x05
#define SC_MESS_ABO		0x06
#define SC_MESS_REJ		0x07
#define SC_MESS_NOP		0x08
#define SC_MESS_MPE		0x09
#define SC_MESS_LCC		0x0A
#define SC_MESS_LCCF		0x0B
#define SC_MESS_RES		0x0C
#define SC_MESS_IDEN		0x80
/* sub-fields of the IDENTIFY Message */
#define	SCM_DISCON		0x40
#define	SCM_LUNIT_MASK		0x07

/* Extended Message Types */
#define	SC_EXM_MDP		0x00
#define	SC_EXM_SDTR		0x01
#define	SC_EXM_EXT_IDEN		0x02

/* responses to INQUIRY command */
#define	INQ_DIR_ACCESS		0x00
#define	INQ_SEQ_ACCESS		0x01
#define	INQ_PRINTER		0x02
#define	INQ_PROCESSOR		0x03
#define	INQ_WRITE_ONCE		0x04
#define	INQ_READ_ONLY		0x05
#define	INQ_NO_DEVICE		0x7F

/* types for TP_SPACE command */
#define	SPACE_BLOCKS		0
#define	SPACE_FILEMARKS		1
#define	SPACE_SEQFMARKS		2
#define	SPACE_END_DATA		3

/* types for TP_ERASE command */
#define	ERASE_SHORT		0
#define	ERASE_LONG		1

#define	HOST_ID			0x80
#define	TAPE_ID			0x04
#define INQUIRY_BUFSZ		56

/* # times to retry a select due to deafness of the controller after select */
#define	SELECT_RETRIES		5

/* Flags for the irq parameter of scsi_cmd */
#define	IRQ_DISCONNECT		0
#define	POLLED			1
#define	SENSING			2

/* Timeout (in deciseconds) on a scsi command before it is killed off */
#define	MAX_SCSI_WAIT		((interval) 20)

/* SCSI error codes returned by the driver */
#define SCSI_FIRSTERR		UNREGISTERED_FIRST_ERR

#define	SERR_AGAIN		(SCSI_FIRSTERR)
#define	SERR_CHECKCON		(SCSI_FIRSTERR-1)
#define	SERR_CMDFAIL		(SCSI_FIRSTERR-2)
#define	SERR_HANGING		(SCSI_FIRSTERR-3)
#define	SERR_NODEV		(SCSI_FIRSTERR-4)
#define	SERR_SELFAIL		(SCSI_FIRSTERR-5)
#define SERR_LOSTARB		(SCSI_FIRSTERR-6)
#define SERR_PHASELOST		(SCSI_FIRSTERR-7)
#define SERR_PHASEWAIT		(SCSI_FIRSTERR-8)
#define SERR_BADPHASE		(SCSI_FIRSTERR-9)
#define SERR_IOFAIL		(SCSI_FIRSTERR-10)
#define	SERR_RESELECT_PENDING	(SCSI_FIRSTERR-11)


/*
** The following is used by scsi_analyze_sense to map the sense code onto the
** error code desired by the caller.
** Different servers require different error codes in Amoeba.
*/

#define	SENSE_OK		0
#define	SENSE_VENDOR		1
#define	SENSE_EOF		2
#define	SENSE_EOM		3
#define	SENSE_LENGTH		4
#define	SENSE_RECOVERED		5
#define	SENSE_NOT_READY		6
#define	SENSE_MEDIUM_ERR	7
#define	SENSE_HARDWARE_ERR	8
#define	SENSE_ILLEGAL_REQ	9
#define	SENSE_UNIT_ATN		10
#define	SENSE_DATA_PROT		11
#define	SENSE_BLANK_CHECK	12
#define	SENSE_COPY_ABORTED	13
#define	SENSE_ABORTED_CMD	14
#define	SENSE_EQUAL		15
#define	SENSE_VOLUME_OVERFLOW	16
#define	SENSE_MISCOMPARE	17
#define	SENSE_RESERVED		18


/* external routines from scsi driver */
extern void	scsi_cmd();
extern errstat	scsi_ctlr_init();

/* external routines from scsi common code */
extern errstat	scsi_req_sense();
extern errstat	scsi_test_unit_ready();
extern errstat	scsi_start_stop_unit();
extern errstat	scsi_inquiry();
extern errstat	scsi_mode_sense();
extern errstat	scsi_mode_select();
extern void	scsi_print_inquiry();
extern char *	scsi_devtype();
extern int	scsi_analyze_sense();

#endif /* __SCSI_H__ */
