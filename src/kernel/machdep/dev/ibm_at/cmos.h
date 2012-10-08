/*	@(#)cmos.h	1.5	94/04/06 09:17:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* fundamental constants */
#define	CMOS_IRQ	8	/* real time clock interrupt */

/* I/O port addresses */
#define CMOS_ADDR	0x70	/* CMOS address port */
#define CMOS_DATA	0x71	/* CMOS data port */

/* number of bytes in CMOS RAM */
#define	CMOS_SIZE	0x40	/* 64 bytes of static ram */

/* offsets */
#define	CMOS_SECOND	0x00	/* RT clock second */
#define	CMOS_MINUTE	0x02	/* RT clock minute */
#define	CMOS_HOUR	0x04	/* RT clock hour */
#define	CMOS_DAY	0x07	/* RT clock day */
#define	CMOS_MONTH	0x08	/* RT clock month */
#define	CMOS_YEAR	0x09	/* RT clock year */
#define	CMOS_STATA	0x0A	/* RT clock status register A */
#define	CMOS_STATB	0x0B	/* RT clock status register B */
#define	CMOS_STATC	0x0C	/* RT clock status register C */
#define	CMOS_STATD	0x0D	/* RT clock status register D */
#define	CMOS_SHUTDOWN	0x0F	/* shutdown status */
#define	CMOS_DDTB	0x10	/* diskette drive type byte */
#define	CMOS_FDTB	0x12	/* fixed disk type byte */
#define	CMOS_EQB	0x14	/* equipment byte */
#define	CMOS_BMEM	0x15	/* size of base memory */
#define	CMOS_EMEM	0x17	/* size of expansion memory */

/*
 * CMOS clock interrupt rate. All times are in milliseconds:
 *
 * Rate		DV0		DV1		DV2
 * 1		0.000238	0.000954	0.030518
 * 2		0.000477	0.001907	0.061035
 * 3		0.000954	0.003815	0.122070
 * 4		0.001907	0.007629	0.244141
 * 5		0.003815	0.015259	0.488281
 * 6		0.007629	0.030518	0.976562
 * 7		0.015259	0.061035	1.953125
 * 8		0.030518	0.122070	3.906250
 * 9		0.061035	0.244141	7.812500
 * 10		0.122070	0.488281	15.625000
 * 11		0.244141	0.976562	31.250000
 * 12		0.488281	1.953125	62.500000
 * 13		0.976562	3.906250	125.000000
 * 14		1.953125	7.812500	250.000000
 * 15		3.906250	15.625000	500.000000
 */

/* status register A bits */
#define	CMOS_UIP	0x80	/* update in progress */
#define	CMOS_SDMASK	0x70	/* stage divider mask */
#define	CMOS_RSBMASK	0x0F	/* rate selection bit mask */
#define	CMOS_DV0	0x00	/* time base of 4.194304 MHz */
#define	CMOS_DV1	0x10	/* time base of 1.048576 MHz */
#define	CMOS_DV2	0x20	/* time base of 32.768 KHz */
#define	CMOS_RS6	0x06	/* interrupt rate of 0.9765625 ms (DIV2) */
#define	CMOS_RS9	0x09	/* interrupt rate of 7.8125 ms (DIV2) */

/* status register B bits */
#define	CMOS_SET	0x80	/* set/reset time update cycle */
#define	CMOS_PIE	0x40	/* periodic interrupt enable */
#define	CMOS_AIE	0x20	/* alarm interrupt enable */
#define	CMOS_UIE	0x10	/* update ended interrupt enable */
#define	CMOS_SQWE	0x08	/* square wave enabled */
#define	CMOS_DM		0x04	/* data mode: binary of BCD */
#define	CMOS_24HOUR	0x02	/* 24/12 hour clock */
#define	CMOS_DSE	0x01	/* day light savings enabled */

/* status register C bits */
#define	CMOS_IRQF	0x80	
#define	CMOS_PF		0x40	
#define	CMOS_AF		0x20	
#define	CMOS_UF		0x10	

/* status register C bits */
#define	CMOS_VRB	0x80	/* valid RAM bit */

/* diskette drives types */
#define	CMOS_NOFLP	0x00	/* no unit installed */
#define	CMOS_T40S9	0x01	/* 40 tracks, 9 sectors (360 Kb) */
#define	CMOS_T80S15	0x02	/* 80 tracks, 15 sectors (1.2 Mb) */
#define	CMOS_T80S9	0x03	/* 80 tracks, 9 sectors (720 Kb) */
#define	CMOS_T80S18	0x04	/* 80 tracks, 18 sectors (1.44 Mb) */

/* fixed disk type */
#define	CMOS_NOFDSK	0x00	/* no fixed disk installed */

/* equipment type video bits (already shifted) */
#define	CMOS_EGA	0x00	/* actually not cga or mono */
#define	CMOS_CGA40	0x01	/* cga, 40 columns mode */
#define	CMOS_CGA80	0x02	/* cga, 80 columns mode */
#define	CMOS_MONO80	0x03	/* monochrome, 80 columns */

/* equipment type co-processor bit */
#define	CMOS_COPROC	0x02	/* 80[23]87 math coprocessor available */

/* conversion macros */
#define bcd2dec(n)	(((n) >> 4) * 10 + ((n) & 0x0F))
#define	dec2bcd(n)	((((n) / 10) << 4) + ((n) % 10))
