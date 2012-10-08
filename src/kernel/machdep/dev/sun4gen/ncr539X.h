/*	@(#)ncr539X.h	1.2	96/02/27 13:54:56 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__NCR539X_H__
#define	__NCR539X_H__

/*
 * Include file for the NCR 539X SCSI Controller Chip
 *
 *	The byte accessible registers have been word aligned in the
 *	sun4 so we put pads between each field in the structs.
 *
 *	This file attempts to support the NCR53C90, NCR53C90A/B and the
 *	NCR539X found in the NCR89C100/NCR89C105 chip set.
 *
 * Author:	Gregory J. Sharp, August 1993
 * Modified:	Gregory J. Sharp, July 1994
 *			- made a single file for all supported chip types.
 */


typedef union ncr_539X	ncr_539X;
typedef uint8		_pad[3];
typedef uint8		_pad7[7]; /* between config3 & xfer_hi! */

union ncr_539X
{
    struct
    {
	volatile uint8	sc_xfr_cnt_lo;		_pad	sc_p0;
	volatile uint8	sc_xfr_cnt_mid;		_pad	sc_p1;
	volatile uint8	sc_fifo;		_pad	sc_p2;
	volatile uint8	sc_cmd;			_pad	sc_p3;
	volatile uint8	sc_stat;		_pad	sc_p4;
	volatile uint8	sc_intr_stat;		_pad	sc_p5;
	volatile uint8	sc_seqstep;		_pad	sc_p6;
	volatile uint8	sc_fifoflags;		_pad	sc_p7;
	volatile uint8	sc_config1;		_pad	sc_p8;
	volatile uint8	sc_reserved1;		_pad	sc_p9;
	volatile uint8	sc_reserved2;		_pad	sc_pA;
	volatile uint8	sc_config2;		_pad	sc_pB;
	volatile uint8	sc_config3;		_pad7	sc_pC;
	volatile uint8	sc_xfr_cnt_hi;		_pad	sc_pE;
    } r;
    struct
    {
	volatile uint8	sc_xfr_cnt_lo;		_pad	sc_p0;
	volatile uint8	sc_xfr_cnt_mid;		_pad	sc_p1;
	volatile uint8	sc_fifo;		_pad	sc_p2;
	volatile uint8	sc_cmd;			_pad	sc_p3;
	volatile uint8	sc_selbusid;		_pad	sc_p4;
	volatile uint8	sc_seltimo;		_pad	sc_p5;
	volatile uint8	sc_sync_period;		_pad	sc_p6;
	volatile uint8	sc_sync_offset;		_pad	sc_p7;
	volatile uint8	sc_config1;		_pad	sc_p8;
	volatile uint8	sc_clkcnv;		_pad	sc_p9;
	volatile uint8	sc_test;		_pad	sc_pA;
	volatile uint8	sc_config2;		_pad	sc_pB;
	volatile uint8	sc_config3;		_pad7	sc_pC;
	volatile uint8	sc_xfr_cnt_hi;		_pad	sc_pE;
    } w;
};

#ifndef __bit
#define	__bit(x)	(1 << (x))
#endif

/* Command Register (R/W) */
/**************************/
/* Bit Masks */
#define	NCR_CMD_ENABLE_DMA		__bit(7)

/* Actual Commands for low 7 bits of Command register */
/* Misc */
#define	NCR_CMD_NOP			0x00
#define	NCR_CMD_FLUSH_FIFO		0x01
#define NCR_CMD_RESET_CHIP		0x02
#define NCR_CMD_RESET_BUS		0x03
/* Diconnected Mode */
#define NCR_CMD_RESEL_SEQ		0x40
#define NCR_CMD_SEL_NOATN		0x41
#define NCR_CMD_SEL_ATN			0x42
#define NCR_CMD_SEL_ATN_STOP		0x43
#define NCR_CMD_SEL_ENABLE		0x44
#define NCR_CMD_SEL_DISABLE		0x45
/* Target Mode */
#define NCR_CMD_SEND_MSG		0x20
#define NCR_CMD_SEND_STATUS		0x21
#define NCR_CMD_SEND_DATA		0x22
#define NCR_CMD_DISCON_SEQ		0x23
#define NCR_CMD_TERMINATE_SEQ		0x24
#define NCR_CMD_TCMD_CMPLT_SEQ		0x25
#define NCR_CMD_DISCONNECT		0x27
#define NCR_CMD_RCV_MSG_SEQ		0x28
#define NCR_CMD_RCV_CMD			0x29
#define NCR_CMD_RCV_DATA		0x2A
#define NCR_CMD_RCV_CMD_SEQ		0x2B
/* Initiator Mode */
#define NCR_CMD_XFER_INFO		0x10
#define NCR_CMD_ICMD_CMPLT_SEQ		0x11
#define NCR_CMD_MSG_ACCEPTED		0x12
#define NCR_CMD_XFER_PAD		0x18
#define NCR_CMD_SET_ATN			0x1A
#define NCR_CMD_RESET_ATN		0x1B

