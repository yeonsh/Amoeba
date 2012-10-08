/*	@(#)82072.h	1.2	94/04/06 09:29:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__82072_H__
#define	__82072_H__

/*
 * Register & command information for Intel 82072 Floppy Controller
 *
 * Author: Gregory J. Sharp, April, May-June 1993
 */

#define	__BIT(a)	(1 << (a))

/*
 * Note that writes to the address of the main status register go to the
 * data rate select register.
 */
struct fdc_reg
{
    volatile unsigned char	fdc_status;
    volatile unsigned char	fdc_data;
};

/* Read bits of the main status register */
#define	FRS_RQM			__BIT(7)	/* Request for master */
#define	FRS_DIO			__BIT(6)	/* Data I/O direction */
#define	FRS_EXM			__BIT(5)	/* Execution Mode */
#define	FRS_BUSY		__BIT(4)	/* Command in progress */

/* Write bits of the "main status register" (data-rate select register) */
#define	FRS_SWR			__BIT(7)	/* Software reset */

/* Commands and their parameters for the data register */
#define	F_MT			__BIT(7)	/* Multi-track mode */
#define	F_MFM			__BIT(6)	/* FM/MFM select */
#define	F_SK			__BIT(5)	/* Skip deleted sectors */
#define	F_DIR			__BIT(6)	/* Direction of relative seek */

#define	F_INVALID_CMD		0x01	/* ??? It appears to be unassigned */
#define	F_READ_TRACK		0x02	/* + MFM */
#define	F_SPECIFY		0x03
#define	F_SENSE_DRIVE_STATUS	0x04
#define	F_WRITE_DATA		0x05	/* + MT + MFM */
#define	F_READ_DATA		0x06	/* + MT + MFM + SK */
#define	F_RECALIBRATE		0x07
#define	F_SENSE_INTR_STATUS	0x08
#define	F_WRITE_DELETED		0x09	/* + MT + MFM */
#define	F_READ_ID		0x0A	/* + MFM */
#define	F_READ_DELETED		0x0C	/* + MT + MFM + SK */
#define	F_FORMAT_TRACK		0x0D	/* + MFM */
#define	F_DUMPREG		0x0E
#define	F_SEEK			0x0F
#define	F_VERSION		0x10
#define	F_CONFIGURE		0x13
#define	F_VERIFY		0x16	/* + MT + MFM + SK */
#define	F_RELATIVE_SEEK		0x8F	/* + DIR */


/* Optional bits in byte 2 of configure command */
#define	EIS			__BIT(6)	/* Enable implied seeks */
#define	EFIFO			__BIT(5)	/* Enable fifo */
#define	NOPOLL			__BIT(4)	/* Disable polling of drives */
#define	FIFOTHR			0x0F		/* Mask fifo threshold */

/* For read/write command parameters */
#define	SS_128			0	/* Sector size arguments */
#define	SS_256			1
#define	SS_512			2
#define	SS_1024			3
#define	DTL			0xFF	/* no variable length sectors */
#define	GPL			0x1B	/* Gap3 for the read/write commands */
#define	GAP3			0x54	/* Gap3 for format command */

/* Shift factors to convert block count to byte count & vice versa */
#define	SHIFT_128		7
#define	SHIFT_256		8
#define	SHIFT_512		9
#define	SHIFT_1024		10

/* Status register 0 */
#define	ST0_IC			(__BIT(6)|__BIT(7))	/* Interrupt code */
#define	ST0_SEEKEND		__BIT(5)
#define	ST0_EQUIP_CHK		__BIT(4)
#define	ST0_NOT_READY		__BIT(3)
#define	ST0_HEAD		__BIT(2)
#define	ST0_UNIT		(__BIT(1)|__BIT(0))

/* Status register 1 */
#define	ST1_ENDCYL		__BIT(7)
#define	ST1_DATA_ERROR		__BIT(5)
#define	ST1_OVER_RUN		__BIT(4)
#define	ST1_NO_DATA		__BIT(2)
#define	ST1_NOT_WRITE		__BIT(1)	/* Not writable */
#define	ST1_ADDR_MARK		__BIT(0)	/* Missing address mark */

/* Status register 2 */
#define	ST2_CTL_MARK		__BIT(6)
#define	ST2_DEDF		__BIT(5)	/* Data error in data field */
#define	ST2_WRONG_CYL		__BIT(4)
#define	ST2_SEH			__BIT(3)	/* Scan equal hit */
#define	ST2_SNS			__BIT(2)	/* Scan not satisfied */
#define	ST2_BADCYL		__BIT(1)
#define	ST2_MAMDF		__BIT(0)	/* Missing address mark in
						   data field */
/* Status register 3 */
#define	ST3_FAULT		__BIT(7)
#define	ST3_WRITE_PROT		__BIT(6)
#define	ST3_READY		__BIT(5)
#define	ST3_TRACK0		__BIT(4)
#define	ST3_TWO_SIDE		__BIT(3)
#define	ST3_HEAD		__BIT(2)
#define	ST3_UNIT		(__BIT(1)|__BIT(0))

#endif	/* __82072_H__ */
