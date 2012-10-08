/*	@(#)82077.h	1.3	94/05/17 11:20:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__82077_H__
#define	__82077_H__

/*
 * Register & command information for Intel 82077 Floppy Controller
 * as implemented in the NCR 89C105 chip set.  Only MFM encoding is
 * supported by this chip and only 720K, 1.44 MB and 2.88 MB densities.
 * The latter is not supported in this driver.
 *
 * Author:
 *	Gregory J. Sharp, Sept 1993
 */

union i82077_reg
{
    struct
    {
	volatile uint8	f_srA;	/* status register A - unused */
	volatile uint8	f_srB;	/* status register B - unused */
	volatile uint8	f_dor;	/* digital output register */
	volatile uint8	f_tdr;	/* tape drive register */
	volatile uint8	f_msr;	/* main status register */
	volatile uint8	f_fifo;
	volatile uint8	f_res1;	/* reserved (NCR test mode enable) */
	volatile uint8	f_dir;	/* digital input register */
    } r;
    struct
    {
	volatile uint8	f_res2;	/* reserved */
	volatile uint8	f_res3;	/* reserved */
	volatile uint8	f_dor;	/* digital output register */
	volatile uint8	f_tdr;	/* tape drive register */
	volatile uint8	f_dsr;	/* data rate select register */
	volatile uint8	f_fifo;
	volatile uint8	f_res4;	/* reserved (NCR test mode enable) */
	volatile uint8	f_ccr;	/* configuration control register */
    } w;
};

#define	__BIT(a)	(1 << (a))

/* Digital Output Register - this is not the same as in the 82077 */
#define	DOR_DRIVE_SELECT	__BIT(0)
#define	DOR_CTLR_RESET		__BIT(2)
#define	DOR_DMAGATE		__BIT(3)
#define	DOR_MOTOR_ENB		__BIT(4)
#define	DOR_DENSITY		__BIT(6)
#    define DOR_DENS_1440		0
#    define DOR_DENS_1200		1
#define	DOR_EJECT		__BIT(7)

/* Main Status Register */
#define	MSR_BSY_DRV0		__BIT(0)
#define	MSR_BSY_DRV1		__BIT(1)
#define	MSR_BSY_DRV2		__BIT(2)
#define	MSR_BSY_DRV3		__BIT(3)
#define	MSR_CMD_BSY		__BIT(4)
#define	MSR_DMA_DISABLE		__BIT(5)
#define	MSR_DIO			__BIT(6)
#define	MSR_RQM			__BIT(7)

/* Data Rate Select Register */
#define	DSR_DATA_RATE_MASK	(__BIT(0) | __BIT(1))
#    define DSR_DR_1MBS			(__BIT(0) | __BIT(1))
#    define DSR_DR_500KBS		0
#    define DSR_DR_300KBS		__BIT(0)
#    define DSR_DR_250KBS		__BIT(1)
#define	DSR_PRECOMP_MASK	(__BIT(2) | __BIT(3) | __BIT(4))
#    define DSR_PRE_OFF			(__BIT(2) | __BIT(3) | __BIT(4))
#    define DSR_PRE_DEFAULT		0
#define	DSR_POWER_DOWN		__BIT(6)
#define	DSR_CTLR_RESET		__BIT(7)

/* Digital Input Register */
#define	DIR_DISK_CHANGED	__BIT(7)

/* Configuration Control Register */
#define	CCR_DRIVE_SEL_MASK	(__BIT(0) | __BIT(1))

/* Commands and their parameters for the data register */
#define	F_MT			__BIT(7)	/* Multi-track mode */
#define	F_MFM			__BIT(6)	/* FM/MFM select */
#define	F_SK			__BIT(5)	/* Skip deleted sectors */
#define	F_DIR			__BIT(6)	/* Direction of relative seek */

#define	F_INVALID_CMD		0x01	/* It appears to be unassigned */
#define	F_READ_TRACK		(0x02 | F_MFM)
#define	F_SPECIFY		0x03
#define	F_SENSE_DRIVE_STATUS	0x04
#define	F_WRITE_DATA		(0x05 | F_MFM | F_MT)
#define	F_READ_DATA		(0x06 | F_MFM | F_MT | F_SK)
#define	F_RECALIBRATE		0x07
#define	F_SENSE_INTR_STATUS	0x08
#define	F_WRITE_DELETED		(0x09 | F_MFM | F_MT)
#define	F_READ_ID		(0x0A | F_MFM)
#define	F_READ_DELETED		(0x0C | F_MFM | F_MT | F_SK)
#define	F_FORMAT_TRACK		(0x0D | F_MFM)
#define	F_DUMPREG		0x0E
#define	F_SEEK			0x0F
#define	F_VERSION		0x10
#define	F_CONFIGURE		0x13
#define	F_VERIFY		(0x16 | F_MFM | F_MT | F_SK)
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

#endif	/* __82077_H__ */
