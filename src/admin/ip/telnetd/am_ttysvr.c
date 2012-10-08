/*	@(#)am_ttysvr.c	1.2	96/02/27 10:15:42 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef AMOEBA

/*
 * This sourcefile is largely based on the Amoeba tty server for xterm.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <amoeba.h>
#include <sys/types.h>
#include <ailamoeba.h>
#include <signal.h>
#include <exception.h>
#include <thread.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <semaphore.h>
#include <circbuf.h>
#include <limits.h>
#include <server/process/proc.h>
#include <file.h>
#include <fault.h>
#include <module/mutex.h>
#include <posix/termios.h>
#include <class/tios.h>
#include <server/tty/tty.h>
#include <module/signals.h>
#include <module/rnd.h>

#include "am_defs.h"

#ifdef DEBUG
#define dbprintf(list)	{ warning list ; }
#else
#define dbprintf(list)	{ /* nothing */ }
#endif

extern char *ProgramName;

#define False	0
#define True	1

/* manifest constants */
#define	TTY_NTHREADS		2
#define	TTY_INQSIZE		2000
#define	TTY_OUTQSIZE		1000
#define	TTY_THREAD_STACKSIZE	8192

static struct Term TermInfo;
struct Term *term = &TermInfo;

/*
 * AIL stuff
 */
#define SAME_ENDIAN(w)	(((w)&_AIL_ENDIAN) == (_ailword & _AIL_ENDIAN) )

extern char *tc_marshal();
extern char *tc_unmarshal();
extern void millisleep();

#define	CTRL(c)		((c) & 0x1f)

/*
 * Tty input data structure.
 * This is a separate buffer in which the edited line is built.
 */
static char tty_line[_POSIX_MAX_CANON+1];
static char *tty_ptr;
static int tty_left;

/*
 * A global flags which denotes whether we're busy with exiting. This
 * prevents two cleanups to be running (one from ttythread, and one
 * from the main stream code as a result of a failed _XAmSelect).
 */
int exiting = False;

/*
 * Mutexes to prevent concurrent reads/writes
 */
static mutex read_mutex;
static mutex write_mutex;

port ttyport;
capability ttycap;
capability ttyintcap;
struct termios tios;

static void ttythread();

static int def_rows = 24;
static int def_cols = 80;
static int tty_inited;

/*
 * Initialize the TTY structures.
 */
void
tty_init()
{
    int i;
    TScreen *screen = &term->screen;

    dbprintf(("tty_init.."));
    if (tty_inited) {
	return;
    }
    tty_inited = 1;

    mu_init(&read_mutex);
    mu_init(&write_mutex);

    uniqport(&ttyport);
    priv2pub(&ttyport, &ttycap.cap_port);

    tios.c_iflag = ICRNL | IXON;
    tios.c_oflag = OPOST;
    tios.c_cflag = CREAD | CS8 | B9600;
    tios.c_lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG;

    tios.c_cc[VEOF] = CTRL('D');
    tios.c_cc[VEOL] = _POSIX_VDISABLE;
    tios.c_cc[VERASE] = CTRL('H');
    tios.c_cc[VINTR] = CTRL('C');
    tios.c_cc[VKILL] = CTRL('U');
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VQUIT] = CTRL('\\');
    tios.c_cc[VSUSP] = _POSIX_VDISABLE;
    tios.c_cc[VTIME] = 0;
    tios.c_cc[VSTART] = CTRL('Q');
    tios.c_cc[VSTOP] = CTRL('S');

    screen->tty_inq = cb_alloc(TTY_INQSIZE);
    screen->tty_outq = cb_alloc(TTY_OUTQSIZE);
    screen->max_col = def_cols;
    screen->max_row = def_rows;

    /*
     * Start tty threads. Ordinarily two should suffice, one for standard
     * input and one for standard (error) output.
     */
    for (i = 0; i < TTY_NTHREADS; i++) {
	if (!thread_newthread(ttythread, TTY_THREAD_STACKSIZE, (char*) 0, 0)) {
	    FatalError("unable to start tty thread.");
	}
    }
}

void
WakeupMainThread()
{
    SomethingHappened();
}

void
Cleanup(val)
int val;
{
    exit(val);
}

/* ARGSUSED */
static void
ttycatcher(sig, us, extra)
    signum sig;
    thread_ustate *us;
    _VOIDSTAR extra;
{
    *((int *)extra) = True;
}

