/*	@(#)mouse.c	1.7	96/02/27 13:51:51 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * mouse.c
 *
 * Drivers for various AT/386 mice. Currently we've got support for mice that
 * "talk" mouse system's mouse, or microsoft protocol, and for logitech bus
 * mouse protocol.
 *
 * TODO:  a serial mouse is currently always assumed to be attached to
 * serial line 0 (COM1).  Instead it should be possible to specify the
 * serial line at runtime using iop_mousecontrol().
 *
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Kees Verstoep, Jun 1993
 *		unregister old mouse handler before setting a new one
 *	Kees Verstoep, Aug 1993
 *		included Xfree mouse protocol, rewritten serial mouse code
 *	Gregory J. Sharp, Aug 1995
 *		The mouse events were being interpreted too early.  They
 *		button and movement events were being sent as a separate
 *		event per button and per movement, making 3 button emulation
 *		and chord-middle impossible.  I caused button interpretation
 *		to occur at enqueued interrupt level and a whole event is
 *		sent and not lots of separate ones now.
 */
/*
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Thomas Roell
 * and David Dawes makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <type.h>
#include <iop/iop.h>
#include <bool.h>
#include <sys/proto.h>
#include <posix/termios.h>
#include "i386_proto.h"

#include "asy.h"
#include "mouse.h"

static void lb_intr(), serial_intr();
static void unset_mouse_handler();

/*
 * The following defines are copied from the Xfree86 mouse driver.
 * The iop_mouse_control() routine uses the IOP_ defines, so they
 * have to be converted.
 */
#define P_NONE	  -1
#define P_MS	   0	/* Microsoft */
#define P_MSC	   1	/* Mouse Systems Corp */
#define P_MM	   2	/* MMseries */
#define P_LOGI	   3	/* Logitech */
#define P_BM	   4	/* BusMouse */
#define P_LOGIMAN  5	/* MouseMan / TrackMan */
#define P_PS2	   6	/* PS/2 mouse */
#define	P_MMHIT    7	/* Hitachi tablet */

#define	BUTTON_PRESS	0x1000

struct mouse_info {
    int   iopMouseType;	/* IOP_ mouse type */
    int   mseType;	/* XFree86 mouse type */
    int   baudRate;
    int   sampleRate;
    int   lastButtons;
    int   chordMiddle;
    int   asy_reg;
    int   (*m_init)    _ARGS((IOPmouseparams *mp));
    void  (*m_enable)  _ARGS((void));
    void  (*m_disable) _ARGS((void));
} xf86Info = {
    -1, P_NONE, 0, 0, 0, 0, 0, NULL, NULL, NULL
};

/*
 * High level mouse interrupt handler
 */

void
xf86PostMseEvent(buttons, dx, dy)
    int buttons, dx, dy;
{
    /* check for changed buttons, and generate necessary event */
    if (buttons != xf86Info.lastButtons || dx || dy) {
	newevent(EV_ButtonPress, buttons, dx, dy);
	xf86Info.lastButtons = buttons;
    }
}

/*
 * Serial mouse support.
 */

static void
serial_doinit(reg, baud, lcr)
int reg;
int baud;
int lcr;
{
    int divisor;

    /* set divisor latch */
    divisor = ASY_BAUDRATE(baud);
    out_byte(reg + ASY_IER, 0);
    out_byte(reg + ASY_LCR, ASY_LCR_DLAB);
    out_byte(reg + ASY_DLLSB, divisor & 0xFF);
    out_byte(reg + ASY_DLMSB, (divisor >> 8) & 0xFF);

    /* turn on/off parity, stop bits, etc */
    out_byte(reg + ASY_LCR, lcr);

    /* hardware requires that SOUT2 is high */
    out_byte(reg + ASY_MCR, ASY_MCR_DTR|ASY_MCR_RTS|ASY_MCR_OUT2);

    /* enable receive, and line status change interrupts + THRE */
    out_byte(reg + ASY_IER, ASY_IER_RDA|ASY_IER_RLS|ASY_IER_THRE);

    xf86Info.asy_reg = reg;
}

