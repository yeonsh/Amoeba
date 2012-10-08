/*	@(#)asy.h	1.4	94/04/06 09:16:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* fundamental constants */
#define	ASY_FREQ	115200L	/* uart's timer frequency */
#define	ASY_TIMEOUT	5	/* # of miliseconds to time out */

/* uart register assigments */
#define	ASY_LINE0	0x3F8	/* asy line 0 (COM1) */
#define	ASY_LINE1	0x2F8	/* asy line 1 (COM2) */
#define	ASY_LINE2	0x3E8	/* asy line 2 (COM3) */
#define	ASY_LINE3	0x2E8	/* asy line 3 (COM4) */

/* uart register offsets (relative to base register) */
#define	ASY_TXB		0	/* transmit buffer */
#define	ASY_RXB		0	/* receive buffer */
#define	ASY_DLLSB	0	/* divisor latch (LSB) */
#define	ASY_IER		1	/* interrupt enable register */
#define	ASY_DLMSB	1	/* divisor latch (MSB) */
#define	ASY_IIR		2	/* interrupt id register */
#define	ASY_LCR		3	/* line control register */
#define	ASY_MCR		4	/* modem control register */
#define	ASY_LSR		5	/* line status register */
#define	ASY_MSR		6	/* modem status register */

/* interrupt enable register bits */
#define	ASY_IER_RDA	0x01	/* enable receive data available */
#define	ASY_IER_THRE	0x02	/* enable transmitter holding register empty */
#define	ASY_IER_RLS	0x04	/* enable recieve line status */
#define	ASY_IER_RMS	0x08	/* enable receive modem status */

/* interrupt identification register bits */
#define	ASY_IIR_MSR	0x00	/* modem status change */
#define	ASY_IIR_PEND	0x01	/* 0 when interrupt pending */
#define	ASY_IIR_TXB	0x02	/* transmitter holding register empty */
#define	ASY_IIR_RXB	0x04	/* received data available */
#define	ASY_IIR_LSR	0x06	/* line status change */
#define	ASY_IIR_MASK	0x07	/* mask off just the meaningful bits */

/* line control register bits */
#define	ASY_LCR_WLS	0x03	/* word length select mask */
#define	ASY_LCR_5BIT	0x00	/* 5 data bits */
#define	ASY_LCR_6BIT	0x01	/* 6 data bits */
#define	ASY_LCR_7BIT	0x02	/* 7 data bits */
#define	ASY_LCR_8BIT	0x03	/* 8 data bits */
#define	ASY_LCR_STB	0x04	/* select # of stop bits */
#define	ASY_LCR_PEN	0x08	/* parity enable */
#define	ASY_LCR_EPS	0x10	/* even parity */
#define	ASY_LCR_STP	0x20	/* stuck parity */
#define	ASY_LCR_SETB	0x40	/* set break */
#define	ASY_LCR_DLAB	0x80	/* divisor latch access */

/* modem control register bits */
#define	ASY_MCR_DROP	0x00	/* drop line on modem */
#define	ASY_MCR_DTR	0x01	/* data terminal ready */
#define	ASY_MCR_RTS	0x02	/* request to send */
#define	ASY_MCR_OUT1	0x04	/* output 1 signal */
#define	ASY_MCR_OUT2	0x08	/* output 2 signal */
#define	ASY_MCR_LOOP	0x10	/* loopback diagnostic */

/* line status register bits */
#define	ASY_LSR_DR	0x01	/* data ready */
#define	ASY_LSR_OR	0x02	/* overrun error */
#define	ASY_LSR_PE	0x04	/* parity error */
#define	ASY_LSR_FE	0x08	/* framing error */
#define	ASY_LSR_BI	0x10	/* break interrupt */
#define	ASY_LSR_THRE	0x20	/* transmitter holding register empty */
#define	ASY_LSR_TEMT	0x40	/* transmitter empty */

/* modem status register bits */
#define	ASY_MSR_DCTS	0x01	/* delta clear to send */
#define	ASY_MSR_DDSR	0x02	/* delta data set ready */
#define	ASY_MSR_TERI	0x04	/* trailing edge ring indicator */
#define	ASY_MSR_DDCD	0x08	/* delta data carrier detect */
#define	ASY_MSR_CTS	0x10	/* clear to send */
#define	ASY_MSR_DSR	0x20	/* data set ready */
#define	ASY_MSR_RI	0x40	/* ring indicator */
#define	ASY_MSR_DCD	0x80	/* data carrier detect */

/* useful macros */
#define	ASY_BAUDRATE(b)	(ASY_FREQ/(b))

