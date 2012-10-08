/*	@(#)fserver.c	1.6	96/02/27 13:07:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** I/O server for Amoeba programs run under supervision of AX.
** This server performs reads, writes and ioctls on stdin, stdout and stderr,
** and acts as a process owner to receive checkpoints when the process dies.
*/

/* Unix headers */

#ifndef __SVR4
/* Avoid 4.1 typeclash of <sys/stdtypes.h> and "posix/termios.h" by faking
 * that we already have included <sys/stdtypes.h>.
 * The only types we *do* need, are just copied from <sys/stdtypes.h>
 */
#define	sigset	signal	/* The posix stuff isn't in sunos4.1.3 */
#define __sys_stdtypes_h
typedef int		size_t;		/* ??? */
typedef int		sigset_t;	/* signal mask - may change */
#endif /* __SVR4 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef SYSV
#include <unistd.h>
#define	tc_marshal	tc_ux_marshal
#define	tc_unmarshal	tc_ux_unmarshal
#define	tc_getattr	tcgetattr
#endif
#include <setjmp.h>
#include <errno.h>

/* Amoeba headers */
#include "ailamoeba.h" /* Implies amoeba.h; for _ailword etc. */
#include "host_os.h" /* For SIGAMOEBA */
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "file.h"
#include "tty/tty.h"
#include "ajax/mymonitor.h"
#include "class/tios.h"
#include "module/prv.h"
#include "module/proc.h"

#include "ax.h"

#ifdef DEBUG
int debugging = 1;
#endif /* DEBUG */

/* Unix externals */
extern int errno;
extern char *getenv();

/* Ail stuff */
/* Macro to check for same-endian-ness */
#define SAME_ENDIAN(w) ( ((w) & _AIL_ENDIAN) == (_ailword & _AIL_ENDIAN) )
/* Marshalers for struct termios */
extern char *tc_marshal();
extern char *tc_unmarshal();

/* Termios-BSD conversion */
extern int tc_getattr();
extern int tc_setattr();
extern void tc_tobsd();
extern void tc_frombsd();

/* From main program */
extern char *progname; /* For error messages */
extern capability process_cap; /* For signal_it */

#define REQBUFSIZE 30000 /* Request buffer size */

/* Command codes used to send requests from one server to the other */
#define CMD_GETLOST 947 /* Command to tell other server to go away */
#define CMD_INTCAP  948 /* Command to pass TTQ_INTCAP to other server */

/* Global variables */
#ifdef __SVR4
int signal_occured = 0;
int saved_signal = 0;
#endif
static int tty_status_saved;
static int other = -1;
#ifdef STORE_CAP
static int child;
#endif
static int no_fork;
static jmp_buf jbuf;
static int jbuf_valid;
static capability intcap; /* Interrupt server capability */
static capability *cap;

/* Forward declarations */
static void quithand(), inthand(), ignhand();
static void transig();
static void signal_it();
static void done();
#ifdef __SVR4
static void dummy_handler();
#endif


/*
** Since we are now in the business of doing ioctls, we must prepare
** to restore the original tty status if the application makes a mess.
*/

static int fd_used;
static struct termios saved_state;

static void
save_tty_status(fd)
    int fd;
{
    if (tc_getattr(fd, &saved_state) >= 0)
    {
	fd_used = fd;
	tty_status_saved = 1;
    }
}


static void
restore_tty_status()
{
    if (!tty_status_saved)
	return;
    (void) tc_setattr(fd_used, TCSAFLUSH, &saved_state);
}


/*
** Write an error message to stderr, prefixed with the program name.
** For some reason we don't want to use stdio.
*/
static void
wr(str)
	char *str;
{
    char buf[256];

    (void) sprintf(buf, "%s: %s\n", progname, str);
    (void) write(2, buf, strlen(buf));
}