/* Status Register (Read Only) */
/*******************************/
#define	NCR_STAT_IO			__bit(0)
#define	NCR_STAT_CD			__bit(1)
#define	NCR_STAT_MSG			__bit(2)
#	define NCR_BP_DATAO	0
#	define NCR_BP_DATAI	NCR_STAT_IO
#	define NCR_BP_CMD	NCR_STAT_CD
#	define NCR_BP_STATUS	(NCR_STAT_CD | NCR_STAT_IO)
#	define NCR_BP_MESGO	(NCR_STAT_MSG | NCR_STAT_CD)
#	define NCR_BP_MESGI	(NCR_STAT_MSG | NCR_STAT_CD | NCR_STAT_IO)
#	define NCR_BP_MASK	(NCR_STAT_MSG | NCR_STAT_CD | NCR_STAT_IO)
#define	NCR_STAT_XFER_CMPT		__bit(3)
#define	NCR_STAT_XFER_ZERO		__bit(4)
#define	NCR_STAT_PARITY_ERR		__bit(5)
#define	NCR_STAT_GROSS_ERR		__bit(6)
/*
 * In smarter chips bit 7 is the interrupt-pending bit.
 * We can't used that, alas.
 */
#define	NCR_STAT_INTR			__bit(7)

/* Interrupt Status Register (Read Only) */
/*****************************************/
#define	NCR_ISTAT_SELECTED		__bit(0)
#define	NCR_ISTAT_SELECTED_ATN		__bit(1)
#define	NCR_ISTAT_RESELECTED		__bit(2)
#define	NCR_ISTAT_FUNC_CMPLT		__bit(3)
#define	NCR_ISTAT_BUS_SERVICE		__bit(4)
#define	NCR_ISTAT_DISCONNECT		__bit(5)
#define	NCR_ISTAT_ILLEGAL_CMD		__bit(6)
#define	NCR_ISTAT_SCSI_RESET_DETECTED	__bit(7)

/* Sequence Step Register (Read Only) */
/**************************************/
#define NCR_SEQUENCE_STEP		0x07
#define	NCR_SEQUENCE_RESERVED		0xF8

/* Synchronous Transfer Period Register (Write Only) */
/*****************************************************/
#define NCR_STP_SYNC_TP			0x1F
#define	NCR_STP_RESERVED		0xE0
#define	NCR_STP_MIN_PERIOD_90		0x05	/* for 5390 & 5390A */
/* For the 9X the minimum period should be 4 but everything hangs
 * if you set it to 4 so we stick to 5 :-( */
#define	NCR_STP_MIN_PERIOD_9X		0x05	/* for 539X chips */
#define	NCR_STP_MAX_PERIOD		0x23

/* Synchronous Transfer Offset Register (Write Only) */
/*****************************************************/
#define	NCR_SO_MAX_OFFSET		0xF

/* FIFO Flags Register (Read Only) */
/***********************************/
#define	NCR_FIFO_FLAGS			0x1F
#define	NCR_FIFO_FLAGS_RESERVED		0xE0	/* NCR5390 restriction */

/* Configuration Register 1 (R/W) */
/**********************************/
#define	NCR_CONF1_BUS_ID			0x07
#define NCR_CONF1_CHIP_TEST_MODE		__bit(3)
#define NCR_CONF1_PARITY_ENABLE			__bit(4)
#define NCR_CONF1_PARITY_TEST_MODE		__bit(5)
#define NCR_CONF1_SCSI_RESET_INT_DISABLE	__bit(6)
#define NCR_CONF1_SLOW_CABLE_MODE		__bit(7)

/* Clock Conversion Register (Write Only) */
/******************************************/
#define	NCR_CLK_CNV_FACTOR		0x07
#define	NCR_CLK_CNV_RESERVED		0xF8

/* Test Register (Write Only) */
/******************************/
#define	NCR_TEST_TARGET_MODE		__bit(0)
#define	NCR_TEST_INITIATOR_MODE		__bit(1)
#define	NCR_TEST_TRISTATE_MODE		__bit(2)

/* Configuration Register 2 (R/W) */
/**********************************/
#define	NCR_CONF2_DMA_PARITY_ENABLE	__bit(0)
#define	NCR_CONF2_REG_PARITY_ENABLE	__bit(1)
#define	NCR_CONF2_TARG_BAD_PARITY_ABORT	__bit(2)
#define	NCR_CONF2_SCSI_2		__bit(3)
#define	NCR_CONF2_DREQ_HI_Z		__bit(4)
#define	NCR_CONF2_RESERVED1		__bit(5)
#define	NCR_CONF2_FEATURES_ENABLE	__bit(6)
#define	NCR_CONF2_RESERVED2		__bit(7)

