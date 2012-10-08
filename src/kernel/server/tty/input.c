/*	@(#)input.c	1.5	96/02/27 14:21:19 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "file.h" /* For FSE_NOMOREDATA */
#include "tty/term.h"
#include "assert.h"
INIT_ASSERT
#ifndef SMALL_KERNEL
#include "table.h"
#endif
#include "cmdreg.h"
#include "stderr.h"
#include "sys/proto.h"

extern void putc();

#ifndef SMALL_KERNEL
static uint16 escape;
#endif

#if !VERY_SIMPLE
#if MAPCASES
/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Mostly used for
 * upper-case only terminals.
 * In AD1990 it really should be possible to scrap this.
 */

char	maptab[] ={
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};
#endif /* MAPCASES */

/*
 * This routine does all processing of typed characters that have to be
 * displayed on the terminal.
 */
static void echochar(iop, c)
register struct io *iop;
{
  if (!tstflag(iop, _ECHO))
	return;
  if (tstflag(iop, _LNEXTCH) && c == iop->io_lnext) {
	outchar(iop, '^');
	outchar(iop, '\b');
	return;
  }
  if (isprint(c) || c == '\n' || c == '\t' || !tstflag(iop, _CTLECHO))
	outchar(iop, c);
  else {
	outchar(iop, '^');
#if MAPCASES
	if (tstflag(iop, _OLCUC) && (unsigned) ((c&0xFF) - 1) <= 25)
		outchar(iop, c ^ 0x60);
	else
#endif /* MAPCASES */
		outchar(iop, c ^ 0x40);
  }
  if (tstflag(iop, _ICANON) && !(c & ~0xFF) && tstbit(iop->io_eof, c)) {
	if (!isprint(c))
		if (tstflag(iop, _CTLECHO))
			outchar(iop, '\b');
		else
			return;
	outchar(iop, '\b');
  }
}

/*
 * Erase a tab.
 */
static void erasetab(iop)
register struct io *iop;
{
  register savecol, c;

  iop->io_state |= SCNTTB;
  savecol = iop->io_ocol;
  iop->io_ocol = iop->io_icol;
  while ((c = nextc(&iop->io_iqueue)) != -1) {
	echochar(iop, c);
	if (!(c & ~0xFF) && (tstbit(iop->io_eof, c) || tstbit(iop->io_brk, c)))
		iop->io_ocol = iop->io_icol;
  }
  iop->io_state &= ~SCNTTB;
  savecol -= iop->io_ocol;
  iop->io_ocol += savecol;
  do
	outchar(iop, '\b');
  while (--savecol);
}

/*
 * Erase a single character.
 */
static void erasechar(iop, echoerase)
register struct io *iop;
{
  register c;

  if ((c = lastc(&iop->io_iqueue)) == -1)
	return;
  if (!(c & ~0xFF) && (tstbit(iop->io_brk, c) || tstbit(iop->io_eof, c)))
	return;
  (void) unputc(&iop->io_iqueue);
  if (!tstflag(iop, _ECHO))
	return;
  if (iop->io_state & SDIRTY)
	return;
  if (tstflag(iop, _PRTERA)) {
	if (!(iop->io_state & SERASE))
		outchar(iop, '\\');
	iop->io_state |= SERASE;
	if (tstflag(iop, _CTLECHO) && !isprint(c) && c != '\t') {
		outchar(iop, '^');
		outchar(iop, c & 0x7F ^ 0x40);
	} else
		outchar(iop, c);
  } else if (tstflag(iop, _CRTERA) || tstflag(iop, _CRTBS)) {
	if (c == '\t') {
		erasetab(iop);
		return;
	}
	if (!isprint(c) && tstflag(iop, _CTLECHO)
#if MAPCASES
	    || isupper(c) && tstflag(iop, _IUCLC)
#endif /* MAPCASES */
	   ) {
		outchar(iop, '\b');
		if (tstflag(iop, _CRTERA)) {
			outchar(iop, ' ');
			outchar(iop, '\b');
		}
	} else if (!isprint(c))
		return;
	outchar(iop, '\b');
	if (tstflag(iop, _CRTERA) && (c & 0xFF) != ' ') {
		outchar(iop, ' ');
		outchar(iop, '\b');
	}
  } else if (echoerase)
	outchar(iop, iop->io_erase);
}

