/*	@(#)ch_io.c	1.1	91/11/21 11:03:28 */
#include "util.h"
#include "mmdf.h"

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
 *     version   1    David H. Crocker    October 1981
 *
 */
/*  CH_IO:  Channel invocation & communication                          */

/*  Jun 80 Dave Crocker     fix ch_close fildes setting to -1
 *  Nov 80 Dave Crocker     convert to ch_struct pointer from ch_code
 *  Jul 81 Dave Crocker     ch_rdrec a little cleaner on error handling
 */

#define TIMEOUT 3600             /* 60 minute sanity timer on channel IO */

#include "ch.h"
#include <signal.h>
#include "nexec.h"

extern char pgm_bakgrnd;
extern LLog *logptr;
extern char *chndfldir;
extern int  *regfdary;           /* for nexec                         */
extern int  numfds;

extern char *blt();

LOCFUN ch_rrec(), ch_exec();

Chan    *ch_curchan;               /* currently active channel program   */
RP_Buf ch_rp;         /* last channel reply text            */

/* ***************  (ch_) COMMUNICATE WITH CHANNEL  ***************** */

LOCVAR FILE *ch_outfp,            /* output file handle for chan */
	   *ch_infp;              /* input file handle for channel */

LOCVAR int    ch_child;           /* Process id of channel program      */
LOCVAR sigtype (* ch_opipe) ();   /* old pipe action value              */

LOCFUN sigtype
ch_catch ()			  /* catch interrupt                    */
{
}

ch_end (type)                     /* get ready to send mail to channels */
    int type;
{                                 /* NOTOK => close; 0 => addr list end */
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "ch_end (%d)", type);
#endif

    return (ch_sbend (type));
}
/**/

ch_sbinit (newchan)               /* "get" a specific channel           */
register Chan  *newchan;          /* pointer to chan structure          */
{                                 /* NOTOK => close; 0 => addr list end */
    int temp;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "ch_open(%s)", newchan -> ch_name);
#endif

    if (newchan == ch_curchan)
	return (RP_OK);           /* already have the channel opened    */

    if (!pgm_bakgrnd && (newchan -> ch_access & DLVRBAK))
	return (RP_NOOP);         /* not allowed unless background      */

    if (ch_curchan != OK)         /* already have chan -> get rid of it */
	ch_end (OK);

    newchan -> ch_access |= DLVRDID; /* note that we did this channel      */

    if ((temp = ch_exec (newchan)) == RP_OK)
	ch_curchan = newchan;

    ch_opipe = signal (SIGPIPE, ch_catch);
    signal (SIGALRM, ch_catch);
    return (temp);
}
/**/

ch_sbend (type)               /* free the channel                   */
int type;                     /* clean ending / abort               */
{
    register int status;

#ifdef DEBUG
    ll_log(logptr, LLOGBTR, "ch_end (%d)", type);
#endif

    if (ch_child == 0)            /* don't have one                     */
	return (OK);

    switch (type)
    {
	case NOTOK:               /* ABORTING;  try to kill child       */
	    ll_log (logptr, LLOGTMP, "Trying to kill child  '%s' pid = (%d)",
			ch_curchan -> ch_name, ch_child);
	    kill (ch_child, SIGKILL);
	    goto closeit;

	case OK:                  /* normal end; just eof the pipes     */
	    ch_wrec ("end");      /* say goodbye */

    closeit:
	    if (ch_outfp == (FILE *) EOF)
	        return(RP_OK);    /* ch_wrec might have already closed  */

	    fclose (ch_outfp);
	    fclose (ch_infp);

	    ch_outfp =            /* flag i/o ports as closed           */
		ch_infp = (FILE *) EOF;
	    signal (SIGPIPE, ch_opipe);
				  /* probably set it back to abort      */
	    break;

	default:
	    err_abrt (RP_NCMD, "Illegal closedest call (%d)", type);
    }

    status = pgmwait (ch_child);  /* pickup child's stats               */
    if (status == NOTOK)          /* abnormal proces termination        */
	ll_err (logptr, LLOGTMP, "(%s) child NOTOK", ch_curchan -> ch_name);

    if (rp_gbval (status) == RP_BNO)
	ll_log (logptr, LLOGTMP, "(%s) child (%s)",
		    rp_valstr (status), ch_curchan -> ch_name);
				  /* log major error returns only       */

    ch_curchan = OK;              /* mark current channel as "none"     */
    ch_child = 0;                 /* don't have one any more            */
    return (RP_OK);
}
/**/

