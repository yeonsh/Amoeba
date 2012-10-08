/*	@(#)ioctl.c	1.6	96/02/27 14:21:25 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#if !VERY_SIMPLE && !NO_IOCTL

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "tty/term.h"

#include <cmdreg.h>
#include <stderr.h>
#include <string.h>
#include <posix/termios.h>
#include <tty/tty_tios.h>
#include "sys/proto.h"

static void flush(threadno, iop, flag)
register struct io *iop;
{
  while (ttythread[threadno] == 0 && flag & _WRITE &&
					iop->io_oqueue.q_nchar != 0) {
	iop->io_state |= SFLUSH;
	(void) await((event) &iop->io_state, 0L);
  }
  if (flag & _WRITE && iop->io_state & SOSLEEP) {
	iop->io_state &= ~SOSLEEP;
	wakeup((event) &iop->io_oqueue);
  }
  if (ttythread[threadno])
	return;
  if (flag & _READ) {
	flushqueue(&iop->io_iqueue);
	iop->io_nbrk = 0;
  }
}

tty_control(threadno, iop, hdr, buf, size)
register struct io *iop;
register header *hdr;
register char *buf;
{
  register n;
  register i;
  register char *p;
  register newbrk = 0;

  n = hdr->h_size;
  if (size < n)
	n = size;
  for (; --n >= 0; buf++) {
	switch (*buf & (~BITMASK & 0xFF)) {
	case _SETFLAG:
		if ((unsigned) (*buf & BITMASK) < NFLAGS * 8)
			setflag(iop, *buf & BITMASK);
		break;
	case _CLRFLAG:
		if ((unsigned) (*buf & BITMASK) < NFLAGS * 8)
			clrflag(iop, *buf & BITMASK);
		break;
	case _SETSPEED:
		if (iop->io_funcs->cd_baud && iop->io_speed != (*buf & BITMASK)
		    && (*iop->io_funcs->cd_baud)(iop, *buf & BITMASK))
			iop->io_speed = *buf & BITMASK;
		break;
	default:
		switch (*buf & BITMASK) {
		case _SETINTR:
			p = iop->io_intr;
			break;
		case _SETIGN:
			p = iop->io_ign;
			break;
		case _SETNPRNT:
			p = iop->io_nprint;
			break;
		case _SETEOF:
			newbrk = 1;
			p = iop->io_eof;
			break;
		case _SETBRK:
			newbrk = 1;
			p = iop->io_brk;
			break;
		case _SETTAB:
			if (--n >= 0)
				iop->io_tabw = *++buf & 0xFF;
			continue;
		case _SETERASE:
			if (--n >= 0)
				iop->io_erase = *++buf;
			continue;
		case _SETWERASE:
			if (--n >= 0)
				iop->io_werase = *++buf;
			continue;
		case _SETKILL:
			if (--n >= 0)
				iop->io_kill = *++buf;
			continue;
		case _SETRPRNT:
			if (--n >= 0)
				iop->io_rprnt = *++buf;
			continue;
		case _SETFLUSH:
			if (--n >= 0)
				iop->io_flush = *++buf;
			continue;
		case _SETLNEXT:
			if (--n >= 0)
				iop->io_lnext = *++buf;
			continue;
		case _SETISTOP:
			if (--n >= 0)
				iop->io_istop = *++buf;
			continue;
		case _SETISTART:
			if (--n >= 0)
				iop->io_istart = *++buf;
			continue;
		case _SETOSTOP:
			if (--n >= 0)
				iop->io_ostop = *++buf;
			continue;
		case _SETOSTART:
			if (--n >= 0)
				iop->io_ostart = *++buf;
			continue;
		case _FLUSH:
			if (--n >= 0)
				flush(threadno, iop, *++buf);
			continue;
		case _SETDTR:
			if (iop->io_funcs->cd_setDTR)
				(*iop->io_funcs->cd_setDTR)(iop, 1);
			continue;
		case _CLRDTR:
			if (iop->io_funcs->cd_setDTR)
				(*iop->io_funcs->cd_setDTR)(iop, 0);
			continue;
		case _SETBREAK:
			if (iop->io_funcs->cd_setBRK)
				(*iop->io_funcs->cd_setBRK)(iop, 1);
			continue;
		case _CLRBREAK:
			if (iop->io_funcs->cd_setBRK)
				(*iop->io_funcs->cd_setBRK)(iop, 0);
			continue;
		case _FLAGS:
			if (n < NFLAGS) {
				n = 0;
				continue;
			}
			for (p = iop->io_flags; p < &iop->io_flags[NFLAGS]; p++)
				*p = *++buf;
			n -= NFLAGS;
			continue;
		default:
			printf("bad ioctl %x\n", *buf);
			continue;
		}
		/* come here only when we have to do a bitmap */
		for (i = 0; i < BMSIZE; i++)
			p[i] = 0;
		if ((*buf & BITMASK) == _SETBRK)
			setbit(p, '\n');
		if (--n >= 0 && (i = *++buf) <= n && i >= 0)
			while (--i >= 0) {
				buf++;
				setbit(p, *buf & 0xFF);
				n--;
			}
	}
  }
  /* If new break and/or end of file characters have been defined, we must
   * re-count the number of break characters.
   */
  if (newbrk) {
	register c;

	iop->io_nbrk = 0;
	while ((c = nextc(&iop->io_iqueue)) != -1)
		if (!(c & ~0xFF) &&
		    (tstbit(iop->io_brk, c) || tstbit(iop->io_eof, c)))
			iop->io_nbrk++;
  }
  hdr->h_status = STD_OK;
  return 0;
}

