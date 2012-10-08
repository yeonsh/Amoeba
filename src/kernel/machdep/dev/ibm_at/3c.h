/*	@(#)3c.h	1.3	94/04/06 09:15:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * E3COM503 Etherlink II and II/16 Interface                   
 */
#define E3C_GA			0x400
#define E3C_SAPROM		0x000
#define E3C_BASEADDR		0x300

#define E3C_GA_PSTR		E3C_GA+0x00
#define E3C_GA_PSPR		E3C_GA+0x01
#define E3C_GA_DQTR		E3C_GA+0x02
#define E3C_GA_BCFR		E3C_GA+0x03
#define E3C_GA_PCFR		E3C_GA+0x04
#define E3C_GA_CFR		E3C_GA+0x05
#define E3C_GA_CTRL		E3C_GA+0x06
#define E3C_GA_STREG		E3C_GA+0x07
#define E3C_GA_IDCFR		E3C_GA+0x08
#define E3C_GA_DAMSB		E3C_GA+0x09
#define E3C_GA_DALSB		E3C_GA+0x0A 
#define E3C_GA_VPTR2		E3C_GA+0x0B
#define E3C_GA_VPTR1		E3C_GA+0x0C
#define E3C_GA_VPTR0		E3C_GA+0x0D
#define E3C_GA_RFMSB		E3C_GA+0x0E
#define E3C_GA_RFLSB		E3C_GA+0x0F

#define E3C_EA0			0x00
#define E3C_EA1			0x01
#define E3C_EA2			0x02
#define E3C_EA3			0x03
#define E3C_EA4			0x04
#define E3C_EA5			0x05

#define E3C_GA_GACFR_MBS0	0x01
#define E3C_GA_GACFR_MBS1	0x02
#define E3C_GA_GACFR_MBS2	0x04
#define E3C_GA_GACFR_RSEL	0x08
#define E3C_GA_GACFR_TEST	0x10
#define E3C_GA_GACFR_OWS	0x20
#define E3C_GA_GACFR_TCM	0x40
#define E3C_GA_GACFR_NIM	0x80

#define E3C_GA_CTRL_RST		0x01
#define E3C_GA_CTRL_XSEL	0x02
#define E3C_GA_CTRL_EALO	0x04
#define E3C_GA_CTRL_EAHI	0x08
#define E3C_GA_CTRL_SHARE	0x10
#define E3C_GA_CTRL_DBSEL	0x20
#define E3C_GA_CTRL_DDIR	0x40
#define E3C_GA_CTRL_START	0x80

#define E3C_GA_CTRL_THIN	0x02
#define E3C_GA_CTRL_THICK	0x00

#define E3C_GA_STREG_REV0	0x01
#define E3C_GA_STREG_REV1	0x02
#define E3C_GA_STREG_REV2	0x04
#define E3C_GA_STREG_DIP	0x08
#define E3C_GA_STREG_DTC	0x10
#define E3C_GA_STREG_OFLW	0x20
#define E3C_GA_STREG_UFLW	0x40
#define E3C_GA_STREG_DPRDY	0x80

/* Interrupt/DMA config register */
#define	E3C_GA_IDCFR_DRQ0	0x01	/* DMA request 0 select */
#define	E3C_GA_IDCFR_DRQ1	0x02	/* DMA request 1 select */
#define	E3C_GA_IDCFR_DRQ2	0x04	/* DMA request 2 select */
#define	E3C_GA_IDCFR_RESERVED	0x08	/* reserved */
#define	E3C_GA_IDCFR_IRQ2	0x10	/* IRQ 2 select */
#define	E3C_GA_IDCFR_IRQ3	0x20	/* IRQ 3 select */
#define	E3C_GA_IDCFR_IRQ4	0x40	/* IRQ 4 select */
#define	E3C_GA_IDCFR_IRQ5	0x80	/* IRQ 5 select */

/*
 * Definitions according to page 10-2
 * of Technical Reference Manual
 */
#define GA_PageStart		0x26  /* Start register for incoming packets */
#define GA_PageStop		0x40  /* Stop register for incoming packets */

#define NIC_TPSR		0x20
#define NIC_PSTART		GA_PageStart
#define NIC_PSTOP		GA_PageStop