static int
serial_init(mp)
    IOPmouseparams *mp;
{
    if (xf86Info.iopMouseType != mp->mp_type) {
	serial_doinit(MOUSE_ASY_REG, mp->mp_baudrate, MOUSE_ASY_LCR);

	setirq(MOUSE_ASY_IRQ, serial_intr);
	/* pic_enable(MOUSE_ASY_IRQ); */
    }
    return 1;
}

static void
serial_enable()
{
    pic_enable(MOUSE_ASY_IRQ);
}

static void
serial_disable()
{
    pic_disable(MOUSE_ASY_IRQ);
}

static void xf86MouseProtocol();
static void xf86SetupMouse();

#define MAX_INB_LOOP	1000

static void
serial_putc(asy, ch)
    int asy;
    int ch;
{
    register int count;

    for (count = 0; count < MAX_INB_LOOP; count++) {
	if ((in_byte(asy + ASY_LSR) & ASY_LSR_THRE) != 0) {
	    break;
	}
    }
    if (count >= MAX_INB_LOOP) {
	/* sanity check; shouldn't happen */
	printf("asy %d: mouse problem?\n", asy);
    }

    out_byte(asy + ASY_TXB, ch & 0xff);
}

static int
serial_puts(buf, len)
char *buf;
int len;
{
    int i;

    for (i = 0; i < len; i++) {
	serial_putc(xf86Info.asy_reg, buf[i]);
    }

    return len;
}

static void
do_mouse_proto(byte)
long byte;
{
    xf86MouseProtocol((unsigned char *) &byte, 1);
}

static void
receive_byte(asy)
int asy;
{
    /* Receive a byte from the serial port and give it to the XFree86 mouse
     * protocol code to turn it into proper button/movement events.
     */
    long byte;

    byte = (unsigned char) in_byte(asy + ASY_RXB);
    enqueue(do_mouse_proto, byte);
}

static void
serial_intr()
{
    int asy = xf86Info.asy_reg;
    register int count;

    /* Read and process all bytes available */
    for (count = 0; count < MAX_INB_LOOP; count++) {
	int iireg, lsreg;

	iireg = in_byte(asy + ASY_IIR);

	switch (iireg & ASY_IIR_MASK) {
	case ASY_IIR_PEND:
	    /* No more data pending */
	    return;

	case ASY_IIR_LSR:
	    /* Line status change */
	    if ((lsreg = in_byte(asy + ASY_LSR)) & ASY_LSR_DR) {
		receive_byte(asy);
	    } else {
		printf("mouse: line int: status = 0x%lx\n", lsreg);
	    }
	    break;

	case ASY_IIR_RXB:
	    /* Data available */
	    while ((lsreg = in_byte(asy + ASY_LSR)) & ASY_LSR_DR) {
		receive_byte(asy);
	    }
	    break;

	case ASY_IIR_TXB:
	    /* Transmitter holding register empty.
	     * These occur when we are telling the mouse to switch
	     * to the proper baudrate.
	     */
	    break;

	default:
	    lsreg = in_byte(asy + ASY_LSR);
	    printf("mouse: weird interrupt: iireg = 0x%x; lsreg = 0x%x\n",
		   iireg, lsreg);
	    return;
	}
    }

    if (count >= MAX_INB_LOOP) {
	/* sanity check; shouldn't happen */
	printf("asy intr %d: mouse problem?\n", asy);
    }
}

/*
 * LOGITECH BUS MOUSE
 *
 * Driver routines for a logitech bus mouse. This is one of the most
 * brain damaged mice I've ever seen, it interrupts constantly, even
 * though there's nothing to report.
 */