/*ARGSUSED*/
tty_status(threadno, iop, hdr, buf, size)
register struct io *iop;
register header *hdr;
register char *buf;
{
  register char *p;

  hdr->h_status = STD_OK;
  switch (hdr->h_extra) {
  case _NREAD:
	* (long *) buf = (long) iop->io_iqueue.q_nchar;
	return 4;
  case _NWRITE:
	* (long *) buf = (long) iop->io_oqueue.q_nchar;
	return 4;
  case _SPEED:
	* (short *) buf = iop->io_speed;
	return 2;
  case _GETFLAGS:
	for (p = iop->io_flags; p < &iop->io_flags[NFLAGS]; p++)
		*buf++ = *p;
	return NFLAGS;
  default:
	hdr->h_status = STD_COMBAD;
	return 0;
  }
}

/*
** TIOS routines.
*/

/*ARGSUSED*/
tios_get_attr(threadno, iop, hdr, t)
register struct io *iop;
register header *hdr;
register struct termios  *t;
{
	register int n;

	/*
	** Map tty bitfields to termios bit fields...
	** XXX: Not implemented, see ioctl: BRKINT, IGNBRK, IGNCR, INPCK, 
	**				    PARMRK
	*/
	t->c_iflag = 0;
	if (tstflag(iop, _ICRNL))
		t->c_iflag |= ICRNL;
	if (tstflag(iop, _INLCR))
		t->c_iflag |= INLCR;
	if (tstflag(iop, _IXOFF))
		t->c_iflag |= IXOFF;
	if (tstflag(iop, _IXON))
		t->c_iflag |= IXON;

	/*
	** Output processing enabled...
	** XXX: For the time being we don't implement output processing
	**      bit fields.
	*/
	t->c_oflag = 0;
	if (tstflag(iop, _OPOST))
		t->c_oflag |= OPOST;
	if (tstflag(iop, _XTAB))
		t->c_oflag |= XTAB;

	/*
	** Control flags...
	** XXX: CLOCAL, CREAD, CSIZE, CSTOPB, HUPCL, PARENB, PARODD
	*/
	t->c_cflag = CS8|CREAD;

	/*
	** Set local flags..
	** XXX: IEXTEN, TOSTOP, ECHONL
	*/
	t->c_lflag = 0;
	if (tstflag(iop, _ECHO))
		t->c_lflag |= ECHO;

	if (tstflag(iop, _CRTERA))
		t->c_lflag |= ECHOE;

	if (tstflag(iop, _CRTKIL))
		t->c_lflag |= ECHOK;

	if (tstflag(iop, _ISIG))
		t->c_lflag |= ISIG;

	if (tstflag(iop, _ICANON) || tstflag(iop, _LNEXTCH))
		t->c_lflag |= ICANON;

	/*
	** XXX: VMIN, VTIM.
	*/
	for (n = 0; n != BMSIZE * 8; n++)
		if (tstbit(iop->io_eof, n))
			break;
	t->c_cc[VEOF]	= (cc_t) n;

	for (n = 0; n != BMSIZE * 8; n++)
		if (tstbit(iop->io_brk, n) && !tstbit(iop->io_brk, CNTRL('J')))
			break;
	t->c_cc[VEOL]	= (n != BMSIZE * 8)? (cc_t) n: _POSIX_VDISABLE;

	t->c_cc[VERASE]	= (cc_t) iop->io_erase;
	t->c_cc[VINTR]	= (cc_t) iop->io_intr_char;
	t->c_cc[VKILL]	= (cc_t) iop->io_kill;
	t->c_cc[VMIN]	= 1;
	t->c_cc[VQUIT]	= (cc_t) iop->io_quit_char;
	t->c_cc[VSUSP]	= (cc_t) iop->io_susp_char;
	t->c_cc[VTIME]	= 0;
	t->c_cc[VSTART]	= (cc_t) iop->io_ostart;
	t->c_cc[VSTOP]	= (cc_t) iop->io_ostop;

	/*
	 * Set baud rate (ispeed == ospeed)
	 */
	cfsetospeed(t, iop->io_speed + 1);

	hdr->h_status = STD_OK;
}