static void
ttythread()
{
    TScreen *screen = &term->screen;
    struct circbuf *oq;
    header hdr;
    char *buf;
    bufsize n;
    int cause = 0, detail = 0;
    int signalled;
    struct termios ttios;

    oq = screen->tty_outq;
    buf = (char *)malloc(BUFSIZ);
    if (buf == (char *)NULL) {
	FatalError("cannot malloc tty server buffer.");
    }

    sig_catch((signum) SIG_TRANS, ttycatcher, (_VOIDSTAR) &signalled);
    for (;;) {
	if (exiting) { /* login disappeared ?! */
	    if (cause == TERM_STUNNED)
		warning("telnet session terminated; stun code %d", detail);
	    else if (cause == TERM_EXCEPTION)
		warning("telnet session terminated; exception code %d", detail);
	    else if (cause != TERM_NORMAL)
		warning("telnet session terminated; cause %d, detail %d",
			cause, detail);
	    Cleanup(0);
	}

	signalled = False;
	do {
	    hdr.h_port = ttyport;
	    n = getreq(&hdr, (bufptr) buf, BUFSIZ);
	} while (ERR_CONVERT(n) == RPC_ABORTED);
	if (ERR_STATUS(n)) {
	    warning("getreq failed (%s)", err_why((errstat) ERR_CONVERT(n)));
	    Cleanup(1);
	}

	switch (hdr.h_command) {
	case FSQ_CREATE:
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case FSQ_READ:
	    dbprintf(("FSQ_READ"));
	    if (!(tios.c_cflag & CREAD)) {
		hdr.h_status = STD_NOTNOW;
		n = 0;
		break;
	    }
	    while (!signalled) {
		if (mu_trylock(&read_mutex, (interval) -1) < 0)
		    continue;
		hdr.h_size = ttycanonread(buf, (size_t)hdr.h_size, &signalled);
		hdr.h_extra = tty_left > 0 ? FSE_MOREDATA : FSE_NOMOREDATA;
		mu_unlock(&read_mutex);
		break;
	    }
	    n = hdr.h_size;
	    hdr.h_status = signalled ? STD_INTR : STD_OK;
	    break;
	case FSQ_WRITE:
	    dbprintf(("FSQ_WRITE"));
	    mu_lock(&write_mutex);
	    n = ttywrite(buf, (int) n);
	    mu_unlock(&write_mutex);
	    hdr.h_size = n;
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case TTQ_CLOSE:
	case TTQ_STATUS:
	case TTQ_CONTROL:
	case TTQ_TIME_READ:
	    hdr.h_status = STD_COMBAD;
	    n = 0;
	    break;
	case TIOS_SETATTR:
	    (void) tc_unmarshal(buf, &ttios, SAME_ENDIAN(hdr.h_size));
	    if (tios_check(&ttios)) {
		hdr.h_status = STD_OK;
		tios = ttios;
	    } else
		hdr.h_status = STD_ARGBAD;
	    n = 0;
	    break;
	case TIOS_GETATTR:
	    n = tc_marshal(buf, tios, SAME_ENDIAN(hdr.h_extra)) - buf;
	    hdr.h_status = STD_OK;
	    break;
	case TIOS_SENDBREAK:
	case TIOS_DRAIN:
	case TIOS_FLUSH:
	case TIOS_FLOW:
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case TIOS_GETWSIZE:
	    hdr.h_extra = screen->max_col;
	    hdr.h_size = screen->max_row;
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case TIOS_SETWSIZE:
	    if (hdr.h_extra > 0 && hdr.h_size > 0) {
		screen->max_col = hdr.h_extra;
		screen->max_row = hdr.h_size;
		hdr.h_status = STD_OK;
	    } else {
		hdr.h_status = STD_ARGBAD;
	    }
	    n = 0;
	    break;
	case STD_AGE:
	case STD_COPY:
	case STD_RESTRICT:
	    hdr.h_status = STD_COMBAD;
	    n = 0;
	    break;
	case STD_TOUCH:
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case STD_DESTROY:
	    hdr.h_status = STD_DENIED;
	    n = 0;
	    break;
	case STD_INFO:
	    hdr.h_status = STD_OK;
	    strcpy(buf, "Xterm tty server");
	    n = strlen(buf);
	    break;
	case PS_CHECKPOINT:
	    exiting = True;
	    cb_putc(oq, '\0');
	    WakeupMainThread();
	    cause = hdr.h_extra;
	    detail = hdr.h_offset;
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case PS_SWAPPROC:
	    hdr.h_status = STD_OK;
	    n = 0;
	    break;
	case TTQ_INTCAP:
	    if (n == sizeof(capability))  {
		hdr.h_status = STD_OK;
		bcopy(buf, (char *)&ttyintcap, sizeof(capability));
	    } else
		hdr.h_status = STD_ARGBAD;
	    n = 0;
	    break;
	default:
	    hdr.h_status = STD_COMBAD;
	    n = 0;
	}
	putrep(&hdr, (bufptr) buf, n);

	SomethingHappened();
    }
}