/* ARGSUSED */
int
lb_init(mp)
    IOPmouseparams *mp;
{
    if (xf86Info.iopMouseType != IOP_MOUSE_LB) {
	register int i;

	/*
	 * Put a magic byte into the signature register,
	 * and see wether it sticks
	 */
	out_byte(LB_CONFREG, LB_CONFIG);
	out_byte(LB_SIGNREG, 0xA5);
	for (i = 0; i < MAX_INB_LOOP; i++)
	     if (in_byte(LB_SIGNREG) == 0xA5) break;
	if (i == MAX_INB_LOOP) {
	    printf("iopsvr: Logitech bus mouse: wrong signature\n");
	    return 0;
	}

	setirq(LB_IRQ, lb_intr);
	out_byte(LB_CNTLREG, LB_ENABLE);
    }
    return 1;
}

void
lb_enable()
{
    pic_enable(LB_IRQ);
}

void
lb_disable()
{
    pic_disable(LB_IRQ);
}

/*
 * High level mouse interrupt handler
 */
#define encode_mouse(dx, dy, buttons) \
        ((((dx) & 0xFF) << 16) | (((dy) & 0xFF) << 8) | ((buttons) & 0xFF))
 
static void
lb_mouse_handler(arg)
    long arg;
{
    int buttons, dx, dy;
 
    dx = (int8)((arg >> 16) & 0xFF);
    dy = (int8)((arg >> 8) & 0xFF);
    buttons = (uint8)(arg & 0xFF);
    xf86PostMseEvent(buttons, dx, dy);
}

static void
lb_intr()
{
    static int old_buttons = 0;
    int buttons, dx, dy;

    /* read delta X movements */
    out_byte(LB_CNTLREG, LB_HC);
    dx = in_byte(LB_DATAREG) & 0x0F;
    out_byte(LB_CNTLREG, LB_HC|LB_SHL);
    dx = (char) (dx | ((in_byte(LB_DATAREG) & 0x0F) << 4));

    /* read delta Y movements, and button status */
    out_byte(LB_CNTLREG, LB_HC|LB_SXY);
    dy = in_byte(LB_DATAREG) & 0x0F;
    out_byte(LB_CNTLREG, LB_HC|LB_SXY|LB_SHL);
    buttons = in_byte(LB_DATAREG);
    dy = (dy | ((buttons & 0x0F) << 4));
    out_byte(LB_CNTLREG, LB_ENABLE); /* and HC = 0 */
    buttons = (buttons >> 5) & 0x07;

    /*
     * We're very careful with calling the high interrupt handler
     * since the bus mouse interrupts a lot (even when there's
     * nothing to report). So only report meaningful events.
     */
    if (buttons != old_buttons || dx || dy) {
	enqueue(lb_mouse_handler, (long) encode_mouse(dx, dy, buttons));
	old_buttons = buttons;
    }
}

/*
 * Generic mouse (de)installation routines
 */

static void
unset_mouse_handler()
{
    switch (xf86Info.mseType) {
    case P_NONE:
	break;
    case P_BM:
	lb_disable();
	unsetirq(LB_IRQ);
	break;
    case P_MS:
    case P_MSC:
    default:
	serial_disable();
	unsetirq(MOUSE_ASY_IRQ);
	break;
    }

    xf86Info.iopMouseType = -1;
    xf86Info.mseType = P_NONE;
    xf86Info.chordMiddle = 0;
    xf86Info.m_init = NULL;
    xf86Info.m_enable = NULL;
    xf86Info.m_disable = NULL;
}


static struct {
    int   m_type;
    char *m_name;
} mtypes[] = {
    { P_MSC,	"Mouse Sytems" },		/* IOP_MOUSE_MM */
    { P_MS,	"Microsoft" },			/* IOP_MOUSE_MS */
    { P_BM,	"Logitech Busmouse" },		/* IOP_MOUSE_LB */
    { P_MM,	"MM series" }, 			/* IOP_MOUSE_MMS */
    { P_LOGI,	"Logitech" },			/* IOP_MOUSE_LOGI */
    { P_LOGIMAN,"Logitech Mouseman" },		/* IOP_MOUSE_LOGIMAN */
    { P_PS2,	"PS/2" },			/* IOP_MOUSE_PS2 */
    { P_MMHIT,	"Hitachi PUMA Plus Tablet" }	/* IOP_MOUSE_HITAB */
};

