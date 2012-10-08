/*	@(#)iotask.c	1.10	96/02/27 14:21:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tty server, manage a tty.
**
** Author: Sjoerd Mullender
** Change log:
** ----------
** Peter Bosch, 290190, peterb@cwi.nl:
**
** TIOS interfaces implemented. Merely map the tios parameters to tty server
** parameters. Implemented functions: tios_get_attr, tios_set_attr. Changed
** ttyint, send the signal number to the handler of a client instead of the
** typed characters. Not supported features to tios_{get, set}_attr:
** iflags: BRKINT, IGNBRK, IGNCR, INPCK, PARMRK.
** oflags: ...
** cflags: CLOCAL, CREAD, CSIZE, CSTOPB, HUPCL, PARENB, PARODD.
** lflags: ECHONL, IEXTEN, TOSTOP.
** c_cc:   VMIN, VTIM.
** Not (yet) supported routines: cfgetospeed, cfsetospeed, cfgetispeed,
** cfsetispeed, tcsendbreak, tcdrain, tcflush, tcflow, tcgetpgrp, tcsetpgrp.
**
** Modified: Gregory J. Sharp, September 1991
**		ttydump() should be the result of an STD_STATUS.  Servers do
**		not need kstat to give status info.
** Modified: Leendert van Doorn, November 1991
**		Removed SMALL_KERNEL defines which guarded ttydump().
*/

#include "ailamoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "file.h"
#include "tty/term.h"
#include "assert.h"
INIT_ASSERT
#include "module/prv.h"
#include "module/rnd.h"
#include "posix/termios.h"
#include "class/tios.h"
#include "string.h"
#include "exception.h"
#include "sys/proto.h"

#ifndef IOTHREAD_STKSIZ
#define IOTHREAD_STKSIZ	0 /* default size */
#endif
#ifndef TTYINT_STKSIZ
#define TTYINT_STKSIZ	0 /* default size */
#endif

#define IOBUFSIZ	1024	/* size of request buffer */
#define SAME_ENDIAN(w) ( ((w) & _AIL_ENDIAN) == (_ailword & _AIL_ENDIAN) )

/* Initial speeds of the ttys */
extern short tty_speeds[];

static uint16 threadnr;
static port ioport;


/* We must declare these somewhere... */
struct io *tty;
char *ttythread;

/*
 * Catch a putsig from the client.
 * The io thread is continued if it was sleeping.
 */
/*ARGSUSED*/
static void
tty_catch(ttynr, signo)
int ttynr;
signum signo;
{
  register struct io *iop;

  compare(ttynr, <, (int) nttythread);
  ttythread[ttynr] = 1;
  /* wakeup all sleeping threads since we don't know which is the right one */
  for (iop = &tty[0]; iop < &tty[ntty]; iop++) {
	wakeup((event) &iop->io_iqueue);
	wakeup((event) &iop->io_oqueue);
	if (iop->io_state & SFLUSH) {
  		iop->io_state &= ~SFLUSH;
		wakeup((event) &iop->io_state);
	}
  }
}

