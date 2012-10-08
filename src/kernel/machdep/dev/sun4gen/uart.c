/*	@(#)uart.c	1.4	96/02/27 13:57:30 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *      The Sun4 UART Driver (z8530 chip)
 *
 *	The Sun4 has two z8530 chips.  One for mouse and keyboard and one
 *	for external serial ports A & B.
 *
 *	The following ifdefs are relevant:
 *	IOP	defined if there is an iop server in the kernel for X windows.
 *	NOTTY	defined if there is no tty server.
 *	NO_OUTPUT_ON_CONSOLE defined if using host as pool processor
 *
 * Author:
 *	Gregory J. Sharp, Aug 1993 - based on original sun 3 driver by
 *				     Robbert van Renesse
 */

#include "amoeba.h"
#include "machdep.h"
#include "promid_sun4.h"
#include "global.h"
#include "memaccess.h"
#include "map.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "interrupt.h"
#include "openprom.h"
#include "tty/term.h"
#include "sys/proto.h"
#include "uart8530.h"

#define	BLEEPTIME	200 /* milliseconds */

extern int	sun_putchar();

#ifdef IOP
static int ev_ok;
extern void keyboard_intr _ARGS(( long ));
#endif /* IOP */

#define UARTB uartb
#define UARTA uarta
#define UARTM uartm
#define UARTK uartk
static struct z8530 *uartb, *uarta, *uartm, *uartk;

/* default serial port speeds */
#define	KSPEED			B(1200)		/* sun keyboard speed */
#define	MSPEED			B(1200)		/* mouse line speed */
#define	OSPEED			B(9600)		/* other serial ports speed */


extern struct mach_info	machine;	/* Our machine information */
extern void		readint();	/* read interrupt handler */

static int (*do_putchar)();	/* pointer to correct putchar routine */

/* when talking to ttys you need to know what your tty number is */
static long	B_ttynum;
static long	A_ttynum;
static long	K_ttynum;

#if defined(NOTTY) || !defined(NO_OUTPUT_ON_CONSOLE)
/* which device is the console */
static int	Console;
#endif

static unsigned char z8530Btabl[] =
{
    R(9),	0xC0,		/* force hardware reset */
    R(4),	0x44,		/* x16 clock, 1 stop, no parity */
    R(3),	0xC0,		/* RCV: 8 bits/char */
    R(5),	0x60,		/* XMT: 8 bits/char */
    R(1),	0x53,		/* no WAIT/DMA REQ, R-IRQ, T-IRQ */
    R(10),	0x00,		/* NRZ mode */
    R(11),	0x50,		/* RXC+TXC = BRG output */
    R(12),	lobyte(OSPEED),
    R(13),	hibyte(OSPEED),
    R(15),	0x00,		/* external interrupts disabled */
    R(14),	0x07,		/* BRG = PCLK */
    R(2),	0x40,		/* interrupt vector (couldn't get it to work) */
    R(5),	0x6A,		/* XMT: 8 bits/char, TXD enabled, DTR=L */
    R(3),	0xC1,		/* RCV: 8 bits/char, RXD enabled */
    R(9),	0x00,		/* interrupts disabled */
    0,		0
};

static unsigned char z8530Atabl[] =
{
    R(4),	0x44,		/* x16 clock, 1 stop, no parity */
    R(3),	0xC0,		/* RCV: 8 bits/char */
    R(5),	0x60,		/* XMT: 8 bits/char */
    R(1),	0x53,		/* no WAIT/DMA REQ, R-IRQ, T-IRQ */
    R(10),	0x00,		/* NRZ mode */
    R(11),	0x50,		/* RXC+TXC = BRG output */
    R(12),	lobyte(OSPEED),
    R(13),	hibyte(OSPEED),
    R(15),	0x00,		/* external interrupts disabled */
    R(14),	0x07,		/* BRG = PCLK */
    R(5),	0x6A,		/* XMT: 8 bits/char, TXD enabled, DTR=L */
    R(3),	0xC1,		/* RCV: 8 bits/char, RXD enabled */
    R(9),	0x0A,		/* master interrupt, no vector */
    0,		0
};

