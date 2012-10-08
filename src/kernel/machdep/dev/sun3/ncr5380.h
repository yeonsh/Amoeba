/*	@(#)ncr5380.h	1.4	94/04/06 09:26:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __NCR_5380_H__
#define __NCR_5380_H__

/*
** The ncr_5380 register set and related constants
*/

typedef union ncr_5380 ncr_5380;

union ncr_5380
{
    struct
    {
	volatile uint8	sc_data;
	volatile uint8	sc_icom;
	volatile uint8	sc_mode;
	volatile uint8	sc_tcom;
	volatile uint8	sc_cbstat;
	volatile uint8	sc_bas;
	volatile uint8	sc_idata;
	volatile uint8	sc_reset;
    } r;
    struct
    {
	volatile uint8	sc_data;
	volatile uint8	sc_icom;
	volatile uint8	sc_mode;
	volatile uint8	sc_tcom;
	volatile uint8	sc_selenb;
	volatile uint8	sc_sdsend;
	volatile uint8	sc_sdtrec;
	volatile uint8	sc_sdirec;
    } w;
};

#define bit(n)	(1 << (n))

/*
** Bits in the initiator command register (sc_icom).
** Bits 5 and 6 vary depending on whether you are reading or writing
*/

#define	NCR_ASSERT_RESET	bit(7)
#define	NCR_AIP			bit(6)	/* read */
#define NCR_TEST_MODE		bit(6)	/* write */
#define NCR_LOST_ARB		bit(5)	/* read */
#define NCR_DIFF_ENBL		bit(5)	/* write */
#define	NCR_ASSERT_ACK		bit(4)
#define NCR_ASSERT_BSY		bit(3)
#define NCR_ASSERT_SEL		bit(2)
#define	NCR_ASSERT_ATN		bit(1)
#define NCR_ASSERT_DBUS		bit(0)

/* Bits in the mode register (sc_mode) */

#define	NCR_BLOCK_DMA		bit(7)
#define	NCR_TARGETMODE		bit(6)
#define	NCR_EN_PAR_CHK		bit(5)
#define	NCR_EN_PAR_INT		bit(4)
#define NCR_EN_EOP_INT		bit(3)
#define NCR_MON_BUSY		bit(2)
#define	NCR_DMA_MODE		bit(1)
#define	NCR_ARBITRATE		bit(0)

/* Bits in target command register (sc_tcom) */

#define NCR_TCOM_REQ		bit(3)
#define NCR_TCOM_MSG		bit(2)
#define NCR_TCOM_CD		bit(1)
#define NCR_TCOM_IO		bit(0)

/* Bits in current SCSI bus status register (sc_cbstat): read only */

#define NCR_CBSR_RST		bit(7)
#define NCR_CBSR_BSY		bit(6)
#define NCR_CBSR_REQ		bit(5)
#define NCR_CBSR_MSG		bit(4)
#define NCR_CBSR_CD		bit(3)
#define NCR_CBSR_IO		bit(2)
#define NCR_CBSR_SEL		bit(1)
#define NCR_CBSR_DBP		bit(0)

#define NCR_CBSR_RESEL		(NCR_CBSR_IO | NCR_CBSR_SEL)

/* Bits in bus and status register (sc_bas): read only */

#define	NCR_END_DMA_XFR		bit(7)
#define	NCR_DMA_REQ		bit(6)
#define NCR_PAR_ERR		bit(5)
#define	NCR_IRQ_ACTIVE		bit(4)
#define	NCR_PHASE_MATCH		bit(3)
#define	NCR_BUSY_ERR		bit(2)
#define	NCR_ATN			bit(1)
#define	NCR_ACK			bit(0)

/*
**************************************************
*/

/* 
** I/O bus phases as read in the Current SCSI Bus Status register
** NB: these are based on the three bits MSG, C/D & I/O
*/

#define NCR_PHASE_DATA_OUT	0	/* Data Out Phase */
#define NCR_PHASE_DATA_IN	0x04	/* Data In Phase */
#define NCR_PHASE_CMD		0x08	/* Command Phase */
#define NCR_PHASE_STATUS	0x0C	/* Status Phase */
#define NCR_PHASE_MESG_OUT	0x18	/* Message Out Phase */
#define NCR_PHASE_MESG_IN	0x1C	/* Message In Phase */

/* values returned by the interrupt investigation routine */
#define NCR_INT_ERR		0	/* error return value */
#define NCR_INT_RESEL		1	/* reselect interrupt */
#define NCR_INT_SEL		2	/* select interrupt */
#define NCR_INT_EOP		3	/* end of processing (dma) interrupt? */
#define NCR_INT_RESET		4	/* bus was reset */
#define NCR_INT_PARITY		5	/* got a parity error */
#define NCR_INT_PHASE		6	/* a phase change occurred */
#define NCR_INT_BUSY		7	/* busy was lost during monitoring */

/* Mask to filter bits in Bus And Status register to detect interrupt reason */
#define BAS_MASK		0xA4

/* Here are 3 different patterns for the interrupts */
#define BAS_INT_EOP		0x80
#define BAS_INT_PAR		0x20
#define BAS_INT_BUSY		0x04

/* Masks and values of Current bus status register to detect interrupt reason */
#define	CBS_MASK_EOP		0xC2
#define	CBS_MASK_PAR		0xE2
#define	CBS_MASK_BUSY		0xE3
#define	CBS_MASK_SEL		0xE2
#define	CBS_MASK_PHASE		0xC2

#define	CBS_INT_EOP		0x40
#define	CBS_INT_PAR		0x60
#define	CBS_INT_BUSY		0x00
#define	CBS_INT_SEL		0x02
#define	CBS_INT_PHASE		0x40

/* timervalues */
#ifdef __STDC__
#define	NCR_SEL_TIMEOUT		200000
#else
#define NCR_SEL_TIMEOUT		100000	/* > 0.25 seconds */
#endif
#define	NCR_AIP_WAIT		7	/* # usecs to wait for AIP to come on */

#endif /* __NCR_5380_H__ */
