/*	@(#)mm_io.c	1.1	91/11/21 09:53:09 */
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
/*  mm_io:  low-level i/o for local mail submission mail               */

/*  May, 80 Dave Crocker    lm_chend, move kill before closes
 *  Jun, 80 Dave Crocker    lm_chinit, to make submit reply channel
 *                          separate from standard output
 *                          mm_cinit, make regfdaray-setting loop tmp<=NOTOK
 */

#include "nexec.h"
#include "util.h"
#include "mmdf.h"

#define LM_SUBMIT   0
#define LM_PICKUP   1

extern struct ll_struct *logptr;

extern int  *regfdary;           /* nexecl's fd rearrangement array   */
extern char *cmddfldir;
extern char *pathsubmit,         /* program to invoke for submission   */
	    *namsubmit,          /* name of it                         */
	    *pathpkup,           /* program to invoke for pickup       */
	    *nampkup;            /* name of it                         */
extern int numfds;

LOCFUN mm_cinit(), mm_cend();

LOCVAR FILE *mm_rfp,           /* pipe input stream                  */
	    *mm_wfp;           /* pipe output stream                 */

LOCVAR int    mm_cid;          /* process id of child mail process      */

/* ************  (mm_)  LOCAL MAIL I/O SUB-MODULE  ****************** */

mm_init ()                        /* get ready for local mail processing  */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_init ()");
#endif
    return (RP_OK);
}

mm_end (type)                     /* done with mail process             */
int       type;
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_end ()");
#endif
    mm_cend (type);          /* get rid of child                   */
    return (RP_OK);
}

mm_sbinit ()                      /* initialize local submission        */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_sbinit ()");
#endif
    return (mm_cinit ((char *) 0));       /* indicate not a pickup              */
}

mm_sbend ()                       /* done with submission               */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_sbend ()");
#endif
    return (mm_cend (OK));
}

mm_pkinit (chname)                /* initialize local pickup            */
    char chname[];                /* name of channel being picked up    */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_pkinit ()");
#endif
    return (mm_cinit (chname));
}

mm_pkend ()                       /* done with pickup                   */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_pkend ()");
#endif
    return (mm_cend (OK));
}
/*                    PROCESS REPLIES                                 */

mm_rrply (valstr, len)           /* get a reply from remote process    */
struct rp_bufstruct *valstr;      /* where to stuff copy of reply text  */
int    *len;                      /* where to indicate text's length    */
{
    short     retval;
    char   *rplystr;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_rrply()");
#endif

    retval = mm_rrec ((char   *) valstr, len);
    if (rp_gval (retval) != RP_OK)
	return (retval);

#ifdef RUNALON
    switch (rp_gval (valstr -> rp_val))
    {
	case 'd':
	    valstr -> rp_val = RP_DONE;
	    break;
	case 'y':
	    valstr -> rp_val = RP_OK;
	    break;
	case 'm':
	    valstr -> rp_val = RP_MOK;
	    break;
	case 'a':
	    valstr -> rp_val = RP_AOK;
	    break;
	case 'n':
	    valstr -> rp_val = RP_NO;
	    break;
    }
#endif

    rplystr = rp_valstr (valstr -> rp_val);
    if (*rplystr == '*')
    {				  /* replyer did a no-no                */
	ll_log (logptr, LLOGTMP, "ILLEGAL REPLY: (%s)", rplystr);
	valstr -> rp_val = RP_RPLY;
    }
#ifdef DEBUG
    else
	ll_log (logptr, LLOGFTR, "(%s)'%s'", rplystr, valstr -> rp_line);
#endif

    return (RP_OK);
}


mm_wrply (valstr, len)           /* pass a reply to local process      */
struct rp_bufstruct *valstr;      /* string describing reply            */
int     len;                      /* length of the string               */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_wrply() (%s) '%s'",
	    rp_valstr (valstr -> rp_val), valstr -> rp_line);
#endif

    return (mm_wrec ((char *) valstr, len));
}
/*         READ DATA FROM LOCAL MAIL (SUB)PROCESS                     */

mm_rrec (linebuf, len)           /* read one "record"                    */
char   *linebuf;		  /* where to stuff the text              */
int    *len;                      /* where to stuff the length count      */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_rrec ()");
#endif

    switch (*len = gcread (mm_rfp, linebuf, LINESIZE - 1, "\000\n\377"))
    {				  /* a record == one line                 */
	case NOTOK:
	    ll_log (logptr, LLOGFAT, "read error");
	    return (RP_LIO);

	case OK:
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "eof");
#endif
	    return (RP_EOF);

	case 1:
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "DONE");
#endif
	    return (RP_DONE);     /* the only valid one-char record     */
    }
    *len -= 1;			  /* cut off the newline                */
    linebuf[*len] = '\0';	  /* keep things null-terminated        */
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "(%d)'%s'", *len, linebuf);
#endif
    return (RP_OK);
}
/**/

mm_rstm (buffer, len)            /* read buffered block of text        */
char   *buffer;			  /* where to stuff the text            */
int    *len;                      /* where to stuff count               */
{
    static char goteof;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_rstm ()");
#endif

    if (goteof)
    {
	goteof = FALSE;
	*len = 0;
	return (RP_DONE);
    }
    switch (*len = gcread (mm_rfp, buffer, LINESIZE - 1, "\000\377"))
    {				  /* read until full                    */
	case NOTOK:
	    ll_log (logptr, LLOGFAT, "read error");
	    return (RP_LIO);

	case OK:
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "DONE");
#endif
	    return (RP_DONE);
    }
    buffer[*len] = '\0';
    if (isnull (buffer[*len - 1]))
    {
	goteof = TRUE;
	*len -= 1;
    }
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "(%d)'%s'", *len, buffer);
#endif
    return (RP_OK);
}
/*            WRITE DATA TO LOCAL MAIL (SUB)PROCESS                   */