static unsigned char z8530Mtabl[] =
{
    R(9),	0xC0,		/* force hardware reset */
    R(4),	0x44,		/* x16 clock, 1 stop, no parity */
    R(3),	0xC0,		/* RCV: 8 bits/char */
    R(5),	0x60,		/* XMT: 8 bits/char */
    R(1),	0x53,		/* no WAIT/DMA REQ, R-IRQ, T-IRQ */
    R(10),	0x00,		/* NRZ mode */
    R(11),	0x50,		/* RXC+TXC = BRG output */
    R(12),	lobyte(MSPEED),
    R(13),	hibyte(MSPEED),
    R(15),	0x00,		/* external interrupts disabled */
    R(14),	0x07,		/* BRG = PCLK */
    R(2),	0x40,		/* interrupt vector (couldn't get it to work) */
    R(5),	0x6A,		/* XMT: 8 bits/char, TXD enabled, DTR=L */
    R(3),	0xC1,		/* RCV: 8 bits/char, RXD enabled */
    R(9),	0x00,		/* interrupts disabled */
    0,		0
};

static unsigned char z8530Ktabl[] =
{
    R(4),	0x44,		/* x16 clock, 1 stop, no parity */
    R(3),	0xC0,		/* RCV: 8 bits/char */
    R(5),	0x60,		/* XMT: 8 bits/char */
    R(1),	0x53,		/* no WAIT/DMA REQ, R-IRQ, T-IRQ */
    R(10),	0x00,		/* NRZ mode */
    R(11),	0x50,		/* RXC+TXC = BRG output */
    R(12),	lobyte(KSPEED),
    R(13),	hibyte(KSPEED),
    R(15),	0x00,		/* external interrupts disabled */
    R(14),	0x07,		/* BRG = PCLK */
    R(5),	0x6A,		/* XMT: 8 bits/char, TXD enabled, DTR=L */
    R(3),	0xC1,		/* RCV: 8 bits/char, RXD enabled */
    R(9),	0x0A,		/* master interrupt, no vector */
    0,		0
};


static int tty_svr_inited;  /* true if tty server is in use */

#ifndef NOTTY
static int queued;

static void
writestub(arg)
long arg;			/* arg = number of tty that was interrupted */
{
    extern writeint();

    queued &= ~(1 << arg);	/* switch off queued bit for this tty */
    if (tty_svr_inited)
	writeint(arg);
}

static void
enqwritestub(arg)
long arg;
{

    if (queued & (1 << arg))
	return;
    queued |= 1 << arg;
    enqueue(writestub, arg);
}
#endif /* NOTTY */


static void
delay()
{
    volatile int del = 1;
  
    del = del && del;
}


static char
readreg(uart, reg)
struct z8530 *uart;
unsigned char reg;
{
    delay();
    uart->csr = reg;
    delay();
    return uart->csr;
}


static
writereg(uart, reg, val)
struct z8530 *uart;
unsigned char reg, val;
{
    delay();
    uart->csr = reg;
    delay();
    uart->csr = val;
}


#define	MAX_WAIT	10000000

ttyA_putchar(c)
{
    long timeout = MAX_WAIT;

    while (!(readreg(UARTA, R(0)) & RR0_TxRDY))
    {
	if (--timeout == 0)	/* Don't wait forever */
	    return 1;
    }
    delay();
    UARTA->dr = c;
    return 0;
}


ttyB_putchar(c)
{
    long timeout = MAX_WAIT;

    while (!(readreg(UARTB, R(0)) & RR0_TxRDY))
    {
	if (--timeout == 0)	/* Don't wait forever */
	    return 1;
    }
    delay();
    UARTB->dr = c;
    return 0;
}


#ifndef NOTTY

putchar(iop, c)
struct io *	iop;
int		c;
{
    register long	ttynum;

    if (do_putchar == 0)
	do_putchar = sun_putchar;
    if (iop == 0)	/* write on console */
	return (*do_putchar)(c);
    ttynum = iop - tty;			/* figure out which port */
    if (ttynum == K_ttynum && ttynum == 0)
	return sun_putchar(c);
    if (ttynum == A_ttynum)
	return ttyA_putchar(c);
    if (ttynum == B_ttynum)
	return ttyB_putchar(c);
    panic("putchar: illegal tty");
    /*NOTREACHED*/
}

#else /* NOTTY */

#ifndef NO_OUTPUT_ON_CONSOLE

/*ARGSUSED*/
putchar(iop, c)
struct io *	iop;
{
    if (do_putchar == 0)
	do_putchar = sun_putchar;
    return (*do_putchar)(c);
}

#endif /* NO_OUTPUT_ON_CONSOLE */

#endif /* NOTTY */



static void
write_kb(c)
{
    long timeout = MAX_WAIT;

    while (!(readreg(UARTK, R(0)) & RR0_TxRDY))
    {
	if (--timeout == 0)	/* Don't wait forever */
	    return;
    }
    delay();
    UARTK->dr = c;
}


