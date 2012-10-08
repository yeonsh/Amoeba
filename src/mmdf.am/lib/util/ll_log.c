/*	@(#)ll_log.c	1.1	92/05/14 10:39:30 */
#include "util.h"
#include "conf.h"
#include <sys/stat.h>
#include "ll_log.h"

/*  Package for logging entries into files                              */

/*      David H. Crocker, Dept. of Electrical Engineering               */
/*      University of Delaware; October 1979                            */

/*  Failure to open a log file, other than due to its being busy,
 *  causes the fd associated with ll_struct to be set to -1.
 *  Later calls then result in a no-op, except for openllog & closellog
 */

/*
 *  Modified to use 4.2BSD's O_APPEND mode unless the file is in cycle mode
 *  Peter Collinson UKC March 1987
 *  Probably would be better to totally rewrite this, but that seems
 *  a little dangerous and is probably not good engineering practice
 */
#ifdef	V4_2BSD
#include <sys/file.h>
#endif

#define _LLRETRY  5               /* number of retries if log locked    */
#define _LLSLEEP  5               /* time between tries                 */


#define SHORTIM   0		  /* Short time format                  */
#define LONGTIM   1		  /* Long time format                   */
#define _LLDIDC 0200		  /* Already did cycle, this opening    */

extern char *strdup ();
extern time_t time();
extern int errno;

LOCFUN ll_tmout(), ll_cycle(), ll_size(), ll_time();

LOCVAR int errno_save;

LOCVAR char
	    _cycling[] = "\n\n***  CYCLING  ***\n",
	    _cycled[] = "***  CYCLED  ***\n";

/* *******************  LOG OPENING AND CLOSING  ******************** */

ll_open (loginfo)                /* physically open the file           */
register struct ll_struct *loginfo;
{
    short     retries;
    int	      opnflg;
    
    if (loginfo -> ll_file == 0 || *(loginfo -> ll_file) == '\0')
	return (loginfo -> ll_fd = NOTOK);
				  /* no pathname => no place to log       */

#ifdef V4_2BSD
    opnflg = (loginfo -> ll_stat & LLOGCYC) ? O_WRONLY : (O_WRONLY|O_APPEND);
#else
    opnflg = 1;
#endif
    if (loginfo -> ll_fd <= 0)   /* now closed; ALWAYS try               */
	for (retries = _LLRETRY;
		(loginfo -> ll_fd = open (loginfo -> ll_file, opnflg)) <= 0;
		sleep (_LLSLEEP))
	{                         /* exclusive & write access             */
	    if (errno != 26 ||    /* not simply busy                    */
		    !(loginfo -> ll_stat & LLOGWAT) ||
		    retries-- <= 0)
		return (loginfo -> ll_fd = NOTOK);
	}

    if (loginfo -> ll_timmax <= 0)
	loginfo -> ll_timmax = LLTIMEOUT;
    loginfo -> ll_timer = time((time_t *)0);
    loginfo -> ll_fp = fdopen(loginfo -> ll_fd, "w");
    return (ll_init (loginfo));
}


ll_close (loginfo)
register struct ll_struct *loginfo;
{                                 /* maybe close file; always clear fd  */
    register int    fd;

    fd = loginfo -> ll_fd;
    loginfo -> ll_fd = 0;        /* clear the fd: erase -1 or handle   */

    if (loginfo -> ll_stat & _LLDIDC)
    {                             /* cycled during this open            */
	if (fd > 0)               /* flag end, from rest of log         */
	    fwrite (&_cycled[5], sizeof(char), (sizeof _cycled) - 6,
			loginfo -> ll_fp);
	loginfo -> ll_stat &= ~_LLDIDC;
    }                             /* clear cycled flag                  */

    return ((fd > 0) ? fclose (loginfo -> ll_fp) : 0);
				  /* -1 or 0 => nothing was open        */
}
/* *********************  LOG INITIALIZATION  *********************** */

ll_hdinit (loginfo, pref)              /* make header field unique           */
register struct ll_struct *loginfo;
register char *pref;
{
    char *curhdr;
    char tmpstr[16];
    static int pid;

    if (pid == 0)
    	pid = getpid();
	
    if ((curhdr = pref) == (char *) 0)
	if ((curhdr = loginfo -> ll_hdr) == (char *) 0)
	    curhdr = "XXXXXX";

    (void) sprintf (tmpstr, "%-6.6s%04d", curhdr, pid % 10000);
				/* use last 4 digits of procid          */

    loginfo -> ll_hdr = strdup (tmpstr);
    ll_close (loginfo);          /* start with a clean pointer        */
}

ll_init (loginfo)                /* position log [print header]        */
register struct ll_struct *loginfo;
{
    if (loginfo -> ll_fd < 0)
	return (NOTOK);

#ifdef V4_2BSD
    if ((loginfo -> ll_stat & LLOGCYC) == 0) /*..... don't fseek */
#endif
    fseek (loginfo -> ll_fp, 0L, 2);


    if ((loginfo->ll_stat & LLOGCYC) && (ll_cycle (loginfo) < OK))
	return (NOTOK);		  /* too big and can't cycle            */

    if ((loginfo -> ll_stat & (LLOGCLS | LLOGTIMED)))
	return (OK);              /* don't do header for opening        */

    if (loginfo -> ll_hdr == (char *) 0) /* give it a place-holder      */
	loginfo -> ll_hdr = "";

    ll_time (loginfo, LONGTIM);
    errno_save = errno;
    fprintf (loginfo -> ll_fp, "---  OPEN  ---(%d)\n", loginfo -> ll_fd);
    errno = errno_save;

    return (OK);
}

