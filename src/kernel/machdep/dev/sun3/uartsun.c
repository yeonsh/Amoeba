/*	@(#)uartsun.c	1.8	94/05/31 13:02:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	If you ARE using a window server then you should define WINDOWS.
**	In this case the keyboard and the mouse interrupts are NOT given to
**	the tty driver but go to the server.
**
**	If WINDOWS is defined and NOTTY is defined then output to serial ports
**	still works but input is ignored.
**
**	If you are NOT using an input server then you must have a tty driver.
**	Mouse interrupts are ignored.  Fanatics can interpret mouse interrupts
**	by modifying the dummy mouse_intr routine for when WINDOWS isn't set.
**
**							gregor
*/

#include "amoeba.h"
#include "memaccess.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#include "assert.h"
INIT_ASSERT
#include "tty/term.h"
#include "sys/proto.h"
#include "arch_proto.h"
#include "uart8530.h"

#ifdef IOP
static int ev_ok = 0;
#endif

#define UARTB	ABSPTR(struct z8530 *, IOBASE + 0x2000)	/* uart B */
#define UARTA	ABSPTR(struct z8530 *, IOBASE + 0x2004)	/* uart A */
#define UARTM	ABSPTR(struct z8530 *, IOBASE + 0x0000)	/* mouse */
#define UARTK	ABSPTR(struct z8530 *, IOBASE + 0x0004)	/* keyboard */

/* values for console type that can read from eeprom */
#define	MONOCHROME_MONITOR	0x00
#define	SERIAL_PORT_A		0x10
#define	SERIAL_PORT_B		0x11
#define	COLOUR_MONITOR		0x12

/* default serial port speeds */
#define	KSPEED			B(1200)		/* sun keyboard speed */
#define	MSPEED			B(1200)		/* mouse line speed */
#define	OSPEED			B(9600)		/* other serial ports speed */

#if !defined(NOTTY) || !defined(NO_OUTPUT_ON_CONSOLE)

static int (*do_putchar)();	/* pointer to correct putchar routine */

/* when talking to ttys you need to know what your tty number is */
static long	B_ttynum;
static long	A_ttynum;

#endif

static long	K_ttynum;

/* which device is the console */
static int	Console;

static char z8530Btabl[] =
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

static char z8530Atabl[] =
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

static char z8530Mtabl[] =
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

static char z8530Ktabl[] =
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


static int tty_svr_inited;

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
    int del = 1;
  
    del = del && del;
}


static char
readreg(uart, reg)
struct z8530 *uart;
{
    delay();
    uart->csr = reg;
    delay();
    return uart->csr;
}


static
writereg(uart, reg, val)
struct z8530 *uart;
{
    delay();
    uart->csr = reg;
    delay();
    uart->csr = val;
}


#if !defined(NOTTY) || !defined(NO_OUTPUT_ON_CONSOLE)

ttyA_putchar(c)
{
    while (!(readreg(UARTA, 0) & RR0_TxRDY))
	;
    delay();
    UARTA->dr = c;
    return 0;
}


ttyB_putchar(c)
{
    while (!(readreg(UARTB, 0) & RR0_TxRDY))
	;
    delay();
    UARTB->dr = c;
    return 0;
}

#endif


#ifndef NOTTY

putchar(iop, c)
struct io *	iop;
{
    int			sun_putchar();
    register long	ttynum;

    if (iop == 0)	/* write on console */
    {
	if (!do_putchar)
	{
#ifdef EARLY_CONSOLE	/* not for sun3/50s */
	    font_boot();
	    erase();
	    do_putchar = sun_putchar;
	    K_ttynum = 0;
	    A_ttynum = 1;
	    B_ttynum = 2;
#else
	    return 0;
#endif
	}

	return (*do_putchar)(c);
    }
    ttynum = iop - tty;			/* figure out which port */
    if (ttynum == K_ttynum)
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
    if (!do_putchar)
	return 0;
    else
	return (*do_putchar)(c);
}