/*
**	startbleep and stopbleep are used by bleep to control the bell in
**	the sun keyboard.
*/

static int bleeping;

static void
startbleep()
{
    bleeping = 1;
    write_kb(0x2);
}

/*ARGSUSED*/
static void
stopbleep(arg)
long arg;
{
    write_kb(0x3);
    bleeping = 0;
}

bleep()
{
    if (!bleeping)
    {
	startbleep();
	sweeper_set(stopbleep, (long) 0, (interval) BLEEPTIME, 1);
    }
}


/*
 * Handle a special receive condition interrupt
 */

/*ARGSUSED*/
static void
handle_src(uart, uartstr)
struct z8530 *	uart;
char *		uartstr;
{
    register char c;

    c = readreg(uart, R(1));
    writereg(uart, R(0), 0x30);    /* error reset */
    DPRINTF(0, ("UART %s special receive condition interrupt\n", uartstr));
    if (c & RR1_FRAME_ERR)
	DPRINTF(0, ("UART %s: framing error\n", uartstr));
    if (c & RR1_OVERRUN_ERR)
	DPRINTF(0, ("UART %s: receiver overrun\n", uartstr));
    if (c & RR1_PARITY_ERR)
	DPRINTF(0, ("UART %s: parity error\n", uartstr));
}


static void keybd_rint();
#ifndef IOP
static void mouse_intr();
#else
void mouse_intr();
#endif
extern void readint();


static void
intuart()
{
    register int foundint;
    register char c;

    /* There are two duart chips - check if one or both interrupted */
    while (readreg(UARTA, R(3)))
    {
        foundint = 1;

	switch ((readreg(UARTB, R(2)) & 0xE) >> 1)
	{
	/* SERIAL PORT B */
	case RR2_B_XMIT:
	    writereg(UARTB, R(0), 0x28);    /* reset TxINT pending */
#ifndef NOTTY
	    enqwritestub(B_ttynum);
#endif /* NOTTY */
	    break;

	case RR2_B_EXT_STATUS:
	    writereg(UARTB, R(0), 0x10);    /* reset EXT/STATUS interrupts */
	    DPRINTF(0, ("UART 2B ext/status interrupt\n"));
	    break;

	case RR2_B_SRC:
	    handle_src(UARTB, "2B");
	    /* Fall through and take the (possibly bad) character */

	case RR2_B_RCV:
	    delay();
	    c = UARTB->dr; /* acknowledge interrupt */
#ifndef NOTTY
	    enqueue(readint, (c & 0xffL) | (B_ttynum << 16));
#else /* NOTTY */
	    if (Console == OP_IUARTB)
		enqueue(readint, (c & 0xffL));
#endif /* NOTTY */
	    break;

	/* SERIAL PORT A */
	case RR2_A_XMIT:
	    writereg(UARTA, R(0), 0x28);    /* reset TxINT pending */
#ifndef NOTTY
	    enqwritestub(A_ttynum);
#endif /* NOTTY */
	    break;

	case RR2_A_EXT_STATUS:
	    writereg(UARTA, R(0), 0x10);    /* reset EXT/STATUS interrupts */
	    DPRINTF(0, ("UART 2A ext/status interrupt\n"));
	    break;

	case RR2_A_SRC:
	    handle_src(UARTA, "2A");
	    /* Fall through and take the (possibly bad) character */

	case RR2_A_RCV:
	    delay();
	    c = UARTA->dr;	/* acknowledge interrupt */
#ifndef NOTTY
	    enqueue(readint, (c & 0xffL) | (A_ttynum << 16));
#else /* NOTTY */
	    if (Console == OP_IUARTA)
		enqueue(readint, (c & 0xffL));
#endif /* NOTTY */
	    break;

	/* CANNOT GET HERE! */
	default:
	    panic("bad UART 2 interrupt");
	} /* end switch */
    }

    while (readreg(UARTK, R(3)))
    {
	foundint = 1;
	switch ((readreg(UARTM, R(2)) & 0xE) >> 1)
	{
	/* MOUSE */
	case RR2_B_XMIT:	/* shouldn't get write interrupts from mouse */
	    writereg(UARTM, R(0), 0x28);	/* reset TxINT pending */
	    printf("UART 1M TxINT\n");
	    break;

	case RR2_B_EXT_STATUS:
	    writereg(UARTM, R(0), 0x10);     /* reset EXT/STATUS interrupts */
	    DPRINTF(0, ("UART 1M ext/status interrupt\n"));
	    break;

	case RR2_B_SRC:
	    handle_src(UARTM, "1M");
	    /* Fall through and take the (possibly bad) character */

	case RR2_B_RCV:
	    delay();
	    enqueue(mouse_intr, (long) (UARTM->dr & 0xffL));
	    break;

	/* KEYBOARD */
	case RR2_A_XMIT:  /* write interrupts from keybd due to bell */
	    writereg(UARTK, R(0), 0x28);	/* reset TxINT pending */
	    break;

	case RR2_A_EXT_STATUS:
	    writereg(UARTK, R(0), 0x10);     /* reset EXT/STATUS interrupts */
	    DPRINTF(0, ("UART 1K ext/status interrupt\n"));
	    break;

	case RR2_A_SRC:
	    handle_src(UARTK, "1K");
	    /* Fall through and take the (possibly bad) character */

	case RR2_A_RCV:
	    delay();
	    enqueue(keybd_rint, (long) ((UARTK->dr & 0xffL) | K_ttynum << 16));
	    break;

	/* CANNOT GET HERE! */
	default:
	    panic("bad UART 1 interrupt");
	}
    } 

    if (foundint == 0)	/* NOBODY INTERRUPTED! */
	DPRINTF(0, ("intuart: spurious UART interrupt\n"));
}