int
tios_check(tp)
    struct termios *tp;
{
    register int i, j;

    /*
     * Check that all chars in th c_cc array are
     * different, except VTIME and VMIN
     */
    for (i = 0; i < sizeof(tp->c_cc); ++i) {
	if (i != VTIME && i != VMIN) for (j = 0; j < i; ++j)
	    if (j != VTIME && j != VMIN &&
	      tp->c_cc[i] == tp->c_cc[j] && tp->c_cc[i] != _POSIX_VDISABLE)
		return False;
    }

    /* cannot change byte size */
    if ((tp->c_cflag & CSIZE) != CS8)
	return False;
    return True;
}

void
erasechar(oq, n, echo)
    struct circbuf *oq;
    int n, echo;
{
    register int rcol;

    if (!echo) return;
    if (n > 0) {
	rcol = n - 1;
	cb_putc(oq, '\b');
	if (tios.c_lflag & ECHOE)
	    cb_puts(oq, " \b", 2);
    } else
	cb_putc(oq, CTRL('G'));
    WakeupMainThread();
}

void
linekill(oq, n, echo)
    struct circbuf *oq;
    int n, echo;
{
    register int rcol;

    if (!echo) return;
    if (n > 0) {
	for (rcol = n-1; rcol >= 0; rcol--) {
	    cb_putc(oq, '\b');
	    if (tios.c_lflag & ECHOK)
		cb_puts(oq, " \b", 2);
	}
    } else
	cb_putc(oq, CTRL('G'));
    WakeupMainThread();
}

/*
 * Canonize
 */
#define ISSET(t, flag)	(((t).c_lflag & flag) != 0)

void
canonize(signalled)
    int *signalled;
{
    TScreen *screen = &term->screen;
    struct circbuf *iq, *oq;
    int ch, n;
    int stop;
    
    dbprintf(("canonize"));
    n = 0;
    oq = screen->tty_outq;
    iq = screen->tty_inq;

    for (stop = False; !stop; ) {
	/* get a character, depending on canonisation */
	if (!ISSET(tios, ICANON)) {
	    /*
	     * No canonization
	     */
	    int time = tios.c_cc[VTIME] * 100; /* inter-byte delay (in msec) */

	    ch = time ? cb_trygetc(iq, time) : cb_trygetc(iq, -1);
	    if (ch < 0 || *signalled)
		break;
	} else {
	    /*
	     * Canonized input
	     */
	    ch = cb_trygetc(iq, -1);
	    if (ch == -1)
		break;
	}

	/* ICANON may have changed while we waited for the first character */
	if (!ISSET(tios, ICANON)) {
	    int min = tios.c_cc[VMIN]; /* minimum characters to read */

	    if (ISSET(tios, ECHO)) {
		cb_putc(oq, ch);
		dbprintf(("non-canon: ECHO: put char '%c'", ch));
		WakeupMainThread();
	    }
	    tty_line[n++] = ch;
	    if (n >= sizeof(tty_line) || (min > 0 && n >= min))
		break;
	} else {
	    if (tios.c_iflag & ISTRIP) ch &= 0x7f;
	    if (ch == tios.c_cc[VEOF]) {
		stop = True;
	    } else if (ch == tios.c_cc[VERASE]) {
		/* erase a single character */
		erasechar(oq, n, ISSET(tios, ECHO));
		if (n != 0) n--;
	    } else if (ch == tios.c_cc[VKILL]) {
		/* erase a whole line */
		linekill(oq, n, ISSET(tios, ECHO));
		n = 0;
	    } else {
		if (n > sizeof(tty_line)) {
		    cb_putc(oq, CTRL('G'));
		    continue;
		}
		if (ch == '\r' || ch == '\n' || ch == tios.c_cc[VEOL]) {
		    if (tios.c_iflag & ICRNL)
			ch = '\n';
		    else if (tios.c_iflag & INLCR)
			ch = '\r';
		    if (ch == '\n' && tios.c_iflag & ICRNL)
			cb_putc(oq, '\r');
		    stop = True;
		}
		if (ISSET(tios, ECHO)) {
		    cb_putc(oq, ch);
		    dbprintf(("canon: ECHO: put char '%c'", ch));
		    WakeupMainThread();
		}
		tty_line[n++] = ch;
	    }
	}
    }
    /* if (ISSET(tios, ECHO)) */ WakeupMainThread();
    tty_left = n;
    tty_ptr = tty_line;
}

/*
 * Canonized tty read
 */
int
ttycanonread(buf, size, signalled)
    char *buf;
    size_t size;
    int *signalled;
{
    if (tty_left == 0)
	canonize(signalled);
    if (size > tty_left)
	size = tty_left;
    strncpy(buf, tty_ptr, size);
    tty_ptr += size;
    tty_left -= size;
    return size;
}

