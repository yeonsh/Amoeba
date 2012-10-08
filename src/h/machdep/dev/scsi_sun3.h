/*	@(#)scsi_sun3.h	1.3	94/04/06 16:46:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SCSI_SUN_H__
#define __SCSI_SUN_H__

/* Sun device dependant SCSI include file */

#define	SUN_NUM_SCSI_CONTROLLERS	1

/* According to sun documentation the initiator ID is set to 0x80 */
#define INITIATOR_ID		0x80

/*
** scsi_info
** This is used by the driver to find the addresses of each device.
** It should be defined and initialised in conf.c
*/

struct scsi_info
{
    vir_bytes	s_scsi_ctlr;
    vir_bytes	s_dma_ctlr;
    vir_bytes	s_sunregs;
    vir_bytes	s_dvma_offset;
};

#define	ADRSUNSCSI(c)	((sun_scsi *) Scsi_info[c].s_sunregs)
#define	ADR9516(c)	((am_9516a *) Scsi_info[c].s_dma_ctlr)
#define	ADR5380(c)	((ncr_5380 *) Scsi_info[c].s_scsi_ctlr)
#define	DVMA_OFFSET(c)	Scsi_info[c].s_dvma_offset

/* Special hardware registers for Sun SCSI interface */

typedef struct sun_scsi sun_scsi;
struct sun_scsi
{
    volatile uint16	ss_firstbyte;
    volatile uint16	ss_cnt;
    volatile uint16	ss_stat;
};


/* SCSI status register bits */

/* read-write */

#define SS_SCSI_RESET	bit(0)
#define SS_FIFO_RESET	bit(1)
#define SS_SBC_INT_EN	bit(2)
#define SS_SEND		bit(3)
#define SS_RECEIVE	0

/* read-only */

#define SBC_INT		bit(9)
#define FIFO_EMPTY	bit(10)
#define FIFO_FULL	bit(11)
#define SECONDBYTE	bit(12)
#define SCSI_BERR	bit(13)
#define SCSI_BCNFL	bit(14)
#define XFR_ACTIVE	bit(15)


/* some useful return values */
#define	OK		0
#define	NOT_OK		1
#define	DIFFPH		2


/* Scsi Bus Phases */
#define SBP_FREE		0x00	/* Bus Free Phase */
#define SBP_ARBIT		0x01	/* Arbitration Phase */
#define SBP_SELECT		0x02	/* Selection Phase */
#define SBP_CMD			0x03	/* Command Phase */
#define SBP_DATAO		0x04	/* Data Out Phase */
#define SBP_DATAI		0x05	/* Data In Phase */
#define SBP_RESELECT		0x08	/* Reselection Phase */
#define SBP_STATUS		0x0C	/* Status Phase */
#define SBP_MESSO		0x18	/* Message Out Phase */
#define SBP_MESSI		0x1C	/* Message In Phase */
#define SBP_RESET		0x50	/* During resetting of the bus */
#define SBP_UNSPECIFIED		0x7F	/* unspecified phase */

#define SBP_NO_PHASE		0xFE	/* a NULL argument for wait_phase */
#define SBP_TIMEOUT		0xFF	/* return value for wait_phase */

/* Various timeouts and bus delays */
#define	SC_ARBIT_DELAY		2	/* 2.2 us */
#define	SC_BUS_SETTLE_DELAY	1	/* 400ns (1 us) */
#define	SC_BUS_CLEAR_DELAY	1	/* 800ns (1 us) */
#define	SC_MIN_RESET		30	/* minimum time to assert reset pin */
#define	SC_SEL_ABORT_TIME	20	/* units = 10 us */
#define PHASE_TIMEOUT		1000000	/* timer value for wait_phase */
#define RW_BYTE_TIMEOUT		10000	/* timer value for rw_byte */

#endif /* __SCSI_SUN_H__ */