static void eraseword(iop)
register struct io *iop;
{
  register c, nblank = 0;

  c = lastc(&iop->io_iqueue);
  if (c == ' ' || c == '\t') {
	if (tstflag(iop, _PRTERA)) {
		do erasechar(iop, 1);
		while ((c = lastc(&iop->io_iqueue)) == ' ' || c == '\t');
	} else {
		do {
			nblank++;
			(void) unputc(&iop->io_iqueue);
		} while ((c = lastc(&iop->io_iqueue)) == ' ' || c == '\t');
		if (tstflag(iop, _CRTERA) || tstflag(iop, _CRTBS))
			erasetab(iop);
		else
			do echochar(iop, iop->io_erase); while (--nblank);
	}
  }
  while (c != -1) {
	if (c == ' ' || c == '\t')
		break;
	if (!(c & ~0xFF) && (tstbit(iop->io_brk,c) || tstbit(iop->io_eof,c)))
		break;
	erasechar(iop, 1);
	c = lastc(&iop->io_iqueue);
  }
}

static void linekill(iop)
register struct io *iop;
{
  register c;

  if (!tstflag(iop, _PRTERA) && tstflag(iop, _CRTKIL) &&
      !(iop->io_state & SDIRTY) /* && iop->io_ocol - iop->io_icol < 128 */) {
	c = lastc(&iop->io_iqueue);
	while (c != -1 &&
	  (c & ~0xFF || !tstbit(iop->io_brk, c) && !tstbit(iop->io_eof, c))) {
		if (c == '\t') {
			do
				(void) unputc(&iop->io_iqueue);
			while ((c=lastc(&iop->io_iqueue)) == ' ' || c == '\t');
			erasetab(iop);
			continue;
		}
		erasechar(iop, 1);
		c = lastc(&iop->io_iqueue);
	}
  } else {
	if (tstflag(iop, _ECHO)) {
		echochar(iop, iop->io_kill);
		outchar(iop, '\n');
	}
	while ((c = lastc(&iop->io_iqueue)) != -1 &&
	  (c & ~0xFF || !tstbit(iop->io_brk, c) && !tstbit(iop->io_eof, c)))
		(void) unputc(&iop->io_iqueue);
  }
  iop->io_state &= ~SDIRTY;
}

static void reprint(iop)
register struct io *iop;
{
  register c;

  echochar(iop, iop->io_rprnt);
  echochar(iop, '\n');
  iop->io_icol = iop->io_ocol;
  while ((c = nextc(&iop->io_iqueue)) != -1)
	echochar(iop, c);
  iop->io_state &= ~SDIRTY;
}
#endif /* !VERY_SIMPLE */

/*
 * Low level input thread (called at interrupt level).
 */