errstat
mouse_init(mp)
    IOPmouseparams *mp;
{
    int type;

    if (mp->mp_type < 0 || mp->mp_type > sizeof(mtypes)/sizeof(mtypes[0])) {
	return STD_ARGBAD;
    }
    type = mtypes[mp->mp_type].m_type;

    if (xf86Info.mseType != type) {
	unset_mouse_handler();
    }

    if (type == P_BM) {
	xf86Info.m_init = lb_init;
	xf86Info.m_enable = lb_enable;
	xf86Info.m_disable = lb_disable;
    } else {
	xf86Info.m_init = serial_init;
	xf86Info.m_enable = serial_enable;
	xf86Info.m_disable = serial_disable;
    }

    if ((*xf86Info.m_init)(mp) == 0) {
	return STD_SYSERR;
    }

    printf("%s mouse initialized\n", mtypes[mp->mp_type].m_name);;

    xf86Info.iopMouseType = mp->mp_type;
    xf86Info.mseType = type;
    xf86Info.baudRate = mp->mp_baudrate;
    xf86Info.sampleRate = mp->mp_samplerate;
    xf86Info.chordMiddle = mp->mp_chordmiddle;

    return STD_OK;
}

void
mouse_enable()
{
    if (xf86Info.m_enable != NULL) {
	(*xf86Info.m_enable)();

	/* tell mouse to use proper baudrate */
	xf86SetupMouse();
    }
}

void
mouse_disable()
{
    if (xf86Info.m_disable != NULL) {
	(*xf86Info.m_disable)();
    }
}

/*
 * Mouse protocol support is taken almost verbatim from XFree86.
 */

#define	usleep(t)		(void) await((event) 0, (interval) (t / 1000))
#define write(fd, buf, size)	serial_puts(buf, size)

/* Line control bits. */
#define LC_STOP_BITS_SHIFT      2

static void
xf86SetMouseSpeed(old, new, cflag)
int old;
int new;
unsigned cflag;
{
    int parity, data_bits, stop_bits;
    int lcr;
    char *msg;

    if (cflag & PARODD) {
	parity = ASY_LCR_PEN;
    } else if  (cflag & PARENB) {
	parity = ASY_LCR_PEN | ASY_LCR_EPS;
    } else {
	parity = 0;
    }
    stop_bits = 1;
    data_bits = ((cflag & CS8) == CS8) ? ASY_LCR_8BIT : ASY_LCR_7BIT;

    lcr = parity | ((stop_bits - 1) << LC_STOP_BITS_SHIFT) | data_bits;

    /* We assume that the mouse has baudrate 'old', so we need to use that
     * to tell the mouse to switch to the baudrate 'new'.
     */
    serial_doinit(MOUSE_ASY_REG, old, lcr);

    switch (new) {
    case 9600: msg = "*q"; break;
    case 4800: msg = "*p"; break;
    case 2400: msg = "*o"; break;
    case 1200:
    default:   msg = "*n"; break;
    }

    serial_puts(msg, 2);

    /* wait a moment */
    await((event) 0, (interval) 100);

    /* switch to new baudrate */
    serial_doinit(MOUSE_ASY_REG, new, lcr);
}

unsigned short xf86MouseCflags[] =
{
	(CS7                   | CREAD | CLOCAL | HUPCL ),   /* MicroSoft */
	(CS8 | CSTOPB          | CREAD | CLOCAL | HUPCL ),   /* MouseSystems */
	(CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL ),   /* MMSeries */
	(CS8 | CSTOPB          | CREAD | CLOCAL | HUPCL ),   /* Logitech */
	0,						     /* BusMouse */
	(CS7                   | CREAD | CLOCAL | HUPCL ),   /* MouseMan,
                                                              [CHRIS-211092] */
	0,						     /* PS/2 */
	(CS8                   | CREAD | CLOCAL | HUPCL ),   /* mmhitablet */
};

/*
 * xf86SetupMouse --
 *	Sets up the mouse parameters
 */