void
iothread()
{
  short threadno = threadnr++;
  header hdr;
  char *buf;
  rights_bits rights;
  register int		ttynr;
  register struct io *	iop;
  register int		n;
  struct termios 	tio;
  char *tc_marshal(), *tc_unmarshal();

  compare(threadno, >=, 0);
  compare(threadno, <, (int) nttythread);
  buf = aalloc((vir_bytes) IOBUFSIZ, 0);
  getsig(tty_catch, (uint16) threadno);
  for (;; 
#ifdef USE_AM6_RPC
  rpc_putrep(&hdr, (bufptr) buf, (bufsize) n)
#else
  putrep(&hdr, (bufptr) buf, (bufsize) n)
#endif
  ) {
	hdr.h_port = ioport;
  	ttythread[threadno] = 0;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, (bufptr) buf, IOBUFSIZ);
#else
	n = getreq(&hdr, (bufptr) buf, IOBUFSIZ);
#endif
	compare(n, !=, RPC_FAILURE);
	ttynr = prv_number(&hdr.h_priv);
	if (ttynr < 0 || ttynr >= (int) ntty) {
		hdr.h_status = STD_CAPBAD;
		n = 0;
		continue;
	}
	iop = &tty[ttynr];
	if (prv_decode(&hdr.h_priv, &rights, &iop->io_random) < 0)
		if (iop->io_state & SNEWRAND &&
		    prv_decode(&hdr.h_priv, &rights, &iop->io_newrand) == 0) {
			iop->io_state &= ~SNEWRAND;
			iop->io_random = iop->io_newrand;
		} else {
			hdr.h_status = STD_CAPBAD;
			n = 0;
			continue;
		}
	switch (hdr.h_command) {
	case TTQ_READ:
	case TTQ_TIME_READ:
		if ((rights & RGT_READ) == 0)
			break;
		n = tty_read(threadno, iop, &hdr, buf, IOBUFSIZ);
		continue;
	case TTQ_WRITE:
		if ((rights & RGT_WRITE) == 0)
			break;
		n = tty_write(threadno, iop, &hdr, buf, n);
		continue;
#if !VERY_SIMPLE && !NO_IOCTL
	case TIOS_GETATTR:
		if ((rights & RGT_READ) == 0)
			break;
		(void)tios_get_attr(threadno, iop, &hdr, &tio);
		n = tc_marshal(buf, tio, SAME_ENDIAN(hdr.h_extra)) - buf;
		continue;

	case TIOS_SETATTR:
		if ((rights & RGT_CONTROL) == 0)
			break;
		(void) tc_unmarshal(buf, &tio, SAME_ENDIAN(hdr.h_size));
		n = tios_set_attr(threadno, iop, &hdr, &tio);
		continue;

	case TTQ_CONTROL:
		if ((rights & RGT_CONTROL) == 0)
			break;
		n = tty_control(threadno, iop, &hdr, buf, n);
		continue;
	case TTQ_STATUS:
		if ((rights & RGT_READ) == 0)
			break;
		n = tty_status(threadno, iop, &hdr, buf, IOBUFSIZ);
		continue;
	case TTQ_INTCAP:
		if ((rights & RGT_CONTROL) == 0)
			break;
		n = tty_intcap(iop, &hdr, buf, n);
		continue;
#endif /* !VERY_SIMPLE && !NO_IOCTL */
	case STD_RESTRICT:
		(void) prv_encode(&hdr.h_priv, (objnum) ttynr,
			rights & (rights_bits) hdr.h_extra, &iop->io_random);
		/* fall through */
	case STD_TOUCH:
		n = 0;
		hdr.h_status = STD_OK;
		continue;
	case STD_INFO:
		(void) strcpy(buf, "+");
		hdr.h_size = n = strlen(buf) + 1;
		hdr.h_status = STD_OK;
		continue;
	case STD_STATUS:
		hdr.h_size = n = ttydump(buf, buf + IOBUFSIZ);
		hdr.h_status = STD_OK;
		continue;
	default:
		hdr.h_status = STD_COMBAD;
		n = 0;
		continue;
	}
	hdr.h_status = STD_CAPBAD;
	n = 0;
  }
}

#if !VERY_SIMPLE
#if 0 /* no one actually uses this! */
hangup(n)
long n;
{
  tty[n].io_state |= SHANGUP;
  if (intsleeping) {
	intsleeping = 0;
	wakeup((event) &intsleeping);
  }
}
#endif

extern void ttyint();
#endif /* !VERY_SIMPLE */

