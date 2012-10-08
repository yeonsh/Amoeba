/*	@(#)gdp8390.h	1.2	94/04/06 09:19:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * National Semiconductor DP8390 Network Interface Controller
 */

/* page 0: for reading */
#define DP_CR		0x00	/* read side of command register */
#define DP_CLDA0	0x01	/* current local DMA address 0 */
#define DP_CLDA1	0x02	/* current local DMA address 1 */
#define DP_BNRY		0x03	/* boundary pointer */
#define DP_TSR		0x04	/* transmit status register */
#define DP_NCR		0x05	/* number of collisions register */
#define DP_FIFO		0x06	/* FIFO */
#define DP_ISR		0x07	/* interrupt status register */
#define DP_CRDA0	0x08	/* current remote DMA address 0 */
#define DP_CRDA1	0x09	/* current remote DMA address 1 */
#define DP_RSR		0x0C	/* receive status register */
#define DP_CNTR0	0x0D	/* tally counter 0 */
#define DP_CNTR1	0x0E	/* tally counter 1 */
#define DP_CNTR2	0x0F	/* tally counter 2 */

/* page 0: for writing (we read them in page 2) */
#define DP_CR		0x00	/* write side of command register */
#define DP_PSTART	0x01	/* page start register */
#define DP_PSTOP	0x02	/* page stop register */
#define DP_BNRY		0x03	/* boundary pointer */
#define DP_TPSR		0x04	/* transmit page start register */
#define DP_TBCR0	0x05	/* transmit byte count register 0 */
#define DP_TBCR1	0x06	/* transmit byte count register 1 */
#define DP_ISR		0x07	/* interrupt status register */
#define DP_RSAR0	0x08	/* remote start address register 0 */
#define DP_RSAR1	0x09	/* remote start address register 1 */
#define DP_RBCR0	0x0A	/* remote byte count register 0 */
#define DP_RBCR1	0x0B	/* remote byte count register 1 */
#define DP_RCR		0x0C	/* receive configuration register */
#define DP_TCR		0x0D	/* transmit configuration register */
#define DP_DCR		0x0E	/* data configuration register */
#define DP_IMR		0x0F	/* interrupt mask register */

/* page 1: read/write */
#define DP_CR		0x00	/* command register */
#define DP_PAR0		0x01	/* physical address register 0 */
#define DP_PAR1		0x02	/* physical address register 1 */
#define DP_PAR2		0x03	/* physical address register 2 */
#define DP_PAR3		0x04	/* physical address register 3 */
#define DP_PAR4		0x05	/* physical address register 4 */
#define DP_PAR5		0x06	/* physical address register 5 */
#define DP_CURR		0x07	/* current page register */
#define DP_MAR0		0x08	/* multicast address register 0 */
#define DP_MAR1		0x09	/* multicast address register 1 */
#define DP_MAR2		0x0A	/* multicast address register 2 */
#define DP_MAR3		0x0B	/* multicast address register 3 */
#define DP_MAR4		0x0C	/* multicast address register 4 */
#define DP_MAR5		0x0D	/* multicast address register 5 */
#define DP_MAR6		0x0E	/* multicast address register 6 */
#define DP_MAR7		0x0F	/* multicast address register 7 */

/* bits in command register */
#define CR_STP		0x01		/* stop: software reset */
#define CR_STA		0x02		/* start: activate NIC */
#define CR_TXP		0x04		/* transmit packet */
#define CR_DMA		0x38		/* mask for DMA control */
#   define CR_DM_NOP	    0x00	/* DMA: no operation */
#   define CR_DM_RR	    0x08	/* DMA: remote read */
#   define CR_DM_RW	    0x10	/* DMA: remote write */
#   define CR_DM_SP	    0x18	/* DMA: send packet */
#   define CR_DM_ABORT	    0x20	/* DMA: abort remote DMA operation */
#define CR_PS		0xC0		/* mask for page select */
#   define CR_PS_P0	    0x00	/* register page 0 */
#   define CR_PS_P1	    0x40	/* register page 1 */
#   define CR_PS_P2	    0x80	/* register page 2 */
#   define CR_PS_T1	    0xC0	/* test mode register map */

/* bits in interrupt status register */
#define ISR_PRX		0x01		/* packet received with no errors */
#define ISR_PTX		0x02		/* packet transmitted with no errors */
#define ISR_RXE		0x04		/* receive error */
#define ISR_TXE		0x08		/* transmit error */
#define ISR_OVW		0x10		/* overwrite warning */
#define ISR_CNT		0x20		/* counter overflow */
#define ISR_RDC		0x40		/* remote DMA complete */
#define ISR_RST		0x80		/* reset status */

/* bits in interrupt mask register */
#define IMR_PRXE	0x01		/* packet received ienable */
#define IMR_PTXE	0x02		/* packet transmitted ienable */
#define IMR_RXEE	0x04		/* receive error ienable */
#define IMR_TXEE	0x08		/* transmit error ienable */
#define IMR_OVWE	0x10		/* overwrite warning ienable */
#define IMR_CNTE	0x20		/* counter overflow ienable */
#define IMR_RDCE	0x40		/* DMA complete ienable */