#endif /* NO_OUTPUT_ON_CONSOLE */
#endif /* NOTTY */


/*
**	startbleep and stopbleep are used by bleep to control the bell in
**	the sun keyboard.  to ring the bell for this keyboard call "bleep()"
**	in timersun.c
*/
write_kb(c)
{
    while (!(readreg(UARTK, 0) & RR0_TxRDY))
	;
    delay();
    UARTK->dr = c;
}

startbleep()
{
    write_kb(0x2);
}

stopbleep()
{
    write_kb(0x3);
}

#ifdef IOP

startclick()
{
    write_kb(0xA);
}

stopclick()
{
    write_kb(0xB);
}

#endif /* IOP */

static void keybd_rint();
extern void readint();
void mouse_intr();


/*ARGSUSED*/
static void
intuart(vecdata)
int vecdata;
{
    int got_intr;

    got_intr = 0;
    if (readreg(UARTA, 3))	/* DUART 2 interrupted */
    {
	got_intr = 1;
	switch (readreg(UARTB, 2) & 0xE)
	{
    /* SERIAL PORT B */
	case 0x0:
	    writereg(UARTB, 0, 0x28);	/* reset TxINT pending */
#ifndef NOTTY
	    enqwritestub(B_ttynum);
#endif /* NOTTY */
	    break;
	case 0x2:
	    writereg(UARTB, 0, 0x10);	/* reset EXT/STATUS interrupts */
	    printf("UART 2B ext/status interrupt\n");
	    break;
	case 0x4:
	    {
		register char c;

		delay();
		c = UARTB->dr; /* acknowledge interrupt */
#ifndef NOTTY
		enqueue(readint, (c & 0xffL) | (B_ttynum << 16));
#else /* NOTTY */
		if (Console == SERIAL_PORT_B)
		    enqueue(readint, (c & 0xffL));
#endif /* NOTTY */
	    }
	    break;
	case 0x6:
	    writereg(UARTB, 0, 0x30);	/* error reset */
	    printf("UART 2B special receive condition interrupt\n");
	    break;
    /* SERIAL PORT A */
	case 0x8:
	    writereg(UARTA, 0, 0x28);	/* reset TxINT pending */
#ifndef NOTTY
	    enqwritestub(A_ttynum);
#endif /* NOTTY */
	    break;
	case 0xA:
	    writereg(UARTA, 0, 0x10);	/* reset EXT/STATUS interrupts */
	    printf("UART 2A ext/status interrupt\n");
	    break;
	case 0xC:
	    {
		register char c;

		delay();
		c = UARTA->dr;	/* acknowledge interrupt */
#ifndef NOTTY
		enqueue(readint, (c & 0xffL) | (A_ttynum << 16));
#else /* NOTTY */
		if (Console == SERIAL_PORT_A)
		    enqueue(readint, (c & 0xffL));
#endif /* NOTTY */
	    }
	    break;
	case 0xE:
	    {
		register char c;

		c = readreg(UARTA, 1);
		writereg(UARTA, 0, 0x30);	/* error reset */
		printf("UART 2A special receive condition intr(%x)\n", c&0xff);
	    }
	    break;
	/* CANNOT GET HERE! */
	default:
	    panic("bad UART 2 interrupt");
	} /* end switch */
    }

    if (readreg(UARTK, 3))	/* UART 1 interrupted */
    {
	got_intr = 1;
	switch (readreg(UARTM, 2) & 0xE)
	{
    /* MOUSE */
	case 0x0:	/* shouldn't get write interrupts from mouse */
	    writereg(UARTM, 0, 0x28);	/* reset TxINT pending */
	    printf("UART 1M TxINT\n");
	    break;
	case 0x2:
	    writereg(UARTM, 0, 0x10);     /* reset EXT/STATUS interrupts */
	    printf("UART 1M ext/status interrupt\n");
	    break;
	case 0x4:
	    delay();
	    enqueue(mouse_intr, (UARTM->dr & 0xffL));
	    break;
	case 0x6:
	    writereg(UARTM, 0, 0x30);	/* error reset */
	    printf("UART 1M special receive condition interrupt\n");
	    break;
    /* KEYBOARD */
	case 0x8:  /* write interrupts from keybd due to bell */
	    writereg(UARTK, 0, 0x28);	/* reset TxINT pending */
	    break;
	case 0xA:
	    writereg(UARTK, 0, 0x10);     /* reset EXT/STATUS interrupts */
	    printf("UART 1K ext/status interrupt\n");
	    break;
	case 0xC:
	    delay();
	    enqueue(keybd_rint, (UARTK->dr & 0xffL) | K_ttynum << 16);
	    break;
	case 0xE:
	    writereg(UARTK, 0, 0x30);	/* error reset */
	    printf("UART 1K special receive condition interrupt\n");
	    break;
	/* CANNOT GET HERE! */
	default:
	    panic("bad UART 1 interrupt");
	}
    }
    if (!got_intr)	/* NOBODY INTERRUPTED! */
	printf("spurious UART interrupt");
}