void
initty()
{
  capability cap;
  register struct io *iop;
  register struct cdevsw *cdp;
  register n;

  uniqport(&ioport);
  priv2pub(&ioport, &cap.cap_port);

  ttythread = aalloc((vir_bytes) nttythread, 0);
  n = 1;
#ifdef USART
  n++;
#endif /* USART */
#ifdef CDC
  n++;
#endif /* CDC */
#ifdef ASY
  n++;
#endif /* ASY */
  tty = (struct io *) aalloc((vir_bytes) ntty * sizeof (struct io), 0);
  cdp = (struct cdevsw *) aalloc((vir_bytes) n * sizeof (struct cdevsw), 0);
  for (iop = &tty[0], n = 0; iop < &tty[ntty]; iop++, n++) {
	flushqueue(&iop->io_oqueue);		/* initialise output queue */
	flushqueue(&iop->io_iqueue);		/* initialise input queue */
	uniqport(&iop->io_random);
	(void) prv_encode(&cap.cap_priv, (objnum) n, PRV_ALL_RIGHTS, &iop->io_random);
	dirnappend(TTY_SVR_NAME, n, &cap);
#if !VERY_SIMPLE
#if !NO_IOCTL
	setflag(iop, _ISTRIP);
	setflag(iop, _ICRNL);
	setflag(iop, _IXON);
	setflag(iop, _IXOFF);
	setflag(iop, _IXANY);
	setflag(iop, _ISIG);
	setflag(iop, _ICANON);
	setflag(iop, _WERASECH);
	setflag(iop, _ECHO);
	setflag(iop, _RPRNTCH);
	setflag(iop, _LNEXTCH);
	setflag(iop, _FLUSHCH);
	setflag(iop, _CRTBS);
	setflag(iop, _CRTERA);
	setflag(iop, _CRTKIL);
	setflag(iop, _CTLECHO);
	setflag(iop, _OPOST);
	setflag(iop, _ONLCR);
	setflag(iop, _XTAB);
	iop->io_speed = tty_speeds[n];
#endif /* !NO_IOCTL */
	setbit(iop->io_eof, CNTRL('D'));
	setbit(iop->io_brk, CNTRL('J'));
	setbit(iop->io_intr, CNTRL('C'));
	setbit(iop->io_intr, CNTRL('\\'));
	setbit(iop->io_intr, CNTRL('Z'));
	iop->io_intr_char = CNTRL('C');
	iop->io_quit_char = CNTRL('\\');
	iop->io_susp_char = _POSIX_VDISABLE;
	iop->io_tabw = 8;
	iop->io_erase = CNTRL('H');
	iop->io_werase = CNTRL('W');
	iop->io_kill = CNTRL('U');
	iop->io_rprnt = CNTRL('R');
	iop->io_flush = CNTRL('O');
	iop->io_lnext = CNTRL('V');
	iop->io_ostop = CNTRL('S');
	iop->io_ostart = CNTRL('Q');
	iop->io_istop = CNTRL('S');
	iop->io_istart = CNTRL('Q');
#endif /* !VERY_SIMPLE */
  }
  n = 0;
  n += uart_init(cdp, &tty[0]);
  for (iop = tty; iop < &tty[n] && iop < &tty[ntty]; iop++)
	iop->io_funcs = cdp;
  cdp++;

#ifdef USART
  iop = &tty[n];
  n += iSBC_init(cdp, iop);
  for (; iop < &tty[n] && iop < &tty[ntty]; iop++)
	iop->io_funcs = cdp;
  cdp++;
#endif /* USART */

#ifdef CDC
  iop = &tty[n];
  n += CDC_init(cdp, iop);
  for (; iop < &tty[n] && iop < &tty[ntty]; iop++)
	iop->io_funcs = cdp;
  cdp++;
#endif /* CDC */

#ifdef ASY
  iop = &tty[n];
  n += asy_init(cdp, iop);
  for (; iop < &tty[n] && iop < &tty[ntty]; iop++)
	iop->io_funcs = cdp;
  cdp++;
#endif /* ASY */
}

void
inittythreads()
{
  void NewKernelThread();
  register n;

  for (n = 0; n < (int) nttythread; n++)
	NewKernelThread(iothread, (vir_bytes) IOTHREAD_STKSIZ);
#if !VERY_SIMPLE
  NewKernelThread(ttyint, (vir_bytes) TTYINT_STKSIZ);
#endif
}

#if !VERY_SIMPLE && !NO_IOCTL
static speedtab[] = {
	50,75,110,134,150,200,300,600,1200,1800,2400,4800,9600,19200,
};
#endif /* !VERY_SIMPLE && !NO_IOCTL */

int
ttydump(begin, end)
char *	begin;
char *	end;
{
  char *	p;
  register struct io *iop;
  register n;

#if VERY_SIMPLE
  p = bprintf(begin, end, "\ntty i #ci #co   stat ustat\n");
#else
#if NO_IOCTL
  p = bprintf(begin, end, "\ntty i #ci #co #br   stat ustat\n");
#else
  p = bprintf(begin, end,
		"\ntty i #ci #co #br   stat f0 f1 f2 f3 f4 f5 speed ustat\n");
#endif /* NO_IOCTL */
#endif /* VERY_SIMPLE */

  for (iop = &tty[0], n = 0; n < (int) ntty; iop++, n++) {
#if VERY_SIMPLE
	p = bprintf(p, end, "%3d %c%4d%4d %06lx ", n, ttythread[n] ? 'y' : 'n',
	       iop->io_iqueue.q_nchar, iop->io_oqueue.q_nchar, iop->io_state);
#else
#if NO_IOCTL
	p = bprintf(p, end, "%3d %c%4d%4d%4d %06lx ", n, ttythread[n] ? 'y' : 'n',
	       iop->io_iqueue.q_nchar, iop->io_oqueue.q_nchar,
	       iop->io_nbrk, iop->io_state);
#else
	p = bprintf(p, end,
		"%3d %c%4d%4d%4d %06lx %02x %02x %02x %02x %02x %02x %5d ", n,
		ttythread[n] ? 'y' : 'n',
		iop->io_iqueue.q_nchar, iop->io_oqueue.q_nchar,
		iop->io_nbrk, iop->io_state,
		iop->io_flags[0]&0xFF, iop->io_flags[1]&0xFF,
		iop->io_flags[2]&0xFF, iop->io_flags[3]&0xFF,
		iop->io_flags[4]&0xFF, iop->io_flags[5]&0xFF,
		speedtab[iop->io_speed]);
#endif /* NO_IOCTL */
#endif /* VERY_SIMPLE */
	p += (*iop->io_funcs->cd_status)(p, end, iop);
    }
    p = bprintf(p, end, "\n");
    return p - begin;
}
