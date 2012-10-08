/*	@(#)kgdbio.c	1.2	94/04/06 09:21:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * kgdbio.c
 *
 * ISA/MCA dependent character I/O routines for the kernel GDB interface.
 * All you have to do to make this work is attach a workstation running
 * the GDB client to serial line zero (9600 Bd, 8 bits, 1 stop bit, no
 * parity) and you can start debugging.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <stddef.h>
#include <amoeba.h>
#include <machdep.h>
#include "asy.h"

#ifdef ASY
#error Cannot define ASY and KGDB !!!
#endif

#ifndef KGDB_IOPORT
#define	KGDB_IOPORT	ASY_LINE0
#endif

#ifndef KGDB_BAUDRATE
#define	KGDB_BAUDRATE	9600
#endif

/*
 * Initialize the kernel GDB I/O channel
 */
void
kgdbio_init()
{
    /* disable asy interrupts, we poll */
    out_byte(KGDB_IOPORT + ASY_IER, 0);

    /* set baud rate */
    out_byte(KGDB_IOPORT + ASY_LCR, ASY_LCR_DLAB);
    out_byte(KGDB_IOPORT + ASY_DLLSB, ASY_BAUDRATE(KGDB_BAUDRATE)&0xFF);
    out_byte(KGDB_IOPORT + ASY_DLMSB, (ASY_BAUDRATE(KGDB_BAUDRATE)>>8)&0xFF);

    /* turn on 8 bits, 1 stop bit, no parity */
    out_byte(KGDB_IOPORT + ASY_LCR, ASY_LCR_8BIT);
    out_byte(KGDB_IOPORT + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);
}

/*
 * Get a character from the kgdb port
 */
int
kgdbio_getc()
{
    while (!(in_byte(KGDB_IOPORT + ASY_LSR) & ASY_LSR_DR)) /* nothing */;
    return in_byte(KGDB_IOPORT + ASY_RXB) & 0xff;
}

/*
 * Put a character to the kgdb port
 */
void
kgdbio_putc(ch)
    int ch;
{
    while (!(in_byte(KGDB_IOPORT + ASY_LSR) & ASY_LSR_THRE)) /* nothing */; 
    out_byte(KGDB_IOPORT + ASY_TXB, ch & 0xff);
}
