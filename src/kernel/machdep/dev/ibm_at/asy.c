/*	@(#)asy.c	1.7	94/08/19 12:51:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * asy.c
 *
 * A simple serial driver for 8250 and 16450 UARTs. The driver can handle
 * four possible lines, of which some may share their irq level. Irq 3 is
 * currently disabled since it conflicts with the WD8003E ethernet board.
 * The driver is capable of setting the UART's baud rate, DTR, line break,
 * and has even hardware flow control, but it can't set the line parity.
 * This has nothing to do with this driver being unable, but the tty server
 * simply doesn't provide this functionality. How someone can write a tty
 * server without addressing these issues is beyond me.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <fault.h>
#include <map.h>
#include <assert.h>
INIT_ASSERT
#include <tty/term.h>

#include "asy.h"
#include "sys/proto.h"

/*
 * Asy device addresses and irq vectors
 */
struct asy_dev {
    int asy_reg;
    int asy_irq;
} asy_dev[] = {
    { 0x3F8,	4 },	/* line 0 (COM1) */
/*  { 0x2F8,	3 },	/* line 1 (COM2) */
    { 0x3E8,	4 },	/* line 2 (COM3) */
/*  { 0x2E8,	3 },	/* line 3 (COM4) */
};

/*
 * Baud rate conversion table
 */
int asy_bauds[] = {
    ASY_BAUDRATE(50),
    ASY_BAUDRATE(75),
    ASY_BAUDRATE(110),
    ASY_BAUDRATE(134),
    ASY_BAUDRATE(150),
    ASY_BAUDRATE(200),
    ASY_BAUDRATE(300),
    ASY_BAUDRATE(600),
    ASY_BAUDRATE(1200),
    ASY_BAUDRATE(1800),
    ASY_BAUDRATE(2400),
    ASY_BAUDRATE(4800),
    ASY_BAUDRATE(9600),
    ASY_BAUDRATE(19200),
};

/* first asy tty */
struct io *asy_tty;

/* queued write events */
long asy_queued = 0;

void readint();
int writeint();
int asy_write();
int asy_setDTR();
int asy_setBRK();
int asy_baud();
int asy_isusp();
int asy_iresume();
int asy_stat();
void asy_intr();

/*
 * Initialize asy's tty server structure and initialize
 * every UART line with a sensible default.
 */
int
asy_init(cdp, first)
    struct cdevsw *cdp;
    struct io *first;
{
    register int i;
    int nasy = 0;
    long irqmask = 0;

    /*
     * Initialze tty server structures
     */
    asy_tty = first;
    cdp->cd_write = asy_write;
#if !VERY_SIMPLE && !NO_IOCTL
    cdp->cd_setDTR = asy_setDTR;
    cdp->cd_setBRK = asy_setBRK;
    cdp->cd_baud = asy_baud;
#endif !VERY_SIMPLE && !NO_IOCTL
#if !VERY_SIMPLE || RTSCTS
    cdp->cd_isusp = asy_isusp;
    cdp->cd_iresume = asy_iresume;
#endif !VERY_SIMPLE || RTSCTS
    cdp->cd_status = asy_stat;

    /*
     * Initialize every UART line. The default setting consists of
     * 9600 baud, eight data bits, no parity, and one stop bit.
     */
    for (i = 0; i < sizeof(asy_dev)/sizeof(struct asy_dev); i++) {
	register int basereg = asy_dev[i].asy_reg;
	int divisor = ASY_BAUDRATE(9600);

	/* probe controller, assume lines are listed in order */
	out_byte(basereg + ASY_IER, 0);
	if (in_byte(basereg + ASY_IER)) break;

	/* set interrupt vector, if not already set */
	if ((irqmask & (1 << asy_dev[i].asy_irq)) == 0) {
	    irqmask |= (1 << asy_dev[i].asy_irq);
	    setirq(asy_dev[i].asy_irq, asy_intr);
	    pic_enable(asy_dev[i].asy_irq);
	}

	/* set baud rate */
	out_byte(basereg + ASY_LCR, ASY_LCR_DLAB);
	out_byte(basereg + ASY_DLLSB, divisor & 0xFF);
	out_byte(basereg + ASY_DLMSB, (divisor >> 8) & 0xFF);

	/* turn on 8 bits, 1 stop bit, no parity */
	out_byte(basereg + ASY_LCR, ASY_LCR_8BIT);
	out_byte(basereg + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);
	out_byte(basereg + ASY_IER, ASY_IER_RDA|ASY_IER_THRE|ASY_IER_RLS);

	printf("Async: tty:%02d at base address 0x%x\n",
	    i + (asy_tty - &tty[0]) , basereg);
	nasy++;
    }
    return nasy;
}

/*
 * Handle asy interrupts
 */