static void
xf86SetupMouse()
{
      /*
      ** The following lines take care of the Logitech MouseMan protocols.
      **
      ** NOTE: There are different versions of both MouseMan and TrackMan!
      **       Hence I add another protocol P_LOGIMAN, which the user can
      **       specify as MouseMan in his Xconfig file. This entry was
      **       formerly handled as a special case of P_MS. However, people
      **       who don't have the middle button problem, can still specify
      **       Microsoft and use P_MS.
      **
      ** By default, these mice should use a 3 byte Microsoft protocol
      ** plus a 4th byte for the middle button. However, the mouse might
      ** have switched to a different protocol before we use it, so I send
      ** the proper sequence just in case.
      **
      ** NOTE: - all commands to (at least the European) MouseMan have to
      **         be sent at 1200 Baud.
      **       - each command starts with a '*'.
      **       - whenever the MouseMan receives a '*', it will switch back
      **	 to 1200 Baud. Hence I have to select the desired protocol
      **	 first, then select the baud rate.
      **
      ** The protocols supported by the (European) MouseMan are:
      **   -  5 byte packed binary protocol, as with the Mouse Systems
      **      mouse. Selected by sequence "*U".
      **   -  2 button 3 byte MicroSoft compatible protocol. Selected
      **      by sequence "*V".
      **   -  3 button 3+1 byte MicroSoft compatible protocol (default).
      **      Selected by sequence "*X".
      **
      ** The following baud rates are supported:
      **   -  1200 Baud (default). Selected by sequence "*n".
      **   -  9600 Baud. Selected by sequence "*q".
      **
      ** Selecting a sample rate is no longer supported with the MouseMan!
      ** Some additional lines in xf86Config.c take care of ill configured
      ** baud rates and sample rates. (The user will get an error.)
      **               [CHRIS-211092]
      */

  
      if (xf86Info.mseType == P_LOGIMAN)
        {
          xf86SetMouseSpeed(1200, 1200, xf86MouseCflags[P_LOGIMAN]);
          write(xf86Info.mseFd, "*X", 2);
          xf86SetMouseSpeed(1200, xf86Info.baudRate,
			    xf86MouseCflags[P_LOGIMAN]);
        }
      else if (xf86Info.mseType != P_BM && xf86Info.mseType != P_PS2) 
	{
	  xf86SetMouseSpeed(9600, xf86Info.baudRate,
			    xf86MouseCflags[xf86Info.mseType]);
	  xf86SetMouseSpeed(4800, xf86Info.baudRate, 
			    xf86MouseCflags[xf86Info.mseType]);
	  xf86SetMouseSpeed(2400, xf86Info.baudRate,
			    xf86MouseCflags[xf86Info.mseType]);
	  xf86SetMouseSpeed(1200, xf86Info.baudRate,
			    xf86MouseCflags[xf86Info.mseType]);

	  if (xf86Info.mseType == P_LOGI)
	    {
	      write(xf86Info.mseFd, "S", 1);
	      xf86SetMouseSpeed(xf86Info.baudRate, xf86Info.baudRate,
                                xf86MouseCflags[P_MM]);
	    }

	  if (xf86Info.mseType == P_MMHIT)
	  {
	    char speedcmd;

	    /*
	     * Initialize Hitachi PUMA Plus - Model 1212E to desired settings.
	     * The tablet must be configured to be in MM mode, NO parity,
	     * Binary Format.  xf86Info.sampleRate controls the sensativity
	     * of the tablet.  We only use this tablet for it's 4-button puck
	     * so we don't run in "Absolute Mode"
	     */
	    write(xf86Info.mseFd, "z8", 2);	/* Set Parity = "NONE" */
	    usleep(50000);
	    write(xf86Info.mseFd, "zb", 2);	/* Set Format = "Binary" */
	    usleep(50000);
	    write(xf86Info.mseFd, "@", 1);	/* Set Report Mode = "Stream" */
	    usleep(50000);
	    write(xf86Info.mseFd, "R", 1);	/* Set Output Rate = "45 rps" */
	    usleep(50000);
	    write(xf86Info.mseFd, "I\x20", 2);	/* Set Incrememtal Mode "20" */
	    usleep(50000);
	    write(xf86Info.mseFd, "E", 1);	/* Set Data Type = "Relative */
	    usleep(50000);

	    /* These sample rates translate to 'lines per inch' on the Hitachi
	       tablet */
	    if      (xf86Info.sampleRate <=   40) speedcmd = 'g';
	    else if (xf86Info.sampleRate <=  100) speedcmd = 'd';
	    else if (xf86Info.sampleRate <=  200) speedcmd = 'e';
	    else if (xf86Info.sampleRate <=  500) speedcmd = 'h';
	    else if (xf86Info.sampleRate <= 1000) speedcmd = 'j';
	    else                                  speedcmd = 'd';
	    write(xf86Info.mseFd, &speedcmd, 1);
	    usleep(50000);

	    write(xf86Info.mseFd, "\021", 1);	/* Resume DATA output */
	  }
	  else
	  {
	    if      (xf86Info.sampleRate <=   0)  write(xf86Info.mseFd, "O", 1);
	    else if (xf86Info.sampleRate <=  15)  write(xf86Info.mseFd, "J", 1);
	    else if (xf86Info.sampleRate <=  27)  write(xf86Info.mseFd, "K", 1);
	    else if (xf86Info.sampleRate <=  42)  write(xf86Info.mseFd, "L", 1);
	    else if (xf86Info.sampleRate <=  60)  write(xf86Info.mseFd, "R", 1);
	    else if (xf86Info.sampleRate <=  85)  write(xf86Info.mseFd, "M", 1);
	    else if (xf86Info.sampleRate <= 125)  write(xf86Info.mseFd, "Q", 1);
	    else                                  write(xf86Info.mseFd, "N", 1);
	  }
        }

#ifdef CLEARDTR_SUPPORT
      if (xf86Info.mseType == P_MSC && (xf86Info.mouseFlags & MF_CLEAR_DTR))
        {
          int val = TIOCM_DTR;
          ioctl(xf86Info.mseFd, TIOCMBIC, &val);
        }
      if (xf86Info.mseType == P_MSC && (xf86Info.mouseFlags & MF_CLEAR_RTS))
        {
          int val = TIOCM_RTS;
          ioctl(xf86Info.mseFd, TIOCMBIC, &val);
        }
#endif
}