ch_rrply (valstr, len)     /* read reply from channel            */
    RP_Buf *valstr;
    int *len;
{
    int     result;

#ifdef notdef
    ll_log(logptr, LLOGBTR, "ch_rrply()");
#endif

    ch_rp.rp_val = RP_NOOP;
    (void) strcpy (ch_rp.rp_line, "(No reason given)");

    result = ch_rrec ((char *) valstr, len);


#ifdef RUNALON
    switch (valstr -> rp_val)
    {
	case 'y':
	    result = RP_OK;
	    break;
	case 'n':
	    result = RP_NO;
	    break;
	case 'a':
	    result = RP_AOK;
	    break;
	case 'd':
	    result = RP_DOK;
	    break;
	case 'm':
	    result = RP_MOK;
	    break;
	case 't':
	    result = RP_TIME;
	    break;
	case 'c':
	    result = RP_NET;
    }
#endif

    *len -= 1;
    if ((*len > 0) && (*len <= RP_LINEBUF_MAX)) {
	blt (valstr -> rp_line, ch_rp.rp_line, *len);
	ch_rp.rp_line[*len] = '\0';
    }
    else
	*len = 0;


#ifdef DEBUG
    ll_log(logptr, LLOGFTR, "reply: (%s) '%s'",
	    rp_valstr (valstr -> rp_val), valstr -> rp_line);
#endif

    ch_rp.rp_val = valstr -> rp_val;

    if (rp_isbad (result))
	return (result);

    return (RP_OK);
}
/**/

ch_wrec (str)                     /* send info to channel               */
char    str[];                    /* string to send                     */
{
#ifdef notdef
    ll_log(logptr, LLOGBTR, "ch_wrec ()");
#endif
    ll_log(logptr, LLOGFTR, "ch_wrec (): deliver writes '%s' to channel\n",str);
				  /* default error message              */
    if (ch_curchan == OK)         /* channel isn't really there         */
	return (RP_DHST);

#ifdef DEBUG
    ll_log(logptr, LLOGFTR, "sending:  '%s'", str);
#endif
#ifdef RUNALON
    printx ("Channel output:  '%s'\n", str);
#else
    if (setjmp (timerest)) {
	ll_err (logptr, LLOGTMP, "(%s) output err to channel",
		ch_curchan -> ch_name);
	ch_sbend (NOTOK);         /* close it                           */
	return (RP_NET);          /* class it as a "network" error      */
    }
    s_alarm (TIMEOUT);
    if (str)                  /* send it to channel                 */
	fputs (str, ch_outfp);

    putc ('\0', ch_outfp);
    fflush (ch_outfp);	/* This causes real IO */
    s_alarm (0);

    if (ferror (ch_outfp))
    {
	ll_err (logptr, LLOGTMP, "(%s) output err to channel",
		ch_curchan -> ch_name);
	ch_sbend (NOTOK);         /* close it                           */
	return (RP_NET);          /* class it as a "network" error      */
    }
    return (RP_OK);
#endif
}
/**/

LOCFUN
	ch_rrec (rplyline, len)    /* read reply from channel            */
    char *rplyline;
    int *len;
{
    int     result;
    char *valstr;

#ifdef notdef
    ll_log(logptr, LLOGBTR, "ch_rrec()");
#endif
#ifdef RUNALON
    printx ("Chrcv:  ");
#endif

    if (setjmp (timerest)) {
	ll_err (logptr, LLOGTMP, "(%s) read err from channel",
		    ch_curchan -> ch_name);
	ch_end (NOTOK);             /* probably not there, anyhow         */
	return (RP_NET);
    }
    s_alarm (TIMEOUT);
    result = RP_OK;
    rplyline[0] = getc (ch_infp); /* one-char rp.h reply value          */
    s_alarm (0);

    *len = 1;
    if (feof (ch_infp))
    {
	ll_log (logptr, LLOGTMP, "(%s) premature channel eof",
		    ch_curchan -> ch_name);
	ch_end (NOTOK);             /* probably not there, anyhow         */
	return (RP_NET);
    }
    if (ferror (ch_infp))
    {
	ll_err (logptr, LLOGTMP, "(%s) read err from channel",
		    ch_curchan -> ch_name);
	ch_end (NOTOK);             /* probably not there, anyhow         */
	return (RP_NET);
    }

#ifdef RUNALON
    switch (rplyline[0])
    {
	case 'y':
	    rplyline[0] = RP_OK;
	    break;
	case 'n':
	    rplyline[0] = RP_NO;
	    break;
	case 'a':
	    rplyline[0] = RP_AOK;
	    break;
	case 'd':
	    rplyline[0] = RP_DOK;
	    break;
	case 'm':
	    rplyline[0] = RP_MOK;
	    break;
	case 't':
	    rplyline[0] = RP_TIME;
	    break;
	case 'c':
	    rplyline[0] = RP_NET;
    }
#endif
    if (setjmp (timerest)) {
	ll_err (logptr, LLOGTMP, "(%s) Channel died", ch_curchan -> ch_name);
	*len += 1;
	return (rplyline[0] = result = RP_NET);
    }
    s_alarm (TIMEOUT);
    if ((*len = gcread (ch_infp, &rplyline[1], LINESIZE - 2, "\n\000\377"))
			== 0)
    {
	ll_err (logptr, LLOGTMP, "(%s) Channel died", ch_curchan -> ch_name);
	rplyline[0] =
	    result = RP_NET;
    }
    s_alarm (0);

    valstr = rp_valstr (rplyline[0]);
    if (*valstr == '*')
    {
	ll_log (logptr, LLOGFAT, "(%s) (%s) '%s'",
		ch_curchan -> ch_name, valstr, &rplyline[1]);
	return (RP_RPLY);
    }

    *len += 1;
#ifdef DEBUG
    ll_log(logptr, LLOGFTR, "record: (%s) '%s'", valstr, &rplyline[1]);
#endif
    return (result);
}
/**/