mm_wrec (buf, len)		  /* write a record/packet              */
char    *buf;			  /* chars to write                     */
int     len;                      /* number of chars to write           */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_wrec () (%d)'%s'", len, buf ? buf : "");
#endif

    if (!isstr(buf) || len == 0)
	(void) putc ('\0', mm_wfp);     /* send an end-of-stream        */
    else
    {                             /* send the record text               */
	fnput (buf, len, mm_wfp);
	(void) putc ('\n', mm_wfp);
    }
    fflush (mm_wfp);             /* force it out                       */

    if (ferror (mm_wfp))
    {				  /* direct write                       */
	ll_log (logptr, LLOGFTR, "write error");
	ll_log (logptr, LLOGFTR, "errno = %d",errno);
	clearerr(mm_wfp);
	return (RP_LIO);
    }

    return (RP_OK);
}
/**/

mm_wstm (buffer, len)            /* write next part of char stream     */
char    *buffer;		  /* chars to write                     */
int     len;                      /* number of chars to write           */
{
    register char   doflush;      /* flush all the text out?            */

    doflush = (buffer == 0 || len == 0);

#ifdef DEBUG
    if (doflush)
	len = 5;

    ll_log (logptr, LLOGBTR, "mm_wstm (): (%d)\"%.*s\"",
	    len, len, (doflush) ? "[EOF]" : buffer);
#endif

    if (doflush)
	fflush (mm_wfp);
    else
	fnput(buffer, len, mm_wfp);

    if (ferror (mm_wfp))
    {
	ll_log (logptr, LLOGFAT, "write error");
	return (RP_LIO);
    }
    return (RP_OK);
}
/*            LOCAL MAIL PROCESS SYSTEM CREATE/DELETE CALLS           */

LOCFUN
	mm_cinit (chname)        /* get a mail process                 */
char    chname[];                 /* name of channel to invoke          */
{
    Pip    pkpin,
	   pkpout;
    short     tmp;
    char    temppath[LINESIZE];
    char    chbuf[LINESIZE];

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_cinit()");
#endif

#ifdef RUNALON
    mm_rfp = stdin;
    mm_wfp = stdout;
    return (RP_OK);
#endif

    if (pipe (pkpin.pipcall) < OK || pipe (pkpout.pipcall) < OK)
	err_abrt (RP_LIO, "Unable to pipe() in mm_pkinit()");

    regfdary[0] = pkpout.pip.prd;     /* set up io fd's for the child       */
    if (chname == 0)              /* make reply channel separate from   */
    {                             /*  regular text output for submit    */
	regfdary[1] = 1;
	regfdary[2] = pkpin.pip.pwrt;
    }
    else                          /* deliver keeps them merged          */
    {
	regfdary[1] = pkpin.pip.pwrt;
	regfdary[2] = CLOSEFD;
    }
    for (tmp = numfds-1; tmp > 2; tmp--)
	regfdary[tmp] = CLOSEFD;
    if (chname == 0)              /* exec submit                        */
    {
	getfpath (pathsubmit, cmddfldir, temppath);
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "Forking %s", temppath);
#endif
	if ((mm_cid = nexecl (FRKEXEC, FRKCIGO, regfdary,
			temppath, namsubmit, (char *)0)) == NOTOK)
	    err_abrt (RP_NO, "Unable to nexecl ('%s')", temppath);
    }
    else                          /* exec pickup process (deliver)      */
    {
	getfpath (pathpkup, cmddfldir, temppath);
	sprintf (chbuf, "-c%s", chname);

#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "Forking %s (%s)", temppath, chbuf);
#endif
	if ((mm_cid = nexecl (FRKEXEC, 0, regfdary,
			temppath, nampkup, "-p", chbuf, (char *)0)) == NOTOK)
	    err_abrt (RP_NO, "Unable to nexecl ('%s')", temppath);
    }

    (void) close (pkpout.pip.prd);
    (void) close (pkpin.pip.pwrt);
    mm_rfp = fdopen (pkpin.pip.prd, "r");
    mm_wfp = fdopen (pkpout.pip.pwrt, "w");
ll_log(logptr,LLOGFTR,"mm_rfp = %d  mm_wfp = %d",fileno(mm_rfp),fileno(mm_wfp));
    return (RP_OK);
}

LOCFUN
	mm_cend (type)           /* get rid of local mail process      */
int     type;
{
    register int   status;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mm_cend (%d)", type);
#endif

#ifdef RUNALON
    return (OK);
#endif

    if (mm_cid == 0)
	return (OK);

/*  if and ending is dirty, try to kill the child and preemptively
 *  close the file descriptors (if open).  a close, rather than fclose,
 *  is used to avoid the chance of writing to a broken pipe.
 */

    if (type != OK)
    {
	kill (mm_cid, 9);
	if (mm_wfp != (FILE *) EOF && mm_wfp != NULL){
	    (void) close (fileno (mm_wfp));
	    fclose(mm_wfp);
	}
	if (mm_rfp != (FILE *) EOF && mm_rfp != NULL){
	    (void) close (fileno (mm_rfp));
	    fclose(mm_rfp);
	}
    }
    else
    {
	if (mm_wfp != (FILE *) EOF && mm_wfp != NULL)
	    fclose (mm_wfp);
	if (mm_rfp != (FILE *) EOF && mm_rfp != NULL)
	    fclose (mm_rfp);
    }

    mm_wfp = mm_rfp = NULL;
    status = pgmwait (mm_cid);
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "child = %s", rp_valstr (status));
#endif
    mm_cid = 0;
    return (status);
}