static void
xf86MouseProtocol(rBuf, nBytes)
     unsigned char *rBuf;
     int nBytes;
{
  int                  i, buttons, dx, dy;
  static int           pBufP /* = 0 */;
  static unsigned char pBuf[8];

  static unsigned char proto[8][5] = {
    /*  hd_mask hd_id   dp_mask dp_id   nobytes */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MicroSoft */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MMSeries */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* Logitech */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* BusMouse */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MouseMan
                                                       [CHRIS-211092] */
    {	0xc0,	0x00,	0x00,	0x00,	3	},  /* PS/2 mouse */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MM_HitTablet */
  };
  
  for ( i=0; i < nBytes; i++) {
    /*
     * Hack for resyncing: We check here for a package that is:
     *  a) illegal (detected by wrong data-package header)
     *  b) invalid (0x80 == -128 and that might be wrong for MouseSystems)
     *  c) bad header-package
     *
     * NOTE: b) is a violation of the MouseSystems-Protocol, since values of
     *       -128 are allowed, but since they are very seldom we can easily
     *       use them as package-header with no button pressed.
     * NOTE/2: On a PS/2 mouse any byte is valid as a data byte. Furthermore,
     *         0x80 is not valid as a header byte. For a PS/2 mouse we skip
     *         checking data bytes.
     *         For resyncing a PS/2 mouse we require the two most significant
     *         bits in the header byte to be 0. These are the overflow bits,
     *         and in case of an overflow we actually lose sync. Overflows
     *         are very rare, however, and we quickly gain sync again after
     *         an overflow condition. This is the best we can do. (Actually,
     *         we could use bit 0x08 in the header byte for resyncing, since
     *         that bit is supposed to be always on, but nobody told
     *         Microsoft...)
     */
    if (pBufP != 0 && xf86Info.mseType != P_PS2 &&
	((rBuf[i] & proto[xf86Info.mseType][2]) != proto[xf86Info.mseType][3]
	 || rBuf[i] == 0x80))
      {
	pBufP = 0;          /* skip package */
      }

    if (pBufP == 0 &&
	(rBuf[i] & proto[xf86Info.mseType][0]) != proto[xf86Info.mseType][1])
      {
	/*
	 * Hack for Logitech MouseMan Mouse - Middle button
	 *
	 * Unfortunately this mouse has variable length packets: the standard
	 * Microsoft 3 byte packet plus an optional 4th byte whenever the
	 * middle button status changes.
	 *
	 * We have already processed the standard packet with the movement
	 * and button info.  Now post an event message with the old status
	 * of the left and right buttons and the updated middle button.
	 */

        /*
	 * Even worse, different MouseMen and TrackMen differ in the 4th
         * byte: some will send 0x00/0x20, others 0x01/0x21, or even
         * 0x02/0x22, so I have to strip off the lower bits. [CHRIS-211092]
	 */
	if ((xf86Info.mseType == P_MS || xf86Info.mseType == P_LOGIMAN)
          && (char)(rBuf[i] & ~0x23) == 0)
	  {
	    buttons = ((int)(rBuf[i] & 0x20) >> 4)
	      | (xf86Info.lastButtons & 0x05);
	    xf86PostMseEvent(buttons, 0, 0);
	  }

	continue;            /* skip package */
      }


    pBuf[pBufP++] = rBuf[i];
    if (pBufP != proto[xf86Info.mseType][4]) continue;

    /*
     * assembly full package
     */
    switch(xf86Info.mseType) {
      
    case P_LOGIMAN:	    /* MouseMan / TrackMan   [CHRIS-211092] */
    case P_MS:              /* Microsoft */
      if (xf86Info.chordMiddle)
	buttons = (((int) pBuf[0] & 0x30) == 0x30) ? 2 :
		  ((int)(pBuf[0] & 0x20) >> 3)
		  | ((int)(pBuf[0] & 0x10) >> 4);
      else
	buttons = (xf86Info.lastButtons & 2)
		  | ((int)(pBuf[0] & 0x20) >> 3)
		  | ((int)(pBuf[0] & 0x10) >> 4);
      dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
      dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
      break;
      
      
    case P_MSC:             /* Mouse Systems Corp */
      buttons = (~pBuf[0]) & 0x07;
      dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
      dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
      break;

    case P_MMHIT:           /* MM_HitTablet */
      buttons = pBuf[0] & 0x07;
      if (buttons != 0)
        buttons = 1 << (buttons - 1);
      dx = (pBuf[0] & 0x10) ?   pBuf[1] : - pBuf[1];
      dy = (pBuf[0] & 0x08) ? - pBuf[2] :   pBuf[2];
      break;
      
    case P_MM:              /* MM Series */
    case P_LOGI:            /* Logitech Mice */
      buttons = pBuf[0] & 0x07;
      dx = (pBuf[0] & 0x10) ?   pBuf[1] : - pBuf[1];
      dy = (pBuf[0] & 0x08) ? - pBuf[2] :   pBuf[2];
      break;
      
    case P_BM:              /* BusMouse */
      buttons = (~pBuf[0]) & 0x07;
      dx =   (char)pBuf[1];
      dy = - (char)pBuf[2];
      break;

    case P_PS2:		    /* PS/2 mouse */
      buttons = (pBuf[0] & 0x04) >> 1 |       /* Middle */
	        (pBuf[0] & 0x02) >> 1 |       /* Right */
		(pBuf[0] & 0x01) << 2;        /* Left */
      dx = (pBuf[0] & 0x10) ?    pBuf[1]-256  :  pBuf[1];
      dy = (pBuf[0] & 0x20) ?  -(pBuf[2]-256) : -pBuf[2];
      break;
    }

    xf86PostMseEvent(buttons, dx, dy);
    pBufP = 0;
  }
}
