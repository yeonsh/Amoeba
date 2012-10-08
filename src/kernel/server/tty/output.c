/*	@(#)output.c	1.6	96/02/27 14:21:36 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "tty/term.h"
#include "assert.h"
INIT_ASSERT
#include "cmdreg.h"
#include "stderr.h"
#include "sys/proto.h"

#if !VERY_SIMPLE && !NO_IOCTL
/* high water marks; one entry for each possible baud rate */
uint16 hiwater[] = {
	100,100,100,100,100,100,200,200,400,400,400,500,500,500,500,
};

/* low water marks; one entry for each possible baud rate */
uint16 lowater[] = {
	 30, 30, 30, 30, 30, 30, 50, 50,120,120,120,125,125,125,125,
};
#endif /* !VERY_SIMPLE && !NO_IOCTL */

#if !VERY_SIMPLE || RTSCTS || XONXOFF
/*
 * Send a start character (^Q) or force RTS high to indicate that internal
 * buffers have drained enough to accept more input.
 */
iresume(iop)
struct io *iop;
{
#if VERY_SIMPLE
#if XONXOFF
  insc(&iop->io_oqueue, CNTRL('Q'));
  ttstart(iop);
#endif /* XONXOFF */
#if RTSCTS
  if (iop->io_funcs->cd_iresume)
	(*iop->io_funcs->cd_iresume)(iop);
#endif /* RTSCTS */
#else
  if (tstflag(iop, _IXOFF)) {
	insc(&iop->io_oqueue, iop->io_istop);
	ttstart(iop);
  }
  if (tstflag(iop, _IMODEM) && iop->io_funcs->cd_iresume)
	(*iop->io_funcs->cd_iresume)(iop);
#endif /* VERY_SIMPLE */
}

/*
 * Send a stop character (^S) or force RTS low to indicate that internal
 * buffers are getting full.
 */
isuspend(iop)
struct io *iop;
{
#if VERY_SIMPLE
#if XONXOFF
  insc(&iop->io_oqueue, CNTRL('S'));
  ttstart(iop);
#endif /* XONXOFF */
#if RTSCTS
  if (iop->io_funcs->cd_isusp)
	(*iop->io_funcs->cd_isusp)(iop);
#endif /* RTSCTS */
#else
  if (tstflag(iop, _IXOFF)) {
	insc(&iop->io_oqueue, iop->io_istart);
	ttstart(iop);
  }
  if (tstflag(iop, _IMODEM) && iop->io_funcs->cd_isusp)
	(*iop->io_funcs->cd_isusp)(iop);
#endif /* VERY_SIMPLE */
}
#endif /* !VERY_SIMPLE || RTSCTS || XONXOFF */

/*
 * Write a character to a terminal if there are any waiting to be written.
 * Returns true if a character was written.
 */
void
writeint(arg)
long arg;
{
  register struct io *iop;
  register c;

  compare((uint16) arg, <, ntty);
  iop = &tty[(uint16) arg];
  do {
	if (
#if !VERY_SIMPLE || XONXOFF
	    iop->io_state & SOSUSP ||
#endif /* !VERY_SIMPLE || XONXOFF */
	    (c = getc(&iop->io_oqueue)) == -1) {
		/* there is nothing to write */
		iop->io_state &= ~SOWRITE;		/* not busy writing */
#if !VERY_SIMPLE && !NO_IOCTL
		if (iop->io_state & SFLUSH && iop->io_oqueue.q_nchar == 0) {
			/* wake up thread waiting for output queue to drain */
			iop->io_state &= ~SFLUSH;
			wakeup((event) &iop->io_state);
		}
#endif /* !VERY_SIMPLE && !NO_IOCTL */
		return;
	}
  } while ((*iop->io_funcs->cd_write)(iop, c) == 0);
  iop->io_state |= SOWRITE;
  if (iop->io_state & SOSLEEP && iop->io_oqueue.q_nchar <= LOWATER(iop)
#if !VERY_SIMPLE && !NO_IOCTL
      && (iop->io_state & SFLUSH) == 0
#endif /* !VERY_SIMPLE && !NO_IOCTL */
     ) {
	iop->io_state &= ~SOSLEEP;
	wakeup((event) &iop->io_oqueue);
  }
}

ttstart(iop)
register struct io *iop;
{
  if ((iop->io_state & SOWRITE) == 0)
	writeint((long) (iop - tty));
}

