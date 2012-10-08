/*	@(#)ch_local.c	1.1	91/11/21 11:35:04 */
/*
 *     MULTI-CHANNEL MEMO DISTRIBUTION FACILITY  (MMDF)
 *     
 *
 *     Copyright (C) 1979,1980,1981  University of Delaware
 *     
 *     Department of Electrical Engineering
 *     University of Delaware
 *     Newark, Delaware  19711
 *
 *     Phone:  (302) 738-1163
 *     
 *     
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *     
 *     Acquisition, use, and distribution of this module and its listings
 *     are subject restricted to the terms of a license agreement.
 *     Documents describing systems using this module must cite its source.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *     
 *
 *     version  -1    David H. Crocker    March   1979
 *     version   0    David H. Crocker    April   1980
 *     version  v7    David H. Crocker    May     1981
 *     version   1    David H. Crocker    October 1981
 *
 */

#include "util.h"
#include "mmdf.h"
#include <signal.h>
#include "ch.h"
#include "phs.h"

extern Chan	*ch_nm2struct();
extern LLog *logptr;

Chan	*chanptr;

/*
 *  CH_LOCAL --  Actual delivery of mail, out of transport environment
 *
 *  lo_wtmail does most of the interesting work.  qu2lo_send handles the
 *  deliver interface and the verification. Lo_wtmail handles all the piping
 *  filing, and the maildelivery stuff and looks after return values etc.
 *
 *  in any event, this process runs setuid to root.  for each recipient, it
 *  determines the uid, etc, and then forks a child to do the actual work
 *  (do the parsing of files etc).The child does a setuid
 *  to the recipient.  the parent version of the process just waits for the
 *  child to complete.  its job is to a) acquire the next address, b) get
 *  the recipient id info, and c) fork and wait on the child.
 */

/*      MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN     */

main (argc, argv)
int     argc;
char   *argv[];
{
    extern char *dupfpath ();
    short retval;
    int   realid,
	  effecid;

    mmdf_init (argv[0]);
    siginit ();
    signal (SIGINT, SIG_IGN);   /* always ignore interrupts             */

    if ((chanptr = ch_nm2struct (argv[0])) == (Chan *) NOTOK)
    {
	ll_log (logptr, LLOGTMP, "qu2lo_send (%s) unknown channel", argv[0]);
	exit (NOTOK);
    }

    retval = ch_local (argc, argv);
    ll_close (logptr);              /* clean log end, if cycled  */
    exit (retval);
}
/***************  (ch_) LOCAL MAIL DELIVERY  ************************ */

ch_local (argc, argv)		  /* send to local machine               */
int     argc;
char   *argv[];
{
    ch_llinit (chanptr);
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "ch_local ()");
#endif

    if (rp_isbad (qu_init (argc, argv)))
	return (RP_NO);		  /* problem setting-up for deliver     */
    phs_note (chanptr, PHS_CNSTRT);         /* make a timestamp */
    phs_note (chanptr, PHS_CNGOT);         /* make a timestamp */

    if (rp_isbad (qu2lo_send ()))
	return (RP_NO);		  /* send the batch of outgoing mail    */

    qu_end (OK);                  /* done with Deliver function         */
    phs_note  (chanptr, PHS_CNEND);     /* note end of session */

    return (RP_OK);		  /* NORMAL RETURN                      */
}


/* VARARGS2 */
err_abrt (code, fmt, b, c, d)     /* terminate ourself                  */
short     code;
char    *fmt, *b, *c, *d;
{
    char linebuf[LINESIZE];

    qu_end (NOTOK);
    if (rp_isbad (code))
    {
	if (rp_gbval (code) == RP_BNO || logptr -> ll_level >= LLOGBTR)
	{                         /* don't worry about minor stuff      */
	    sprintf (linebuf, "%s%s", "err [ ABEND (%s) ]\t", fmt);
	    ll_log (logptr, LLOGFAT, linebuf, rp_valstr (code), b, c, d);
#ifdef DEBUG
	    abort ();
#endif
	}
    }
    ll_close (logptr);           /* in case of cycling, close neatly   */
    exit (code);
}