/*
** The file server main function.
** Never returns; calls done() to exit.
** XXX Some of the more interesting parameters are passed from
** the main program in global variables.  Sigh.
*/
void
fserver(no_fork_arg, privport, ax_tty, num)
    int no_fork_arg;		/* If set, don't fork a second server */
    port *privport;		/* Port to get requests from */
    struct tty_caps *ax_tty;	/* Capability corresponding to that port */
    int num;			/* # entries in ax_tty */
{
    header hdr;
    char buf[REQBUFSIZE];
    int n;
    bufsize requestsize, replysize;
    int go_away = 0;
    int lastcmd = 0;
    struct termios tio;
    int sts = 1;
    objnum obj;
    rights_bits rights;
    
    /* Copy arguments to global variables */
    no_fork = no_fork_arg;
    
    cap = &ax_tty[0].t_cap;

    /*
    ** Fork off a second instantiation, since some programs need
    ** concurrent input and output.  But then, some don't...
    ** If the fork fails, we don't give up but continue crippled.
    */
    if (!no_fork) {
        if ((other=fork()) < 0) {
	    fprintf(stderr,
		"%s: can't fork second fileserver, continuing with one\n",
		progname);
	    no_fork = 1;
	}
    }
    if (other == 0) {
	/*
	** Child.
	** Ignore signals.  The parent is handling them, and only one
	** of the two processes should attempt to kill the Amoeba
	** process when a Unix signal arrives.
	** (Actually, signals in the child are handled by a dummy handler,
	** so the signals are treated as similar as possible
	** in parent and child.)
	*/
#ifdef STORE_CAP
	child = 1;
#endif
	other = getppid();
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGQUIT, ignhand);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGINT, ignhand);
    }
    else {
	/*
	** Parent.
	** Setup signal handlers.
	*/
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGQUIT, quithand);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) sigset(SIGINT, inthand);
    }

    /*
    ** Both parent and child handle the transaction signal,
    ** which may make us jump out of a read signal call.
    */
    signal(SIGAMOEBA, transig);
    
    /*
    ** Server main loop.
    */
    do {
#ifdef __SVR4
	/* Solaris 2.x streams FLIP driver cannot pro_stun() during getreq() */
	/* See also comment in dummy_handler() */
	if (signal_occured) {
	    signal_it(saved_signal);
	    saved_signal = 0;
	    signal_occured = 0;
	}
#endif
	hdr.h_port = *privport;
	requestsize = getreq(&hdr, buf, REQBUFSIZE);
	if (ERR_STATUS(requestsize)) {
		errstat err = ERR_CONVERT(requestsize);
		if (err == RPC_ABORTED) {
			/*
			** This happens normally when a signal handler is
			** called while we are blocked in the getreq().
			** Just go on, ask for a request again.
			*/
			MON_EVENT("getreq interrupted");
			continue;
		}
		/*
		** Unexpected getreq() error
		** (wrong buffer address? null port?).
		*/
		fprintf(stderr, "%s: getreq error (%s)\n", progname,
							err_why(err));
		break;
	}
	
	replysize = 0;

	/* Check that the cap is ok */
	obj = prv_number(&hdr.h_priv);
	if (obj >= num ||
		prv_decode(&hdr.h_priv, &rights,
			    &ax_tty[obj].t_cap.cap_priv.prv_random) < 0)
	{
	    hdr.h_status = STD_CAPBAD;
	    putrep(&hdr, NILBUF, replysize);
	    continue;
	}

	/*
	** Command decoding switch.
	*/
	switch (lastcmd = hdr.h_command) {
	
	case FSQ_READ:
		MON_EVENT("FSQ_READ");
		if ((rights & RGT_READ) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		if (hdr.h_size > REQBUFSIZE)
			hdr.h_size = REQBUFSIZE;
		if (setjmp(jbuf) != 0) {
		    /* Get here after longjmp from transig */
		    MON_EVENT("longjumped -- return STD_INTR");
		    jbuf_valid = 0;
		    hdr.h_status = STD_INTR;
		    break;
		}
		jbuf_valid++;
		n = read(ax_tty[obj].t_fd, buf, (int)hdr.h_size);
		jbuf_valid = 0;
		if (n < 0) {
		    MON_NUMERROR("read error -- return STD_SYSERR", errno);
		    hdr.h_status = STD_SYSERR;
		    hdr.h_offset = errno;
		}
		else {
		    hdr.h_status = STD_OK;
		    hdr.h_extra = ((bufsize) n < hdr.h_size) ?
					FSE_NOMOREDATA : FSE_MOREDATA;
		    hdr.h_size = replysize = n;
		}
		break;
	
	case FSQ_WRITE:
		MON_EVENT("FSQ_WRITE");
		if ((rights & RGT_WRITE) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		n = write(ax_tty[obj].t_fd, buf, (int)requestsize);
		if (n < 0) {
		   MON_NUMERROR("write error -- return STD_SYSERR", errno);
		    hdr.h_status = STD_SYSERR;
		    hdr.h_offset = errno;
		}
		else {
		    hdr.h_status = STD_OK;
		    hdr.h_size = n;
		}
		break;

	case TIOS_GETATTR:
		MON_EVENT("TIOS_GETATTR");
		if ((rights & RGT_READ) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		if (tc_getattr(ax_tty[obj].t_fd, &tio) >= 0) {
		    /* NB passing tio by value to tc_marshal! */
		    replysize =
			tc_marshal(buf, tio, SAME_ENDIAN(hdr.h_extra)) - buf;
		    hdr.h_status = STD_OK;
		} else
		    hdr.h_status = STD_SYSERR;
		break;

	case TIOS_GETWSIZE:
		MON_EVENT("TIOS_GETWSIZE");
		if ((rights & RGT_READ) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
#ifdef TIOCGWINSZ
		{
			struct winsize size;
			if (ioctl(ax_tty[obj].t_fd, TIOCGWINSZ, &size) >= 0) {
				hdr.h_extra = size.ws_col;
				hdr.h_size = size.ws_row;
				hdr.h_status = STD_OK;
			} else
				hdr.h_status = STD_SYSERR;
		}
#else
		hdr.h_status = STD_COMBAD;
#endif /* TIOCGWINSZ */
		n = 0;
		break;

	case TIOS_SETATTR:
		MON_EVENT("TIOS_SETATTR");
		if ((rights & RGT_CONTROL) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		if (!tty_status_saved)
		    save_tty_status(ax_tty[obj].t_fd);
		(void) tc_unmarshal(buf, &tio, SAME_ENDIAN(hdr.h_size));
		if (tc_setattr(ax_tty[obj].t_fd, (int)hdr.h_extra, &tio) < 0)
		    hdr.h_status = STD_SYSERR;
		else
		    hdr.h_status = STD_OK;
		break;

	case TIOS_SENDBREAK:
	case TIOS_DRAIN:
	case TIOS_FLUSH:
	case TIOS_FLOW:
		MON_EVENT("TIOS_* -- return STD_COMBAD");
		hdr.h_status = STD_COMBAD;
		break;
	
	case CMD_INTCAP: /* Sent by other server */
		MON_EVENT("CMD_INTCAP");
	case TTQ_INTCAP:
		MON_EVENT("TTQ_INTCAP");
		if ((rights & RGT_CONTROL) == 0)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		if (requestsize != CAPSIZE)
			hdr.h_status = STD_ARGBAD;
		else {
			if (!no_fork && hdr.h_command == TTQ_INTCAP) {
				/* Try to tell other server about it */
				header h2;
				h2.h_port = ax_tty[0].t_cap.cap_port;
				h2.h_priv = ax_tty[0].t_cap.cap_priv;
				h2.h_command = CMD_INTCAP;
				(void) timeout((interval) 5000);
				(void) trans(&h2, buf, requestsize,
					&h2, NILBUF, (bufsize)0);
			}
#if defined(SYSV) || defined(__STDC__)
			memcpy((char *)&intcap, buf, CAPSIZE);
#else
			bcopy(buf, (char *)&intcap, CAPSIZE);
#endif /* SYSV */
			hdr.h_status = STD_OK;
		}
		break;
	
	case STD_INFO:
		MON_EVENT("STD_INFO");
		strcpy(buf, "+");
		strcat(buf, progname);
		strcat(buf, " server");
		if (getenv("USER") != NULL)
			strcat(strcat(buf, " for "), getenv("USER"));
		hdr.h_status = STD_OK;
		hdr.h_size = replysize = strlen(buf) + 1;
		break;
	
	case STD_RESTRICT:
		MON_EVENT("STD_RESTRICT");
		if (prv_encode(&hdr.h_priv, obj, rights & hdr.h_offset,
				&ax_tty[obj].t_cap.cap_priv.prv_random) == 0)
		    hdr.h_status = STD_OK;
		else
		    hdr.h_status = STD_CAPBAD;
		hdr.h_size = 0;
		break;
	
	case PS_CHECKPOINT:
		MON_EVENT("PS_CHECKPOINT");
		if (rights != 0xFF)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		if (hdr.h_extra == TERM_NORMAL) {
			if (hdr.h_offset != 0)
				fprintf(stderr, "%s: exit status %ld\n",
					progname, hdr.h_offset);
			sts = hdr.h_offset;
		}
		else if (hdr.h_extra == TERM_STUNNED && hdr.h_offset == SIGINT){
			fprintf(stderr, "%s: interrupted\n", progname);
		}
		else {
			(void) handle((int)hdr.h_extra, hdr.h_offset,
							buf, (int)requestsize);
		}
		hdr.h_status = STD_OK;
		/* NB Reply size is zero, so the process will die */
		go_away++;
		break;
	
	case PS_SWAPPROC:
		MON_EVENT("PS_SWAPPROC");
		if (rights != 0xFF)
		{
		    hdr.h_status = STD_DENIED;
		    break;
		}
		/* Validate requestsize and first argument (oldproc) */
		if (requestsize != 2 * sizeof(capability) ||
				prv_number(&((capability *)buf)->cap_priv) !=
				prv_number(&process_cap.cap_priv) ||
				!PORTCMP(&((capability *) buf)->cap_port,
						&process_cap.cap_port)) {
			MON_ERROR("old process capability mismatch");
			hdr.h_status = STD_ARGBAD;
		}
		else {
			errstat err;
			static capability nullcap;
			/* We don't want to see the oldproc's checkpoint */
			err = pro_setowner(&process_cap, &nullcap);
			if (err != STD_OK)
				fprintf(stderr, "%s: pro_setowner: %s\n",
							progname, err_why(err));
			process_cap = ((capability *) buf) [1];
			hdr.h_status = STD_OK;
			if (NULLPORT(&process_cap.cap_port)) {
				MON_EVENT("process wants to become a daemon");
				go_away++;
			}
		}
		break;
	
	case CMD_GETLOST:
		MON_EVENT("CMD_GETLOST");
		if (rights != 0xFF)
		    hdr.h_status = STD_DENIED;
		else
		{
		    sts = hdr.h_offset;
		    hdr.h_status = STD_OK;
		    go_away++;
		}
		break;
	
	default:
		MON_NUMERROR("unrecognized command", hdr.h_command);
		hdr.h_status = STD_COMBAD;
		break;
	
	}
	putrep(&hdr, buf, replysize);
    } while (!go_away);
    done(lastcmd, sts);
}

/*
** Finish off.
** Never returns -- ends by calling exit(sts).
*/

static void
done(lastcmd, sts)
	int lastcmd;
	int sts;
{
	MON_NUMEVENT("done", lastcmd);
	if (!no_fork && lastcmd != CMD_GETLOST) {
		header hdr;
		int n;
		MON_EVENT("tell other to hit it as well");
		hdr.h_port = cap->cap_port;
		hdr.h_priv = cap->cap_priv;
		hdr.h_command = CMD_GETLOST;
		hdr.h_offset = sts;
		(void) timeout((interval) 5000);
		n = trans(&hdr, 0, 0, &hdr, 0, 0);
		if (n != 0)
			MON_NUMEVENT("trans error", n);
	}
#ifdef STORE_CAP
	if (!child) {
		extern void removeproccap();

		removeproccap();
	}
#endif
	restore_tty_status();
	exit(sts);
}


/*
** Interrupt handlers -- pass the signal to the process.
** Must be of type void()(int) for ANSI C signal handling.
** Reinstate the signal because:
**	a) this allows recursive signals
**	b) System V signal handling resets signals to default (?)
*/

/*ARGSUSED*/
static void
quithand(sig)
int sig;
{
#ifdef __SVR4
	dummy_handler(SIGQUIT);
#else
	signal_it(SIGQUIT);
#endif
}

/*ARGSUSED*/
static void
inthand(sig)
int sig;
{
#ifdef __SVR4
	dummy_handler(SIGINT);
#else
	signal_it(SIGINT);
#endif
}

#ifdef __SVR4
/*ARGSUSED*/
static void
dummy_handler(sig)
{
	/* When a getmsg(2) of a getreq() is interrupted by a signal, the
	** getmsg(2) returns without copying the kernel data into its
	** buffer. First the signal handler is called. We cannot do a
	** trans() (called by pro_stun) in this signal handler, because
	** it would receive the data meant for the getreq(). So in the
	** signal handler we only set a flag "signal_occured" and save
	** the signal in "saved_signal". Then the signal handler returns
	** and the getmsg(2) returns with an error "interrupted system
	** call". At the beginning of the fserver loop we check for
	** occured signals and call ** signal_it() with the saved signal.
	**
	** -- rvdp 01/06/95
	*/
	signal_occured = 1;
	saved_signal = sig;
}
#endif


/*ARGSUSED*/
static void
ignhand(sig)
int sig;
{
}


/*
** Handler for transaction signal (SIGAMOEBA under Unix).
** This may be received while we are serving, to indicate that the
** client wants to interrupt the transaction.
** If we are blocked in a read system call, we jump out of it;
** otherwise, we ignore the hint (which is quite legal).
*/
/*ARGSUSED*/
static void
transig(sig)
int sig;
{
#ifdef __SVR4
	signal(SIGAMOEBA, transig);
#endif
	if (jbuf_valid)
		longjmp(jbuf, 1);
}


/*
** Signal the Amoeba process; called from signal handlers.
*/
static void
signal_it(sig)
int sig;
{
	errstat err;
	
	(void) timeout((interval) 3000);
	if (!NULLPORT(&intcap.cap_port)) { /* Try TTI_INTERRUPT */
		header h;
		char c;
		h.h_port = intcap.cap_port;
		h.h_priv = intcap.cap_priv;
		h.h_command = TTI_INTERRUPT;
		/* Set the interrupt char.
		   This should use the actual char instead of a constant,
		   but I'm to lazy to code that right now; also, the
		   session server actually prefers a constant... */
		if (sig == SIGQUIT)
			c = '\\' & 0x1f; /* Control-\ */
		else
			c = 'C' & 0x1f; /* Control-C */
		err = ERR_CONVERT(trans(&h, &c, 1, &h, NILBUF, 0));
		if (err == 0)
			err = ERR_CONVERT(h.h_status);
		if (err == STD_OK)
			return;
		wr("TTI_INTERRUPT failed:");
		wr(err_why(err));
	}
	err = pro_stun(&process_cap, (long) sig);
	switch (err) {
	case STD_OK:
		wr("pro_stun: process signaled");
		break;
	case RPC_FAILURE:
		wr("pro_stun: RPC failure (try again?)");
		break;
	case RPC_NOTFOUND:
		wr("pro_stun: server not found -- bye, bye");
		done(0, 1);
		break;
	case STD_CAPBAD:
		wr("pro_stun: bad capability -- bye, bye");
		done(0, 1);
		break;
	case STD_NOTNOW:
		wr("pro_stun: process already stopped");
		if (sig == SIGQUIT) {
			wr("QUIT signal -- bye, bye");
			done(0, 1);
		}
		break;
	default:
		wr("pro_stun failed:");
		wr(err_why(err));
		break;
	}
	return;
}