/* *********************  DO THE LOGGING  *************************** */

/* VARARGS3 */
ll_log (loginfo, verblev, format, a1, a2, a3, a4, a5, a6, a7, a8, a9, a0)
register struct ll_struct *loginfo;
int     verblev;
char   *format,
       *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8, *a9, *a0;
{
    if (verblev > loginfo -> ll_level)
	return (OK);              /* too verbose for this setting       */

    switch (loginfo -> ll_fd)
    {
	case NOTOK: 		  /* error in its past                  */
	    return (OK);

	case 0:
	    if (ll_open (loginfo) < OK)
		return (NOTOK);   /* couldn't open it properly          */
	    break;

	default: 		  
	    if (loginfo -> ll_stat & LLOGTIMED)
	    {
	        if (ll_tmout(loginfo) < OK)
		    return (NOTOK);
	    }
	    else if (ll_cycle (loginfo) < OK)  /* check file size	*/
		return (NOTOK);
    }

    ll_time (loginfo, 
	     (loginfo -> ll_stat & (LLOGCLS | LLOGTIMED)) ? LONGTIM : SHORTIM);
    errno_save = errno;
    fprintf(loginfo->ll_fp,
		format, a1, a2, a3, a4, a5, a6, a7, a8, a9, a0);
    (void) putc('\n', loginfo->ll_fp);
    errno = errno_save;

    fflush(loginfo->ll_fp);

    if (loginfo -> ll_stat & LLOGCLS)
    {
	if (ll_close (loginfo) == EOF)
	    return (loginfo -> ll_fd = NOTOK);
    }
				  /* error or close after each entry    */
    return (OK);
}

/* *********************  INTERNAL ROUTINES  ************************ */

LOCFUN
    ll_tmout(loginfo)
register struct ll_struct *loginfo;
{
    int temp;
    time_t now;

    now = time((time_t *)0);

    if ((now - (loginfo->ll_timer)) > (loginfo -> ll_timmax))
    {

	if ((temp = ll_close(loginfo)) < OK)
	    return(temp);

	return (ll_open(loginfo));
    }

    return (ll_init(loginfo));
}


LOCFUN
	ll_cycle (loginfo)        /* file not within limits => cycle     */
register struct ll_struct *loginfo;
{
    int stats;
    register int    numblocks,
		    maxblocks;

    if ((maxblocks = loginfo -> ll_msize) <= 0)
	return (OK);

    maxblocks *= 25;              /* units of 25 blocks                   */
    numblocks = ll_size (loginfo);

    if ((stats = loginfo -> ll_stat) & _LLDIDC)
    {                             /* different test if already cycled     */
	if (numblocks > maxblocks + 1)
	    goto endit;           /* the sucker REwrote length of file    */
    }
    else
	if (numblocks >= maxblocks)
	{                         /* normal uncycled check                */
	    if ((stats & LLOGCLS) || !(stats & LLOGCYC))
	    {                     /* give up if one-liner or no cycling   */
endit:
		ll_close (loginfo);
		return (loginfo -> ll_fd = NOTOK);
	    }
				  /* record status of llog file           */
	    loginfo -> ll_msize = numblocks / 25;
	    loginfo -> ll_stat |= _LLDIDC;
				  /* make notes in it & goto beginning    */

	    fseek (loginfo->ll_fp, -20L, 1);
	    fwrite(_cycling, sizeof(char), sizeof _cycling - 1,
			loginfo->ll_fp);
	    fseek (loginfo->ll_fp, 0L, 0);
	    fwrite(_cycled, sizeof(char), sizeof _cycled - 1,
			loginfo->ll_fp);
	}
    return (OK);
}
/**/

LOCFUN
	ll_size (loginfo)       /* how big is log file?               */
register struct ll_struct *loginfo;
{
    struct stat llstat;

    if(fflush(loginfo->ll_fp) == EOF)  /* must show what is buffered too */
	return (0);
    if (fstat(loginfo -> ll_fd, &llstat) < 0)
	return (0);

    /*NOSTRICT*/
    return ((st_gsize (&llstat) + 1023L)/1024L);
}
/**/

LOCFUN
	ll_time (loginfo, timtype)
register struct ll_struct *loginfo;
int     timtype;
{				  /* record real date/time              */
    extern struct tm   *localtime ();
    time_t clock;
    register struct tm *tpt;

    time (&clock);
    tpt = localtime (&clock);

    errno_save = errno;
    switch (timtype)
    {
	case LONGTIM: 		  /* mm/dd hh:mm:ss                       */
	    fprintf (loginfo->ll_fp, "%2d/%2d %2d:%02d:%02d %s:  ",
		    tpt->tm_mon + 1, tpt->tm_mday, tpt->tm_hour,
		    tpt->tm_min, tpt->tm_sec, loginfo -> ll_hdr);
	    break;

	case SHORTIM: 		  /* mm:ss                                */
	    fprintf (loginfo->ll_fp, "%2d:%02d ", tpt->tm_min, tpt->tm_sec);
    }
    errno = errno_save;
}