/*
 * Queue a character in the output queue after doing some output processing.
 */
void
outchar(iop, c)
register struct io *iop;
{
  c &= 0xFF;
#if !VERY_SIMPLE
  if (tstbit(iop->io_nprint, c))	/* we don't print some chars */
	  return;
  if (tstflag(iop, _OSTRIP))		/* strip to 7 bits */
	c &= 0x7F;
  switch (c) {				/* some chars need special processing */
  case '\n':
	if (tstflag(iop, _OPOST) && tstflag(iop, _ONLCR)) {
		iop->io_state |= SOLF;
		outchar(iop, '\r');
		iop->io_state &= ~SOLF;
	}
	if (tstflag(iop, _ONLRET))
		iop->io_ocol = 0;
	break;
  case '\r':
	if (tstflag(iop, _OPOST)) {
		if (tstflag(iop, _ONOCR) && iop->io_ocol == 0)
			return;
		if (tstflag(iop, _OCRNL) && !(iop->io_state & SOLF)) {
			outchar(iop, '\n');
			return;
		}
	}
	iop->io_ocol = 0;
	break;
  case '\b':
	iop->io_ocol--;
	break;
  case '\t':
	if (tstflag(iop, _OPOST) && tstflag(iop, _XTAB)) {
		/* expand tab to spaces */
		do
			outchar(iop, ' ');
		while ((int) iop->io_ocol % (int) iop->io_tabw);
		return;
	}
	iop->io_ocol =
	     ((int)iop->io_ocol/(int)iop->io_tabw+1)*iop->io_tabw;
	break;
  default:
	if (tstflag(iop, _OPOST)) {
#if MAPCASES
		/* generate escapes for upper case only terminals */
		if (tstflag(iop, _OLCUC)) {
			register char *s = "({)}!|^~'`";
			while (*s++)
				if (c == *s++) {
					outchar(iop, '\\');
					c = s[-2];
					break;
				}
			if (isupper(c))
				outchar(iop, '\\');
			if (islower(c))
				c = toupper(c);
		}
#endif /* MAPCASES */
		if (tstflag(iop, _OTILDE) && c == '~')	/* for Hazeltines */
			c = '`';
	}
	if (isprint(c))
		iop->io_ocol++;
	break;
  }
  /* output the character only when we are not counting for tab erasure */
  if ((iop->io_state & SCNTTB) == 0)
#endif /* !VERY_SIMPLE */
	putc(&iop->io_oqueue, c);
}

tty_write(threadno, iop, hdr, buf, size)
register struct io *iop;
register header *hdr;
register char *buf;
int size;
{
  uint16 n;

  compare(threadno, >=, 0);
  compare(threadno, <, (int) nttythread);
  n = hdr->h_size;
  if (size < (int) n)
  	hdr->h_size = n = size;

  while (n != 0 && ttythread[threadno] == 0
#if !VERY_SIMPLE
#if NO_IOCTL
	 && (iop->io_state & SFLUSHO) == 0
#else
	 && !tstflag(iop, _OFLUSH)
#endif /* NO_IOCTL */
#endif /* !VERY_SIMPLE */
	) {
#if !VERY_SIMPLE
	if (iop->io_iqueue.q_nchar != 0)
		iop->io_state |= SDIRTY;
#endif /* !VERY_SIMPLE */
	outchar(iop, *buf++);
	n--;
	if (iop->io_oqueue.q_nchar >= HIWATER(iop)) {
		ttstart(iop);
		while (ttythread[threadno] == 0 &&
				       iop->io_oqueue.q_nchar > LOWATER(iop)) {
		       	iop->io_state |= SOSLEEP;
			(void) await((event) &iop->io_oqueue, 0L);
		}
#if !VERY_SIMPLE
#if NO_IOCTL
		if (iop->io_state & SFLUSHO)
#else
		if (tstflag(iop, _OFLUSH))
#endif /* NO_IOCTL */
			n = 0;
#endif /* !VERY_SIMPLE */
	}
  }
#if !VERY_SIMPLE
#if NO_IOCTL
  if (iop->io_state & SFLUSHO)
#else
  if (tstflag(iop, _OFLUSH))
#endif /* NO_IOCTL */
	n = 0;
#endif /* !VERY_SIMPLE */
  ttstart(iop);
  hdr->h_size -= n;
  hdr->h_status = STD_OK;
  return 0;
}
