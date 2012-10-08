/*	@(#)interrupt.c	1.2	96/03/07 14:07:52 */
#include "util.h"
#include "mmdf.h"

/*  INTERRUPT TRAP SETTING AND CATCHING
 *
 *  Aug, 81 Dave Crocker    major addition to mmdftimeout, for conditionally
 *                          using timerest only when non-zero.
 */

#include <signal.h>

extern struct ll_struct   *logptr;

#ifdef __STDC__
#define SIGARGS (int _dummy)
#else
#define	SIGARGS	()
#endif

/* *******************  SOFTWARE INTERRUPT TRAPS  ********************* */

#ifndef DEBUG
sigtype
sig10 SIGARGS			  /* signal 10 interrupts to here       */
{
    signal (SIGBUS, SIG_DFL);
    ll_err (logptr, LLOGFAT, "Dying on sig10: bus error");
    sigabort ("sig10");
}

sigtype
sig12 SIGARGS			  /* signal 12 interrupts to here       */
{
    signal (SIGSYS, SIG_DFL);
    ll_err (logptr, LLOGFAT, "Dying on sig12: bad sys call arg");
    sigabort ("sig12");
}
#endif /* DEBUG */

sigtype
sig13 SIGARGS			  /* signal 13 interrupts to here       */
{
    signal (SIGPIPE, SIG_DFL);
    ll_err (logptr, LLOGFAT, "Dying on sig13: pipe write w/no reader");
    sigabort ("sig13");
}
/**/

siginit ()			  /* setup interrupt locations          */
{
    extern sigtype mmdftimeout();

#ifndef DEBUG
    signal (SIGBUS, sig10);
    signal (SIGSYS, sig12);
#endif /* DEBUG */
    signal (SIGPIPE, sig13);
    signal (SIGALRM, mmdftimeout);    /* in case alarm is set & fires       */
}


/* ***********************  TIME-OUT TRAP  **************************** */

jmp_buf timerest;                 /* where to restore to, from mmdftimeout */
				  /* not external, if not used outside  */
				  /* otherwise, set by return routine   */
int     flgtrest;                 /* TRUE, if timerest has been set     */

sigtype
mmdftimeout SIGARGS			  /* alarm timeouts/interrupts to here  */
{
    extern int errno;

    signal (SIGALRM, mmdftimeout);    /* reenable interrupt for here        */

    if (flgtrest) {
    	errno = EINTR;
    	flgtrest = 0;
	longjmp (timerest, TRUE);
    }

    ll_log (logptr, LLOGFAT, "ALARM signal");
    return;                       /* probably generate an i/o error     */
}

s_alarm(n)
unsigned int n;
{
    extern sigtype mmdftimeout();

    flgtrest = ((n == 0) ? 0 : 1);
/* for masscomp til bugfix */
    if (n>0)
	signal(SIGALRM, mmdftimeout);
    alarm(n);
}

/* ARGSUSED */

sigabort (thesig)
    char thesig[];
{
    signal (SIGHUP, SIG_DFL);
    signal (SIGINT, SIG_DFL);
    signal (SIGILL, SIG_DFL);
    signal (SIGBUS, SIG_DFL);
    signal (SIGSYS, SIG_DFL);
    signal (SIGPIPE, SIG_DFL);
    signal (SIGALRM, SIG_DFL);

#ifdef DEBUG
    abort ();
#endif

    exit (NOTOK);
    /* NOTREACHED */
}