LOCFUN
	ch_exec (thechan)         /* invoke process which "is" channel  */
Chan   *thechan;      /* channel descriptor structure       */
{
    register  tmp;
    int     nampipe[2];           /* to xmit process for addr name        */
    int     respipe[2];           /* from xmit process, for responses     */
    int     oldpid;               /* parent pid */
    char    temppath[128];
    char    arg1[10];             /* fd for input (names/addrs)         */
    char    arg2[10];             /* fd for output (responses)          */
    char    arg3[20];             /* options                            */

#ifdef notdef
    ll_log(logptr, LLOGBTR, "ch_exec()");
#endif
#ifdef RUNALON
    printx ("Forking %s\n", thechan -> ch_name);
    ch_infp = fdopen (0, "r");
    ch_outfp = fdopen (1, "w");

    return (RP_OK);               /* debug mode                         */
#endif;

    if (pipe (nampipe) < OK)
    {
	ll_err(logptr, LLOGTMP, "problem with 1st ch_exec pipe");
	return (RP_NO);
    }
    if (pipe (respipe) < OK)
    {
	ll_err(logptr, LLOGTMP, "problem with 2d ch_exec pipe");
	(void) close (nampipe[0]);
	(void) close (nampipe[1]);
	return (RP_NO);
    }
ll_log(logptr,LLOGFTR,"ch_exec() : before forking nampipe[0] = %d, nampipe[1] = %d, respipe[0] = %d, respipe[1] = %d",nampipe[0],nampipe[1],respipe[0],respipe[1]);
    sprintf (arg1, "%d", nampipe[0]);
    sprintf (arg2, "%d", respipe[1]);
    arg3[0] = '\0';
    if( domsg == TRUE )
	strcat( arg3, "w" );

    regfdary[0] = 0;              /* must keep open, in case of POBox   */
    regfdary[1] = 1;              /*   or other interaction with caller */

    for(tmp=2; tmp < numfds; regfdary[tmp++] = CLOSEFD);

    regfdary[nampipe[0]] = nampipe[0];
    regfdary[respipe[1]] = respipe[1];

    getfpath (thechan -> ch_ppath, chndfldir, temppath);

#ifdef DEBUG
    ll_log(logptr, LLOGBTR, "[%s]'%s' ('%s','%s', '%s')",
		temppath, thechan -> ch_name, arg1, arg2, arg3);

#endif
    ll_close (logptr);            /* since we are about to fork()       */
    fflush (stdout);
    fflush (stderr);
    oldpid = getpid ();

    ch_child = nexecl (FRKEXEC, FRKERRR, regfdary, temppath,
			thechan -> ch_name, arg1, arg2, arg3, (char *)0);

    (void) close (nampipe[0]);     /* Close other end of pipes */
    (void) close (respipe[1]);

    if (ch_child == NOTOK)        /* didn't get the child               */
    {
	ll_err (logptr, LLOGFAT, "Unable to run '%s'", thechan -> ch_ppath);
	ch_child = 0;
	(void) close (nampipe[1]);       /* Close other end of pipes     */
	(void) close (respipe[0]);
	if (getpid () == oldpid)
	    return (RP_NO);       /* still the parent => fork() error   */
	else
	    exit (RP_MECH);      /* must be child                       */
    }
				  /* clean up regfd arrary for future   */
    regfdary[nampipe[0]] = -1;
    regfdary[respipe[1]] = -1;

    ch_infp = fdopen (respipe[0], "r");
    ch_outfp = fdopen (nampipe[1], "w");

    ll_log (logptr,LLOGFTR,"[ Accessing %s (%s)]\n", 
		thechan -> ch_name, thechan -> ch_show);
    ll_log (logptr, LLOGFTR, "[ %s ] (r%d,w%d)",
		thechan -> ch_name, respipe[0], nampipe[1]);

    return (RP_OK);
}