/*ARGSUSED*/
tios_set_attr(threadno, iop, hdr, t)
register struct io *iop;
register header *hdr;
register struct termios *t;
{
	int speed;

	/*
	** Map termios bitfields to tty bit fields...
	** XXX: BRKINT, IGNBRK, IGNCR, INPCK, PARMRK
	*/
	clrflag(iop, _ICRNL);
	if (t->c_iflag & ICRNL)
		setflag(iop, _ICRNL);
	clrflag(iop, _INLCR);
	if (t->c_iflag & INLCR)
		setflag(iop, _INLCR);
	clrflag(iop, _IXOFF);
	if (t->c_iflag & IXOFF)
		setflag(iop, _IXOFF);
	clrflag(iop, _IXON);
	if (t->c_iflag & IXON)
		setflag(iop, _IXON);

	/*
	** Output processing enabled...
	*/
	clrflag(iop, _OPOST);
	if (t->c_oflag & OPOST)
		setflag(iop, _OPOST);
	clrflag(iop, _XTAB);
	if (t->c_oflag & XTAB)
		setflag(iop, _XTAB);

	/*
	** Set local flags..
	** XXX: IEXTEN, TOSTOP, ECHONL
	*/
	clrflag(iop, _ECHO);
	if (t->c_lflag & ECHO)
		setflag(iop, _ECHO);
	clrflag(iop, _CRTERA);
	if (t->c_lflag & ECHOE)
		setflag(iop, _CRTERA);
	clrflag(iop, _CRTKIL);
	if (t->c_lflag & ECHOK)
		setflag(iop, _CRTKIL);
	clrflag(iop, _ICANON);
	clrflag(iop, _LNEXTCH);
	clrflag(iop, _WERASECH);
	if (t->c_lflag & ICANON) {
		setflag(iop, _ICANON);
		setflag(iop, _LNEXTCH);
		setflag(iop, _WERASECH);
	}
	clrflag(iop, _ISIG);
	if (t->c_lflag & ISIG)
		setflag(iop, _ISIG);
	clrflag(iop, _NOFLUSH);
	if (t->c_lflag & NOFLSH)
		setflag(iop, _NOFLUSH);

	/*
	** Clear bitmaps.
	** XXX: VMIN, VTIM.
	*/
	(void) memset((_VOIDSTAR) iop->io_eof, 0, BMSIZE);
	(void) memset((_VOIDSTAR) iop->io_intr, 0, BMSIZE);

	setbit(iop->io_eof, t->c_cc[VEOF]);
	if (t->c_cc[VEOL] != _POSIX_VDISABLE) {
		(void) memset((_VOIDSTAR) iop->io_brk, 0, BMSIZE);
		setbit(iop->io_brk, t->c_cc[VEOL]);
		setbit(iop->io_brk, CNTRL('J'));
	}

	iop->io_erase	  = (char) t->c_cc[VERASE];
	iop->io_kill	  = (char) t->c_cc[VKILL];
	iop->io_intr_char = (char) t->c_cc[VINTR];
	iop->io_quit_char = (char) t->c_cc[VQUIT];
	iop->io_susp_char = (char) t->c_cc[VSUSP];
	iop->io_ostart    = (char) t->c_cc[VSTART];
	iop->io_ostop     = (char) t->c_cc[VSTOP];

	/*
	** Set interrupt bits in interrupt bitmask.
	*/
	setbit(iop->io_intr, iop->io_intr_char);
	setbit(iop->io_intr, iop->io_quit_char);
	setbit(iop->io_intr, iop->io_susp_char);

	/*
	 * Set baud rate (ispeed == ospeed)
	 */
	speed = cfgetospeed(t) - 1; /* convert to tty server speed index */
	if (speed >= B_50 && speed <= B_19200 && iop->io_speed != speed &&
	    iop->io_funcs->cd_baud && (*iop->io_funcs->cd_baud)(iop, speed))
		iop->io_speed = speed;

	hdr->h_status = STD_OK;
	return(0);
}

#endif /* !VERY_SIMPLE && !NO_IOCTL */