void
readint(arg)
long arg;
{
  register struct io *iop;
  register c;

  compare((uint16) (arg>>16), <, ntty);
  iop = &tty[arg>>16];
  c = arg & 0xFF;
#if !VERY_SIMPLE
  /* Ignoring a character means really ignore it! */
  if (tstbit(iop->io_ign, c))
	return;
  /* Strip character to 7 bits if necessary. */
  if (tstflag(iop, _ISTRIP))
	c &= 0x7F;
  /* Do literal nexting very early. */
  if (iop->io_state & SLNCH)
	c |= 0x100;
  /* Now do the mappings. */
  if (c == '\r' && tstflag(iop, _ICRNL))
	c = '\n';
  else if (c == '\n' && tstflag(iop, _INLCR))
	c = '\r';
#if MAPCASES
  if (tstflag(iop, _IUCLC) && c <= 0x7F) {
	if (iop->io_state & SBKSL) {
		if (maptab[c])
			c = maptab[c];
		iop->io_state &= ~SBKSL;
	} else if (isupper(c))
		c = tolower(c);
	else if (c == '\\') {
		iop->io_state |= SBKSL;
		return;
	}
  }
#endif /* MAPCASES */
#endif /* !VERY_SIMPLE */
#ifndef SMALL_KERNEL
  if (escape && iop == &tty[0]) {
	extern struct dumptab dumptab[];
	register struct dumptab *d;

	escape = 0;
	for (d = dumptab; d->d_func; d++)
		if (c == d->d_char) {
			(*d->d_func)((char *) 0, (char *) 0);
			return;
		}
	if (c != ESCAPE) {
		printf("\n");
		for (d = dumptab; d->d_func; d++)
			printf("%c: %s\n", d->d_char, d->d_help);
		return;
	}
  } else if (iop == &tty[0] && c == ESCAPE) {
	escape = 1;
	return;
  }
#endif /* SMALL_KERNEL */
#if !VERY_SIMPLE || XONXOFF
#if !VERY_SIMPLE
  if (tstflag(iop, _IXON))
#endif /* !VERY_SIMPLE */
  {

#if VERY_SIMPLE
#define STOPCHAR	CNTRL('S')
#define STARTCHAR	CNTRL('Q')
#else
#define STOPCHAR	iop->io_ostop
#define STARTCHAR	iop->io_ostart
#endif /* VERY_SIMPLE */

	if (iop->io_state & SOSUSP) {
		/* output is suspended */
		if (c == STARTCHAR) {
			/* start char typed while being suspended */
			iop->io_state &= ~SOSUSP;
			ttstart(iop);			/* start output */
			return;
		}
		if (c == STOPCHAR) {
			/* stop char typed while being suspended; ignore */
			return;
		}
#if !VERY_SIMPLE
		/* other char typed; resume only when _IXANY */
		if (tstflag(iop, _IXANY))
#endif /* !VERY_SIMPLE */
		{
			iop->io_state &= ~SOSUSP;
			ttstart(iop);			/* start output */
		}
	} else {
		/* output not suspended */
		if (c == STOPCHAR) {
			/* suspend output */
			iop->io_state |= SOSUSP;
			return;
		}
		if (c == STARTCHAR) {
			/* ignore superfluous start characters */
			return;
		}
	}
  }
#endif /* !VERY_SIMPLE || XONXOFF */
#if !VERY_SIMPLE
  if (tstflag(iop, _FLUSHCH) && c == iop->io_flush) {
#if NO_IOCTL
	if (iop->io_state & SFLUSHO)
		iop->io_state &= ~SFLUSHO;
#else
	if (tstflag(iop, _OFLUSH))
		clrflag(iop, _OFLUSH);
#endif /* NO_IOCTL */
	else {
		flushqueue(&iop->io_oqueue);
		echochar(iop, iop->io_flush);
		if (iop->io_iqueue.q_nchar != 0)
			reprint(iop);
#if NO_IOCTL
		iop->io_state |= SFLUSHO;
#else
		setflag(iop, _OFLUSH);
#endif /* NO_IOCTL */
	}
	ttstart(iop);
	return;
  }
#if NO_IOCTL
  iop->io_state &= ~SFLUSHO;
#else
  clrflag(iop, _OFLUSH);
#endif /* NO_IOCTL */
  if (tstflag(iop, _ICANON)) {
	if (iop->io_state & SQUOT &&
	    (c == iop->io_erase || c == iop->io_kill)) {
		erasechar(iop, 0);
		if (iop->io_state & SDIRTY)
			reprint(iop);
	} else if  (c == iop->io_erase) {
		erasechar(iop, 1);
		if  (iop->io_state & SDIRTY)
			reprint(iop);
		ttstart(iop);
		return;
	} else if (tstflag(iop, _WERASECH) && c == iop->io_werase) {
		eraseword(iop);
		if (iop->io_state & SDIRTY)
			reprint(iop);
		iop->io_state &= ~SQUOT;
		ttstart(iop);
		return;
	} else if (c == iop->io_kill) {
		linekill(iop);
		ttstart(iop);
		return;
	}
  }
  if (iop->io_state & SERASE) {
	iop->io_state &= ~SERASE;
	outchar(iop, '/');
  }
  if (tstflag(iop, _RPRNTCH) && c == iop->io_rprnt) {
	reprint(iop);
	ttstart(iop);
	return;
  }
#endif /* !VERY_SIMPLE */

#if !VERY_SIMPLE || RTSCTS || XONXOFF
#if !VERY_SIMPLE
  if (tstflag(iop, _IXOFF)) {
#endif /* !VERY_SIMPLE */
	if (iop->io_iqueue.q_nchar >= HIWATER(iop))
		if (iop->io_state & SISUSP) {
			if ((int) iop->io_iqueue.q_nchar >=
			    (int) (2 * HIWATER(iop)))
				return;
		} else {
			isuspend(iop);
			iop->io_state |= SISUSP;
		}
#if !VERY_SIMPLE
  }
#endif /* !VERY_SIMPLE */
#endif /* !VERY_SIMPLE || RTSCTS || XONXOFF */
#if !VERY_SIMPLE
  if (tstflag(iop, _ISIG) && (c & 0x100) == 0 && tstbit(iop->io_intr, c)) {
	echochar(iop, c);
	iop->io_state |= SINTED;
	if (intsleeping) {
		intsleeping = 0;
		wakeup((event) &intsleeping);
	}
	setbit(iop->io_ints, c);
	if (!tstflag(iop, _NOFLUSH)) {
		flushqueue(&iop->io_iqueue);
		iop->io_nbrk = 0;
	}
	iop->io_state &= ~SOSUSP;
	ttstart(iop);
	return;
  }
#endif /* !VERY_SIMPLE */
#if !VERY_SIMPLE || !XONXOFF && !RTSCTS
  if (iop->io_iqueue.q_nchar >= NINQUEUE) {
	outchar(iop, CNTRL('G'));	/* queue is full */
	ttstart(iop);
	return;
  }
#endif /* !VERY_SIMPLE || !XONXOFF && !RTSCTS */

#if VERY_SIMPLE
  putc(&iop->io_iqueue, c);
#else
  if (tstflag(iop, _LNEXTCH) && c == iop->io_lnext)
	iop->io_state |= SLNCH;
  else {
	register lc = lastc(&iop->io_iqueue);
	if (lc == -1 || !(lc & ~0xFF) &&
	    (tstbit(iop->io_eof, lc) || tstbit(iop->io_brk, lc)))
		iop->io_icol = iop->io_ocol;
	putc(&iop->io_iqueue, c);
	iop->io_state &= ~SLNCH;
	if (!(c & ~0xFF) && (tstbit(iop->io_eof, c) || tstbit(iop->io_brk, c)))
		iop->io_nbrk++;
  }
  echochar(iop, c);
  if (c == '\\')
	iop->io_state |= SQUOT;
  else
	iop->io_state &= ~SQUOT;
#endif /* VERY_SIMPLE */
  if (iop->io_iqueue.q_nchar != 0
#if !VERY_SIMPLE
      && (iop->io_nbrk != 0 || !tstflag(iop, _ICANON))
#endif /* !VERY_SIMPLE */
     ) {
	wakeup((event) &iop->io_iqueue);
#if !VERY_SIMPLE
	if (tstflag(iop, _INTRUP)) {
		iop->io_state |= SINTRUP;
		if (intsleeping) {
			intsleeping = 0;
			wakeup((event) &intsleeping);
		}
	}
#endif /* !VERY_SIMPLE */
  }
  ttstart(iop);
}