void
inituart()
{
    extern		cputype;
    register char *	p;
    int			sun_putchar();


    (void) setvec(30, intuart);

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
/* look in eeprom to see what kind of console to use */
    Console = *(char *)(IOBASE+0x4000+0x1F);
/*
** load fonts and clear screen
**	this is necessary all the time in case there is a window system
**	but the console is on a serial port
*/
    font_boot();
    erase();
    switch (Console)
    {
    case MONOCHROME_MONITOR:
/*
**there is no tty:00 if there are windows but the read interrupt routine
** doesn't go to the tty driver in that case
*/
	do_putchar = sun_putchar;
	K_ttynum = 0;
	A_ttynum = 1;
	B_ttynum = 2;
	break;

    case SERIAL_PORT_A:
	do_putchar = ttyA_putchar;
	A_ttynum = 0;
	B_ttynum = 1;
/* just in case there is a keyboard but it isn't the console */
	K_ttynum = 2;
	printf("Console on serial port A\n");
	break;

    case SERIAL_PORT_B:
	do_putchar = ttyB_putchar;
	B_ttynum = 0;
	A_ttynum = 1;
/* just in case there is a keyboard but it isn't the console */
	K_ttynum = 2;
	break;

    case COLOUR_MONITOR:
	K_ttynum = 0;
	A_ttynum = 1;
	B_ttynum = 2;
	do_putchar = sun_putchar;
	panic("Console in eeprom is set to colour monitor");
	break;
    }
#endif /* NO_OUTPUT_ON_CONSOLE */
    printf("machine type = %x\n", cputype);
#ifndef NO_OUTPUT_ON_CONSOLE
    printf("console type = %x\n", Console);
#endif /* NO_OUTPUT_ON_CONSOLE */
    printf("system reg   = %x\n", fctlb((long) 0x40000000));
    printf("user dvma    = %x\n", fctlb((long) 0x50000000));
    printf("bus error    = %x\n", fctlb((long) 0x60000000));
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

char keytab[] = {
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

char sfttab[] = {
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

static int shifted, cntled, alted;

static void
keybd_rint(c)
long c;
{
    switch (c)	/* interpret control keys */
    {
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
#ifdef IOP
    if (shifted && cntled && c == 0x01) {
	/*
	** meta-shift-L1 is switch console<->Xwindows
	*/
	ev_ok = !ev_ok;
	console_enable(!ev_ok);
	printf("[Events %s]\n", ev_ok? "enabled" : "disabled");
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
    erase(); /* clear the screen */
    home_cursor();
}
#else /* IOP */

/*ARGSUSED*/
void
mouse_intr(c)
long c;
{
    /* ignore the mouse when no window server */
}

#endif /* IOP */