/* Configuration Register 3 (R/W) */
/**********************************/
#define	NCR_CONF3_FAST_CLK		__bit(0)
#define	NCR_CONF3_FAST_SCSI		__bit(1)
#define	NCR_CONF3_CDB10			__bit(2)
#define	NCR_CONF3_QUEUE_TAG_ENABLE	__bit(3)
#define	NCR_CONF3_MSG_RESERVE_CHECK	__bit(3)
#define	NCR_CONF3_RESERVED		0xE0

/* Transfer Count High Register (R) */
/************************************/
#define	NCR_CHIP_REV_FAMILY		0xF1
#define	NCR_CHIP_REV_LEVEL		0x07


/* Macros for setting SCSI chip registers */
/******************************************/

#define	getb(a)		mem_get_byte((volatile char *)(a))
#define	putb(a, b)	mem_put_byte((volatile char *)(a), (b))
#define	put_OR_b(a, b)	mem_OR_byte((volatile char *)(a), (b))
#define	put_AND_b(a, b)	mem_AND_byte((volatile char *)(a), (b))

#define	getl(a)		mem_get_long((volatile long *)(a))
#define	putl(a, b)	mem_put_long((volatile long *)(a), (long) b)
#define	put_OR_l(a, b)	mem_OR_long((volatile long *)(a), (long) b)
#define	put_AND_l(a, b)	mem_AND_long((volatile long *)(a), (long) b)


/* Macros for geting and setting the SCSI byte count register */
/**************************************************************/

/*
 * We only do up to 64K transfers at a time for compatibility between
 * the various chips
 */

#define	SET_SCSI_BYTE_CNT(s, n, type)	\
    do {								\
	putb(&(s)->w.sc_xfr_cnt_lo,  (char) ((n) & 0xFF));		\
	putb(&(s)->w.sc_xfr_cnt_mid, (char) (((n) >> 8) & 0xFF));	\
	if ((type) == CHIP_NCR539X) {					\
	    putb(&(s)->w.sc_xfr_cnt_hi, (char) (((n) >> 16) & 0xFF));	\
	}								\
    } while (0)

#define	GET_SCSI_BYTE_CNT(s, type)	\
	((getb(&(s)->r.sc_xfr_cnt_lo)  & 0xFF) |		\
	((getb(&(s)->r.sc_xfr_cnt_mid) & 0xFF) << 8) |		\
	(((type) == CHIP_NCR539X) ?				\
	      ((getb(&(s)->r.sc_xfr_cnt_hi) & 0xFF) << 8) : 0))

/* How to tell if a SCSI chip has an interrupt pending */
/*******************************************************/

#define	INTR_PENDING(i)		((i) != 0)
/*
 * In a better world it would be the following, but the 5390 can't so it.
 *   #define	INTR_PENDING(s, i)	((s) & NCR_STAT_INTR)
 * Furthermore, sometimes certain versions of the chip forget to set the
 * interrupt bit in the status register when a reselect occurs.  (At least,
 * that's what it looks like.)
 */

/* Chip specific initialization */
/********************************/

/*
 * The following are used for identifying the various chip types in the
 * driver.
 */
#define	CHIP_NCR5390		0
#define	CHIP_NCR5390A		1
#define CHIP_NCR539X		2

#define	NCR539X_CHIP_INIT(s, clk, chiptype)	\
    do {								\
	switch (chiptype) {						\
	case CHIP_NCR5390:						\
	    break;							\
	case CHIP_NCR5390A:						\
	    putb(&(s)->w.sc_config2, NCR_CONF2_FEATURES_ENABLE);	\
	    break;							\
	case CHIP_NCR539X:						\
	    putb(&(s)->w.sc_config2, NCR_CONF2_FEATURES_ENABLE);	\
	    if ((clk) > 25)						\
		put_OR_b(&(s)->w.sc_config3, NCR_CONF3_FAST_CLK);	\
	    else							\
		put_AND_b(&(s)->w.sc_config3, ~NCR_CONF3_FAST_CLK);	\
	    break;							\
	default:							\
	    panic("unknown SCSI chip type");				\
	}								\
    } while (0)

#define	NCR539X_SETFAST(s, type) \
    do {								\
	if ((type) == CHIP_NCR539X) {					\
		put_OR_b(&(s)->w.sc_config3, NCR_CONF3_FAST_SCSI);	\
	}								\
    } while (0)
#define	NCR539X_CLRFAST(s, type) \
    do {								\
	if ((type) == CHIP_NCR539X) {					\
		put_AND_b(&(s)->w.sc_config3, ~NCR_CONF3_FAST_SCSI);	\
	}								\
    } while (0)

#endif /* __NCR539X_H__ */