static void
set_uart_addr(p)
nodelist * p;
{
    vir_bytes	vaddr;
    int32	slv;
    int		proplen;

    proplen = obp_getattr(p->l_curnode, "address", (void *) &vaddr, sizeof (vaddr));
    compare (proplen, ==, sizeof (vaddr));

    proplen = obp_getattr(p->l_curnode, "slave", (void *) &slv, sizeof (slv));
    compare (proplen, ==, sizeof (slv));

    switch (slv)
    {
    case 0:
	/* Then it is the duart for serial ports a & b */
	DPRINTF(1, ("set_uart_addr: found uart slave 0 at %x\n", vaddr));
	uartb = (struct z8530 *) vaddr;
	uarta = uartb + 1;
	break;

    case 1:
	/* Duart for keyboard and mouse */
	DPRINTF(1, ("set_uart_addr: found uart slave %d at %x\n", slv, vaddr));
	assert(slv == 1);
	uartm = (struct z8530 *) vaddr;
	uartk = uartm + 1;
	break;

    default:
	printf("set_uart_addr: found uart slave %d. Ignoring it.\n", slv);
	break;
    }
}


void
inituart()
{
    register unsigned char *	p;

    obp_regnode("name", "zs", set_uart_addr);
    assert(uarta != 0 && uartk != 0); /* Should have found two uarts! */

    (void) setvec(INT_SERIAL, intuart);

    /* Set up the registers in the two duarts */
    for (p = z8530Mtabl; *p != 0; p += 2)
	writereg(UARTM, p[0], p[1]);
    for (p = z8530Ktabl; *p != 0; p += 2)
	writereg(UARTK, p[0], p[1]);

    for (p = z8530Btabl; *p != 0; p+=2)
	writereg(UARTB, p[0], p[1]);
    for (p = z8530Atabl; *p != 0; p+=2)
	writereg(UARTA, p[0], p[1]);

#ifndef NO_OUTPUT_ON_CONSOLE

    /* Look in eeprom to see what kind of console to use */
    if (prom_devp->op_magic != OBP_MAGIC)
	panic("inituart: openprom has bad magic number");

    Console = get_console_type();
    switch (Console)
    {
    case OP_IKEYBD:
	do_putchar = sun_putchar;
	K_ttynum = 0;
	A_ttynum = 1;
	B_ttynum = 2;
	break;

    case OP_IUARTA:
	do_putchar = ttyA_putchar;
	A_ttynum = 0;
	B_ttynum = 1;
	K_ttynum = 2;
	break;

    case OP_IUARTB:
	do_putchar = ttyB_putchar;
	B_ttynum = 0;
	A_ttynum = 1;
	K_ttynum = 2;
	break;

    default:
	panic("inituart: unknown console type %x", Console);
	/*NOTREACHED*/
    }

    /* At this point printf should work!  Some vital statistics ... */
    DPRINTF(0, ("\nROM-vector Version = %x\n", prom_devp->op_romvec_version));
    DPRINTF(0, ("PROM Monitor Version = %d.%d\n", prom_devp->op_mon_id >> 16,
					    prom_devp->op_mon_id & 0xFFFF));

#endif /* NO_OUTPUT_ON_CONSOLE */

}

#ifndef NOTTY