void
asy_intr(irq)
    int irq;
{
    register int i;
    int asy_writeint();

    /*
     * Walk through the asy line list and handle interrupts for every
     * line which shares the current irq level. Handling them all instead
     * of only one prevents starvation.
     */
    for (i = 0; i < sizeof(asy_dev)/sizeof(struct asy_dev); i++) {
	register int basereg, iireg;
	register int count;

	if (asy_dev[i].asy_irq != irq) continue;
	basereg = asy_dev[i].asy_reg;
	for (count = 0; count < 100; count++) {
	    iireg = in_byte(basereg + ASY_IIR);
	    if ((iireg & ASY_IIR_PEND) != 0) {
		/* no interrupt pending */
		break;
	    }
	    switch (iireg) {
	    case ASY_IIR_MSR: /* modem status change */
		break;
	    case ASY_IIR_TXB: /* transmitter holding register empty */
		if ((asy_queued & (1 << i)) == 0) {
		    asy_queued |= (1 << i);
		    enqueue(asy_writeint, (long)i);
		}
		break;
	    case ASY_IIR_RXB: /* received data available */
		(void) in_byte(asy_dev[i].asy_reg + ASY_LSR);
		enqueue(readint, (long)in_byte(basereg + ASY_RXB) |
		    (asy_tty - &tty[0] + i) << 16);
		break;
	    case ASY_IIR_LSR: /* line status change */
		if (in_byte(asy_dev[i].asy_reg + ASY_LSR) & ASY_LSR_DR)
		    (void) in_byte(basereg + ASY_RXB);
		break;
	    }
	}
	if (count >= 100) {
	    /* sanity check; shouldn't happen */
	    printf("asy %d (irq %d): excessive interrupts\n", i, irq);
	}
    }
}

/*
 * The UART generates a lot more ASY_IIR_TXB interrupts than
 * we can handle. This simple "lock" prevents queue overflows.
 */
int
asy_writeint(line)
    long line;
{
    asy_queued &= ~(1 << line);
    writeint(asy_tty - &tty[0] + line);
}

/*
 * Write a character
 */
int
asy_write(iop, ch)
    struct io *iop;
    char ch;
{
    register int basereg = asy_dev[iop - asy_tty].asy_reg;
    register int count;

    for (count = 0; count < 1000; count++) {
	if ((in_byte(basereg + ASY_LSR) & ASY_LSR_THRE) != 0) {
	    break;
	}
    }
    out_byte(basereg + ASY_TXB, ch);
    return 0;
}

/*
 * Set/unset Data Terminal Ready
 */
int
asy_setDTR(iop, set)
    struct io *iop;
    int set;
{
    register int mcr, basereg = asy_dev[iop - asy_tty].asy_reg;

    if (set)
	mcr = in_byte(basereg + ASY_MCR) | ASY_MCR_DTR;
    else
	mcr = in_byte(basereg + ASY_MCR) & ~ASY_MCR_DTR;
    out_byte(basereg + ASY_MCR, mcr);
    return 1;
}

/*
 * Set/unset line break
 */
int
asy_setBRK(iop, set)
    struct io *iop;
    int set;
{
    register int lcr, basereg = asy_dev[iop - asy_tty].asy_reg;

    if (set)
	lcr = in_byte(basereg + ASY_LCR) | ASY_LCR_SETB;
    else
	lcr = in_byte(basereg + ASY_LCR) & ~ASY_LCR_SETB;
    out_byte(basereg + ASY_LCR, lcr);
    return 1;
}

/*
 * Set baud rate
 */
int
asy_baud(iop, speed)
    struct io *iop;
    int speed;
{
    register int basereg = asy_dev[iop - asy_tty].asy_reg;
    register int divisor, lcr;

    assert(speed < (sizeof(asy_bauds) / sizeof(int)));

    /* set divisor latch */
    divisor = asy_bauds[speed];

    /* set speed, and reinitialize the uart */
    lcr = in_byte(basereg + ASY_LCR);
    out_byte(basereg + ASY_IER, 0);
    out_byte(basereg + ASY_LCR, ASY_LCR_DLAB);
    out_byte(basereg + ASY_DLLSB, divisor & 0xFF);
    out_byte(basereg + ASY_DLMSB, (divisor >> 8) & 0xFF);
    out_byte(basereg + ASY_LCR, lcr);
    out_byte(basereg + ASY_LCR, ASY_LCR_8BIT);
    out_byte(basereg + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);
    out_byte(basereg + ASY_IER, ASY_IER_RDA|ASY_IER_THRE|ASY_IER_RLS);
    return 1;
}

#if !VERY_SIMPLE || RTSCTS
/*
 * Suspend remote device by dropping RTS
 */
int
asy_isusp(iop)
    struct io *iop;
{
    register int basereg = asy_dev[iop - asy_tty].asy_reg;

    out_byte(basereg + ASY_MCR, ASY_MCR_DTR|ASY_MCR_OUT2);
    return 1;
}

/*
 * Start remote device by raising RTS
 */
int
asy_iresume(iop)
    struct io *iop;
{
    register int basereg = asy_dev[iop - asy_tty].asy_reg;

    out_byte(basereg + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);
    return 1;
}
#endif /* !VERY_SIMPLE || RTSCTS */

/*ARGSUSED*/
int
asy_stat(begin, end, iop)
    char *begin;	/* start of buffer to dump status in */
    char *end;		/* end of buffer */
    struct io *iop;	/* tty to dump status for */
{
    register int basereg = asy_dev[iop - asy_tty].asy_reg;

    return bprintf(begin, end,
	"lcr %x, lsr %x, mcr %x, msr %x, ier %x\n",
	in_byte(basereg + ASY_LCR), in_byte(basereg + ASY_LSR),
	in_byte(basereg + ASY_MCR), in_byte(basereg + ASY_MSR),
	in_byte(basereg + ASY_IER)) - begin;
}