/*
 * Write data to tty
 */
int
ttywrite(buf, size)
    char *buf;
    int size;
{
    TScreen *screen = &term->screen;
    struct circbuf *oq;
    int ch, i;

    dbprintf(("ttywrite %d bytes", size));
    oq = screen->tty_outq;
    for (i = 0; i < size; i++) {
	if ((ch = buf[i]) == '\n' && tios.c_iflag & ICRNL)
	    cb_putc(oq, '\r');
	cb_putc(oq, ch);
	if (ch == '\n') {
	    /* Provide a wakeup for each line.  Otherwise the circular buffer
	     * could be filled (thus blocking us) without the main thread
	     * knowing it can already process some data.
	     */
	    WakeupMainThread();
	}
    }
    dbprintf(("ttywrite: last char '%c'", buf[size - 1]));
    WakeupMainThread();
    return size;
}

static void
flush_read()
{
    TScreen *screen = &term->screen;
    struct circbuf *iq;
    char *p;
    int n;

    dbprintf(("flush_read"));
    iq = screen->tty_inq;
    cb_putc(iq, '\n');
    mu_lock(&read_mutex);
    while ((n = cb_getp(iq, &p, 0/*don't block*/)) > 0)
	cb_getpdone(iq, n);
    tty_left = 0;
    mu_unlock(&read_mutex);
    WakeupMainThread();
}

/*
 * Before characters are pushed onto the input stream of the shell
 * process some of them may need some pre-processing. For example,
 * interrupt should be processed as soon as possible to obtain the
 * desired effect. This routine deals with key strokes that have
 * such special properties.
 */
int
ttypreprocess(ch)
    int ch;
{
    if ((tios.c_lflag & ISIG) && ch == tios.c_cc[VQUIT]) {
	if (!(tios.c_lflag & NOFLSH)) flush_read();
	ttysendsig(SIGQUIT);
	return 1;
    }
    if ((tios.c_lflag & ISIG) && ch == tios.c_cc[VINTR]) {
	if (!(tios.c_lflag & NOFLSH)) flush_read();
	ttysendsig(SIGINT);
	return 1;
    }
    return 0;
}

/*
 * Send Unix signal to shell
 */
ttysendsig(sig)
    int sig;
{
    header hdr;
    long tout;

    dbprintf(("ttysendsig(%d)", sig));
    if (!(tios.c_lflag & ISIG))
	return;
    if (NULLPORT(&ttyintcap.cap_port))
	return;
    hdr.h_port = ttyintcap.cap_port;
    hdr.h_priv = ttyintcap.cap_priv;
    hdr.h_extra = sig;
    hdr.h_command = TTI_SIGNAL;
    tout = timeout(2000L);
    trans(&hdr, NILBUF, 0, &hdr, NILBUF, 0);
    timeout(tout);
}

/*
 * Additional code not present in the xterm tty driver.
 */

int
tty_tcsetattr(newtios)
struct termios *newtios;
{
    if (tios_check(newtios)) {
	tios = *newtios;
	return 1;
    } else {
	warning("tty_tcsetattr: tios_check failed");
	return 0;
    }
}

int
tty_tcgetattr(newtios)
struct termios *newtios;
{
    *newtios = tios;
    return 1;
}

int
tty_data_available()
{
    int size;

    /* dbprintf(("tty data?")); */
    size = cb_full(term->screen.tty_outq);

    if (size > 0) {
	/* dbprintf(("tty: %d bytes available", size)); */
    }

    return (size > 0);
}

int
tty_write_to_prog(buf, size)
char *buf;
int size;
{
    buf[size] = '\0';
    dbprintf(("put prog input \"%s\" (%d bytes)", buf, size));
    if (cb_puts(term->screen.tty_inq, buf, size) != 0) {
	warning("tty_write_to_prog failed");
	return -1;
    } else {
	return size;
    }
}

int
tty_read_from_prog(buf, size)
char *buf;
int   size;
{
    int got;

    dbprintf(("get prog output"));
    got = cb_gets(term->screen.tty_outq, buf, 0, size);
    if (got > 0) {
	buf[got] = '\0';
	dbprintf(("%d bytes; data: \"%s\"", got, buf));
    } else {
	warning("tty_read_from_prog: no data");
    }
    return got;
}

void
tty_change_window_size(cols, rows)
int cols;
int rows;
{
    dbprintf(("tty_change_window_size(%d, %d)", cols, rows));
    if (tty_inited) {
	TScreen *screen = &term->screen;

	screen->max_col = cols;
	screen->max_row = rows;
    } else {
	def_cols = cols;
	def_rows = rows;
    }
}

int
tty_can_flush()
{
    return 1;
}

#endif /* AMOEBA */