/*ARGSUSED*/
static int
uart_stat(begin, end, iop)
char *		begin;	/* start of buffer to dump status in */
char *		end;	/* end of buffer */
struct io *	iop;	/* tty to dump status for */
{
    return bprintf(begin, end, "no status available\n") - begin;
}


/*ARGSUSED*/
uart_init(cdp, first)
struct cdevsw *cdp;
struct io *first;	/* here for compatibility with tty driver */
{
/*  assert(first == &tty[0]); */
    cdp->cd_write = putchar;
#if !VERY_SIMPLE && !NO_IOCTL
    cdp->cd_setDTR = 0;
    cdp->cd_setBRK = 0;
    cdp->cd_baud = 0;
#endif /* !VERY_SIMPLE && !NO_IOCTL */
#if !VERY_SIMPLE || RTSCTS
    cdp->cd_isusp = 0;
    cdp->cd_iresume = 0;
#endif /* !VERY_SIMPLE || RTSCTS */
    cdp->cd_status = uart_stat;
    tty_svr_inited = 1;
    return 3;
}

#endif /* NOTTY */

unsigned char keytab[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1B,  '1',  '2',
	 '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
	 '-',  '=',  '`', '\b', 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\t',  'q',  'w',
	 'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',
	 '[',  ']', 0x7F, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  'a',  's',  'd',
	 'f',  'g',  'h',  'j',  'k',  'l',  ';', '\'',
	'\\', '\r', 0xFF, '\b', 0x1E, '\f', 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,  'z',  'x',  'c',  'v',
	 'b',  'n',  'm',  ',',  '.',  '/', 0xFF, '\n',
	0xFF, '\n', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF,  ' ', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

unsigned char sfttab[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1B,  '!',  '@',
	 '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
	 '_',  '+',  '~', '\b', 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, '\t',  'Q',  'W',
	 'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',
	 '{',  '}', 0x7F, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  'A',  'S',  'D',
	 'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',
	 '|', '\r', 0xFF, '\b', 0x1E, '\f', 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,  'Z',  'X',  'C',  'V',
	 'B',  'N',  'M',  '<',  '>',  '?', 0xFF, '\n',
	0xFF, '\n', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF,  ' ', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

static int shifted, cntled, alted, l_1ed;

static void
keybd_rint(c)
long c;
{
    switch (c)	/* interpret control keys */
    {
    case 0x01:
	l_1ed = 1;
	break;
    case 0x81:
	l_1ed = 0;
	break;
    case 0x63:
    case 0x6E:
	shifted = 1;
	break;
    case 0xE3:
    case 0xEE:
	shifted = 0;
	break;
    case 0x4C:
	cntled = 1;
	break;
    case 0xCC:
	cntled = 0;
	break;
    case 0x13:
	alted = 1;
	break;
    case 0x93:
	alted = 0;
	break;
    case 0x7F: /* sent if no keys are pressed! */
	return;
    }

    /* If they press L1-a then jump into the monitor - just like SunOS */
    if (l_1ed && c == 0x4D) {
	(*prom_devp->op_enter)();
	l_1ed = 0; /* Clear the flag since the prom got the key release */
	return;
    }

#ifdef IOP
    if (shifted && cntled && l_1ed) {
	/*
	** meta-shift-L1 is switch console<->Xwindows
	*/
	ev_ok = !ev_ok;
	console_enable(!ev_ok);
	printf("[Events %s]\n", ev_ok? "enabled" : "disabled");
	return;
    }
    if (ev_ok) {
	keyboard_intr(c);
	return;
    }
#endif /* IOP */
    if (c & 0x80)		/* ignore key release */
	return;
    c = (shifted ? sfttab[c] : keytab[c]) & 0xFF;	/* translate */
    if (c == 0xFF)	/* invalid */
	return;
    if (cntled)		/* control key is depressed */
	c &= 0x1F;
    if (alted)		/* alt key is depressed */
	c |= 0x80;
    readint(c);
}


#ifdef IOP

displaymapped()
{
    ev_ok = 1;
    console_enable(0);
}

displayunmapped()
{
    ev_ok = 0;
    console_enable(1);
}

void
kbd_setleds(i)
int i;
{
    write_kb(0xE);
    write_kb(i);
}

startclick()
{
    write_kb(0xA);
}

stopclick()
{
    write_kb(0xB);
}

#else /* IOP */

/*ARGSUSED*/
static void
mouse_intr(c)
long c;
{
    /* ignore the mouse when no window server */
}

#endif /* IOP */
