/*	@(#)printer.c	1.12	96/02/27 13:52:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * printer.c
 *
 * This file contains a fairly simple printer driver, supporting only one
 * printer. The driver is multithreaded (guarded by mutexes), but it won't
 * do much good since the primary printer loop is locked during a single
 * request. The printer driver uses the ordinary FSQ_WRITE interface, and
 * provides status information up on a std_status request.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <machdep.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdcom.h>
#include <string.h>
#include <file.h>
#include <exception.h>
#include <machdep.h>
#include <module/mutex.h>
#include <module/rnd.h>
#include <module/prv.h>
#include <sys/proto.h>
#include "i386_proto.h"


#define PRINTER_CAP_NAME	"printer"

#ifndef NR_PRINTERSVR_THREADS
#define NR_PRINTERSVR_THREADS	2
#endif
#ifndef PRINTERSVR_STKSIZ
#define PRINTERSVR_STKSIZ	2200
#endif

/* printer's interrupt vector */
#define	PRT_IRQ		7		/* interrupt vector */

/* controller'ss register offsets */
#define	PRT_DATA	0x00		/* data register */
#define	PRT_STATUS	0x01		/* status register */
#define	PRT_CMD		0x02		/* command register */

/* controller commands */
#define	PRT_INIT	0x08		/* initialize printer bits */
#define	PRT_SELECT	0x0C		/* select printer bit */
#define	PRT_ASSERT	0x1D		/* strobe a character */
#define	PRT_NEGATE	0x1C		/* enable interrupts */

/* controller status bits */
#define	PRT_SELECTED	0x10		/* indicate that printer in on-line */
#define	PRT_NOPAPER	0x20		/* indicate that paper is up */
#define	PRT_IDLE	0x90		/* idle status bits */
#define	PRT_STATUSMASK	0xB0		/* filter out status bits */
#define	PRT_NOTPRESENT	0xFF		/* controller not present */

/*
 * Possible printer base addresses
 */
static int prt_address[] = {
    0x3BC, 0x378, 0x278
};

/* global data */
static capability prt_cap;		/* printer capability */
static int prt_base;			/* printer's base address */
static mutex prt_lock;			/* prevent concurrent update */

/* printer buffer variables */
static char *prt_chunk;			/* printer buffer */
static int prt_leftover;		/* # of bytes in print buffer */
errstat prt_errstat;			/* global error status */

void prt_intr();
void prt_reset();
char *prt_status();
errstat prt_write();
void pic_enable();

/*
 * Signal catcher
 */
/*ARGSUSED*/
void
prt_catcher(thread_no, sig)
int thread_no;
signum sig;
{
    return;
}

/*
 * Printer's main service loop
 */
void
prt_thread()
{
    header hdr;
    bufsize reqlen, replen;
    char *rbufptr, *reqbuf;
    rights_bits rts;
    
    getsig(prt_catcher, 0);
    reqbuf = aalloc((vir_bytes) BUFFERSIZE, 0);
    for (;;) {
	hdr.h_port = prt_cap.cap_port;
#ifdef USE_AM6_RPC
	reqlen = rpc_getreq(&hdr, reqbuf, BUFFERSIZE);
#else
	reqlen = getreq(&hdr, reqbuf, BUFFERSIZE);
#endif
	if (ERR_STATUS(reqlen)) {
	    printf("printer: getreq returned %d\n", reqlen);
	    continue;
	}
	replen = 0;
	rbufptr = NILBUF;
	if (prv_decode(&hdr.h_priv, &rts, &prt_cap.cap_priv.prv_random) != 0)
	    hdr.h_status = STD_CAPBAD;
	else {
	    switch (hdr.h_command) {
	    case FSQ_WRITE:
		if (rts & RGT_WRITE)
		    hdr.h_status = prt_write(reqbuf, (int) hdr.h_size);
		else
		    hdr.h_status = STD_DENIED;
		break;
	    case STD_STATUS:
		if (rts & RGT_STATUS)
		{
		    hdr.h_status = STD_OK;
		    rbufptr = prt_status();
		    hdr.h_size = replen = strlen(rbufptr);
		}
		else
		    hdr.h_status = STD_DENIED;
		break;
	    case STD_INFO:
		hdr.h_status = STD_OK;
		rbufptr = "printer server";
		hdr.h_size = replen = strlen(rbufptr);
		break;
	    default:
		hdr.h_status = STD_COMBAD;
		break;
	    }
	}
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, rbufptr, replen);
#else
	putrep(&hdr, rbufptr, replen);
#endif
    }
    /* NOTREACHED */
}