tty_read(threadno, iop, hdr, buf, size)
int threadno;
register struct io *iop;
register header *hdr;
char *buf;
{
  register c;
  uint16 n;
  char *p;

  compare(threadno, >=, 0);
  compare(threadno, <, (int) nttythread);
  if (size > (int) hdr->h_size)
	size = hdr->h_size;
  while (ttythread[threadno] == 0 && (iop->io_iqueue.q_nchar == 0
#if !VERY_SIMPLE
	 || iop->io_nbrk == 0 && tstflag(iop, _ICANON)
#endif /* !VERY_SIMPLE */
	)) {
	if (hdr->h_command == TTQ_TIME_READ && hdr->h_extra == 0 || hdr->h_extra) {
		/* non-blocking read; nothing to return yet */
		hdr->h_status = STD_OK;
		hdr->h_size = 0;
		return 0;
	}
	/*
	 * Apart from the FSQ_READ versus TTQ_READ bogosity here
	 * I couldn't find any usage of the TTQ_TIME_READ in any
	 * library stub. Someone should figure this out someday.
	 *
	 * I took the liberty of changing the h_extra timeout field
	 * to be in ms iso ds. Since nobody uses it, it looks safe.
	 *	sater
	 */
	hdr->h_extra = await((event) &iop->io_iqueue, (interval) hdr->h_extra); /*AMOEBA4*/
  }
  if (ttythread[threadno]) {
	/* signalled */
	hdr->h_status = STD_INTR;
	hdr->h_size = 0;
	return 0;
  }
  p = buf;
  n = 0;
  while ((int) n < size) {
	c = getc(&iop->io_iqueue);
	if (c == -1)
		break;
#if !VERY_SIMPLE
	if (!(c & ~0xFF) && (tstbit(iop->io_eof, c) || tstbit(iop->io_brk, c)))
		iop->io_nbrk--;
#endif /* !VERY_SIMPLE */
#if !VERY_SIMPLE || XONXOFF || RTSCTS
	if (
#if !VERY_SIMPLE
	    (tstflag(iop, _IXOFF) || tstflag(iop, _IMODEM)) &&
#endif /* !VERY_SIMPLE */
	    iop->io_iqueue.q_nchar <= LOWATER(iop) &&
	    iop->io_state & SISUSP) {
		iresume(iop);
		iop->io_state &= ~SISUSP;
	}
#endif /* !VERY_SIMPLE || XONXOFF || RTSCTS */
#if !VERY_SIMPLE
	if ((c & ~0xFF) == 0 && tstbit(iop->io_eof, c) && tstflag(iop, _ICANON))
		break;
#endif /* !VERY_SIMPLE */
	*p++ = c;
	n++;
#if !VERY_SIMPLE
	if ((c & ~0xFF) == 0 && tstbit(iop->io_brk, c) && tstflag(iop, _ICANON))
		break;
#endif /* !VERY_SIMPLE */
  }
#if !VERY_SIMPLE
  compare(iop->io_nbrk, >=, 0);
  if (iop->io_iqueue.q_nchar == 0)
	iop->io_state &= ~SDIRTY;
#endif /* !VERY_SIMPLE */
  hdr->h_status = STD_OK;
  hdr->h_size = n;
  hdr->h_extra = FSE_NOMOREDATA; /* XXX not if the buffer was too small */
  return n;
}