/* bits in data control register */
#define DCR_WTS		0x01		/* word transfer select */
#   define DCR_BYTEWIDE	0x00		/* WTS: byte wide transfers */
#   define DCR_WORDWIDE	0x01		/* WTS: word wide transfers */
#define DCR_BOS		0x02		/* byte order select */
#   define DCR_LTLENDIAN     0x00	/* BOS: little endian */
#   define DCR_BIGENDIAN     0x02	/* BOS: big endian */
#define DCR_LAS		0x04		/* long address select */
#define DCR_BMS		0x08		/* burst mode select */
#define DCR_AR		0x10		/* autoinitialize remote */
#define DCR_FTS		0x60		/* FIFO threshold select */
#   define DCR_2BYTES	    0x00	/* 2 bytes */
#   define DCR_4BYTES	    0x40	/* 4 bytes */
#   define DCR_8BYTES	    0x20	/* 8 bytes */
#   define DCR_12BYTES	    0x60	/* 12 bytes */

/* bits in transmit configuration register */
#define TCR_CRC		0x01		/* inhibit CRC */
#define TCR_ELC		0x06		/* encoded loopback control */
#   define TCR_NORMAL	    0x00	/* ELC: normal operation */
#   define TCR_INTERNAL	    0x02	/* ELC: internal loopback */
#   define TCR_0EXTERNAL    0x04	/* ELC: external loopback LPBK=0 */
#   define TCR_1EXTERNAL    0x06	/* ELC: external loopback LPBK=1 */
#define TCR_ATD		0x08		/* auto transmit */
#define TCR_OFST	0x10		/* collision offset enable (be nice) */

/* bits in transmit status register */
#define TSR_PTX		0x01		/* packet transmitted (without error)*/
#define TSR_DFR		0x02		/* transmit deferred */
#define TSR_COL		0x04		/* transmit collided */
#define TSR_ABT		0x08		/* transmit aborted */
#define TSR_CRS		0x10		/* carrier sense lost */
#define TSR_FU		0x20		/* fifo underrun */
#define TSR_CDH		0x40		/* CD heartbeat */
#define TSR_OWC		0x80		/* out of window collision */

/* bits in receive configuration register */
#define RCR_SEP		0x01		/* save errored packets */
#define RCR_AR		0x02		/* accept runt packets */
#define RCR_AB		0x04		/* accept broadcast */
#define RCR_AM		0x08		/* accept multicast */
#define RCR_PRO		0x10		/* physical promiscuous */
#define RCR_MON		0x20		/* monitor mode */

/* bits in receive status register */
#define RSR_PRX		0x01		/* packet received intact */
#define RSR_CRC		0x02		/* CRC error */
#define RSR_FAE		0x04		/* frame alignment error */
#define RSR_FO		0x08		/* FIFO overrun */
#define RSR_MPA		0x10		/* missed packet */
#define RSR_PHY		0x20		/* multicast address match !! */
#define RSR_DIS		0x40		/* receiver disabled */

/*
 * The dp8390 and board configuration parameters. This structure
 * is filled by the board specific initialization routine which
 * bases its values on the information in dp_info and data it
 * can extract from the board itself.
 */
typedef struct dpconf {
    int		  dpc_irq;		/* IRQ level */
    int		  dpc_reg;		/* board base I/O register */
    int		  dpc_dpreg;		/* dp8390 base I/O register */
    unsigned char dpc_tpsr;		/* start transmit page */
    unsigned char dpc_pstart;		/* start of receive ring */
    unsigned char dpc_pstop;		/* end of receive ring */
    int		  dpc_16bit;		/* 16-bit memory access */
    phys_bytes 	  dpc_basemem;		/* on board base memory */
} dpc_t, *dpc_p;

/*
 * dp8390 recieve header
 */
typedef struct dphead {
    char	dph_status;		/* copy of rsr */
    char	dph_next;		/* pointer to next packet */
    char	dph_rbcl;		/* receive byte count low */
    char	dph_rbch;		/* receive byte count high */
} dphead_t, *dphead_p;

/*
 * dp8390 hardware and driver statistics
 */
typedef struct dpstat {
    int         dps_carlost;		/* carrier sense lost */
    int         dps_collisions;		/* packets collided at least once */
    int         dps_crc;		/* input CRC errors */
    int         dps_deferred;		/* deferred packets */
    int         dps_fifo;		/* FIFO underrun */
    int         dps_frame;		/* input framing errors */
    int         dps_heartbeat;		/* heart beat failure */
    int         dps_lcol;		/* late collisions */
    int         dps_lost;		/* packets lost */
    int         dps_ovw;		/* over writes */
    int         dps_read;		/* packets read */
    int         dps_written;		/* packets written */
    int         dps_xcollisions;	/* aborts due to excessive collisions*/
    int		dps_frozen;		/* frozen transmitter */
    int		dps_align;		/* frame alignment error */
    int		dps_wraps;		/* wrap around copies */
} dpstat_t, *dpstat_p;