/*
 * Initialize the printer
 */
void
prt_init()
{
    capability pub_cap;
    register int i;

    mu_init(&prt_lock);
    uniqport(&prt_cap.cap_port);
    uniqport(&prt_cap.cap_priv.prv_random);
    prt_cap.cap_priv.prv_rights = PRV_ALL_RIGHTS;

    /* probe printer controller */
    setirq(PRT_IRQ, prt_intr);
    pic_enable(PRT_IRQ);
    for (i = 0; i < sizeof(prt_address)/sizeof(int); i++) {
	prt_reset(prt_address[i]);
	if (in_byte(prt_address[i] + PRT_STATUS) != PRT_NOTPRESENT) {
	    prt_base = prt_address[i];
	    break;
	}
    }
    if (i == sizeof(prt_address)/sizeof(int))
	return;
    printf("Printer: base address 0x%x\n", prt_base);

    /* publish the put capability of the server */
    pub_cap = prt_cap;
    priv2pub(&prt_cap.cap_port, &pub_cap.cap_port);
    dirappend(PRINTER_CAP_NAME, &pub_cap);

    for (i = 0; i < NR_PRINTERSVR_THREADS; i++)
	NewKernelThread(prt_thread, (vir_bytes) PRINTERSVR_STKSIZ);
}

/*
 * Write buffer to printer
 */
errstat
prt_write(buf, len)
    char *buf;
    int len;
{
    mu_lock(&prt_lock);
    prt_chunk = buf;
    prt_leftover = len;
    prt_intr(); /* start printing */
    (void) await((event) &prt_base, (interval) 0);
    prt_leftover = 0;
    mu_unlock(&prt_lock);
    return prt_errstat;
}

/*
 * Printer interrupt handler.
 * An annoying problem is that the 8259A controller sometimes generates
 * spurious interrupts to this printer vector, these are ignored.
 */
void
prt_intr()
{
    /* finished, or spurious interrupt */
    if (prt_leftover == 0) return;

    /*
     * Loop to handle fast (buffered) printers. It is important that
     * the processor interrupts are not disabled here.
     */
    do {
	int status = in_byte(prt_base + PRT_STATUS);

	if ((status & PRT_STATUSMASK) == PRT_SELECTED)
	    return; /* still busy with last character */
	else if ((status & PRT_STATUSMASK) == PRT_IDLE) {
	    out_byte(prt_base + PRT_DATA, *prt_chunk++);
	    disable(); /* prevent long strobes */
	    out_byte(prt_base + PRT_CMD, PRT_ASSERT);
	    out_byte(prt_base + PRT_CMD, PRT_NEGATE);
	    enable();
	} else {
	    prt_errstat = STD_SYSERR; /* what else ? */
	    enqueue((void (*) _ARGS((long))) wakeup, (long) &prt_base);
	    return;
	}
    } while (--prt_leftover > 0);
    prt_errstat = STD_OK;
    enqueue((void (*) _ARGS((long))) wakeup, (long) &prt_base);
}

/*
 * Query printer's status
 */
char *
prt_status()
{
    int status = in_byte(prt_base + PRT_STATUS);

    if ((status & PRT_NOPAPER) && (status & PRT_SELECTED) == 0)
	return "printer not on-line";
    if (status & PRT_NOPAPER)
	return "printer out of paper";
    if ((status & PRT_SELECTED) == 0)
	return "printer not on-line";
    if ((status & PRT_STATUSMASK) == PRT_SELECTED)
	return "printer busy";
    if ((status & PRT_STATUSMASK) == PRT_IDLE)
	return "printer idle";
    return "unknown printer error";
}

/*
 * Reset the printer's controller to some sane state
 */
void
prt_reset(base)
    int base;
{
    out_byte(base + PRT_CMD, PRT_INIT);
    pit_delay(2); /* should be enough */
    out_byte(base + PRT_CMD, PRT_SELECT);
}
