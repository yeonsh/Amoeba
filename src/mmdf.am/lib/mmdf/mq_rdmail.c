/*	@(#)mq_rdmail.c	1.2	96/02/27 09:57:15 */
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
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *
 *     This module and its listings may be used freely by United States
 *     federal, state, and local government agencies and by not-for-
 *     profit institutions, after sending written notification to
 *     Professor David J. Farber at the above address.
 *
 *     For-profit institutions may use this module, by arrangement.
 *
 *     Notification must include the name of the acquiring organization,
 *     name and contact information for the person responsible for
 *     maintaining the operating system, name and contact information
 *     for the person responsible for maintaining this program, if other
 *     than the operating system maintainer, and license information if
 *     the program is to be run on a Western Electric Unix(TM) operating
 *     system.
 *     
 *     Documents describing systems using this module must cite its
 *     source.
 *     
 *     Development of this module was supported in part by the
 *     University of Delaware and in part by contracts from the United
 *     States Department of the Army Readiness and Materiel Command and
 *     the General Systems Division of International Business Machines.
 *     
 *     Portions of the MMDF system were built on software originally
 *     developed by The Rand Corporation, under the sponsorship of the
 *     Information Processing Techniques Office of the Defense Advanced
 *     Research Projects Agency.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *     
 *
 *     version  -1    David H. Crocker    March  1979
 *     version   0    David H. Crocker    April  1980
 *     version   1    David H. Crocker    May    1981
 *
 *	7/88	#ifndef'ed out code that was removing file locks on SYS5
 *		systems. Edward C. Bennett <edward@engr.uky.edu>
 *
 */
#include "util.h"
#include "mmdf.h"
#include <signal.h>
#include <sys/stat.h>
#include "ch.h"
#include "msg.h"
/*  msg_cite() is defined in adr_queue.h  */
#include "adr_queue.h"

#ifndef EWOULDBLOCK
#ifdef SYS5
#define EWOULDBLOCK	EACCES
#else
#define EWOULDBLOCK	EBUSY /* To handle Berkeley 4.2 and Others */
#endif
#endif

extern	LLog	*logptr;
extern	long	lseek();
extern	time_t	time();
extern	char	*supportaddr;
extern	char	*aquedir;
extern	char	*mquedir;
extern	char	*squepref;	/* string to preface sub-queue name	*/
extern	char	*lckdfldir;
extern	int	lk_open();

long	mq_gtnum();	/* reads long ascii # from addr file		*/
LOCVAR	int	mq_fd;		/* read file descriptor, saved 		*/
LOCVAR	FILE	*mq_rfp;	/* read handle, for getting entries	*/
LOCVAR	char	mq_dlm;		/* mq_gtnum's delimeter char		*/
LOCVAR	long	mq_curpos;	/* Current offset of read pointer (rfp)	*/
LOCVAR	long	mq_lststrt;	/* Offset to start of addr list		*/
LOCVAR	long	mq_optstrt;	/* Offset to options			*/
LOCVAR	Msg	*mq_curmsg;	/* SEK handle on current message	*/

mq_rkill (type)			/* give-up access to msg info		*/
    int type;			/* type of ending (currently ignored)	*/
{
#ifdef DEBUG
     ll_log (logptr, LLOGBTR, "mq_rkill (type=%d, mq_fd=%d, mq_rfp=%o)",
		type, mq_fd, mq_rfp);
#endif

    if (mq_fd != NOTOK)
	lk_close (mq_fd, (char *) 0, lckdfldir, mq_curmsg -> mg_mname);
    if (mq_rfp != (FILE *) EOF && mq_rfp != (FILE *) NULL)
	fclose (mq_rfp);

    mq_fd = NOTOK;
    mq_rfp = NULL;
}
/**/

mq_rinit (thechan, themsg, retadr)	/* gain access to info on the msg */
	Chan *thechan;			/* channel points to sub-queue */
	Msg  *themsg;
	char *retadr;			/* return addres */
{
    char msgname[LINESIZE];
    extern int errno;
    register short     len;
    int       tmpfd;
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_rinit ()");
#endif

    mq_curmsg = themsg;		/* SEK not message for file locking */

    if (thechan == (Chan *) 0)	/* do the entire queue */
    {
	sprintf (msgname, "%s%s", aquedir, themsg -> mg_mname);
    }
    else
    {
	sprintf (msgname, "%s%s/%s",
		squepref, thechan -> ch_queue, themsg -> mg_mname);
    }

    errno = 0;
    if ((mq_fd = lk_open (msgname, 2, lckdfldir, themsg -> mg_mname, 20)) < OK ||
	(tmpfd = dup (mq_fd)) < OK ||
	(mq_rfp = fdopen (tmpfd, "r+")) == (FILE *)NULL)
    {				/* msg queued in file w/its name */
ll_log(logptr,LLOGFTR,"mq_fd = %d tmpfd = %d, mq_rfp = %x, errno = %d",
		mq_fd, tmpfd, mq_rfp, errno);
	switch (errno)
	{
	    case ENOENT:	/* another deliver probably did it */
#ifdef DEBUG
		ll_log (logptr, LLOGFTR, "%s no entry", msgname);
		break;
#endif
	    case EWOULDBLOCK:	/* another deliver probably using it */
#ifdef DEBUG
		ll_log (logptr, LLOGFTR, "%s busy", msgname);
#endif
		printx ("Message '%s': busy.\n", themsg -> mg_mname);
		break;

	    default:
		printx ("can't open address list '%s'\n", msgname);
		ll_err (logptr, LLOGTMP, "can't open '%s'", msgname);
	}
	mq_rkill (NOTOK);	/* Cleanup open files */
	return (NOTOK);
    }

ll_log(logptr,LLOGFTR,"mq_fd = %d tmpfd = %d",mq_fd,tmpfd);
    mq_lststrt = 0L;
    /*NOSTRICT*/
    themsg -> mg_time = (time_t) mq_gtnum ();
    themsg -> mg_stat = (mq_dlm == ADR_DONE ? ADR_WARNED : 0);
				/* whether warning been sent */
    mq_optstrt = mq_lststrt - 1;	/* Offset to option bits */
    themsg -> mg_stat |= (char) mq_gtnum ();
    if (fgets (retadr, ADDRSIZE, mq_rfp) == NULL)
    {
	ll_err (logptr, LLOGTMP, "Can't read %s sender name", themsg -> mg_mname);
	mq_rkill (OK);
	retadr[0] = '\0';	/* eliminate old sender name */
	return (NOTOK);
    }
    len = strlen (retadr);
    mq_lststrt += len;
    if (retadr[len - 1] != '\n') {
	while (fgetc(mq_rfp) != '\n')
	{
	    mq_lststrt++;
	    if (feof(mq_rfp) || ferror(mq_rfp)) {
		ll_err (logptr, LLOGTMP, "Can't read %s sender name", themsg -> mg_mname);
		mq_rkill (OK);
		retadr[0] = '\0';	/* eliminate old sender name */
		return (NOTOK);
	    }
	}
	mq_lststrt++;

	/*  The address was probably too long, substitute orphanage */
	sprintf (retadr, "%s (Orphanage)", supportaddr);
    }
    else
	retadr[len - 1] = '\0';		/* eliminate the newline */

    mq_curpos = mq_lststrt;
    ll_log(logptr,LLOGPTR,"mq_curpos = %ld",mq_curpos);
#ifdef DEBUG
   ll_log (logptr, LLOGFTR, "name='%s' ret='%s' (fd=%d)",
	       themsg -> mg_mname, retadr, fileno (mq_rfp));
#endif
    return (OK);
}
/**/

/*
 *  mq_setpos() repositions the read pointer (rfp) but because mq_rfp is
 *	just a dup of mq_fd, mq_fd is repositioned as well.  Be careful!
 */
mq_setpos (offset)
    long offset;
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_setpos (%ld)", offset);
#endif

/*#ifndef SYS5*/
/*LISA's temp change.*/
#ifdef SYS5
    /*
     *  The following is needed due to a bug in V7/SysIII/4.nBSD Stdio
     *  which will not allow you to invalidate the incore buffer of
     *  of a file fopened for reading.   (DPK @ BRL)
     *
     *  This segment must be omitted on SYS5 machines because the act of
     *  closing and reopening the mq_rfp removes the lock that mq_fd is
     *  supposed to be holding. We can safely omit this because System V
     *  doesn't seem to have the stdio bug. (ECB 7/88)
     */
    fclose (mq_rfp);
    ll_log(logptr,LLOGFTR,"*****Closed mq_rfp.*******");
    mq_rfp = NULL;
    if ((mq_rfp = fdopen (dup(mq_fd), "r+")) == NULL) {
	mq_rkill (NOTOK);
	ll_log (logptr, LLOGFAT, "mq_setpos(), cannot reopen address list");
    }
#endif

    ll_log(logptr,LLOGFTR,"*****Offset = %d*******",offset);
    if (offset == 0L) {
	fseek (mq_rfp, mq_lststrt, 0);
	ll_log(logptr,LLOGFTR,"seek result = %d",errno);
    	mq_curpos = mq_lststrt;
    ll_log(logptr,LLOGPTR,"mq_curpos = %ld",mq_curpos);
    }
    else {
	fseek (mq_rfp, offset, 0);
	ll_log(logptr,LLOGFTR,"seek result = %d",errno);
    	mq_curpos = offset;
    ll_log(logptr,LLOGPTR,"mq_curpos = %ld",mq_curpos);
    }
}

mq_radr (theadr)	/* obtain next address in msg's queue */
    register struct adr_struct *theadr;
{
    char *arglist[20];
    int argc;
    register int len;
    char *ret;


#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_radr ()");
#endif

    theadr -> adr_que = 0;	/* null it to indicate empty entry */
    ll_log(logptr,LLOGPTR,"mq_curpos = %ld",mq_curpos);
    theadr -> adr_pos = mq_curpos;
    for (;;) {
	len = sizeof (theadr -> adr_buf);
    ll_log (logptr, LLOGFTR, "before fgets -- adr: '%s' len = %d", theadr -> adr_buf,len);
	if ((ret = fgets (theadr->adr_buf, sizeof (theadr->adr_buf), mq_rfp)) == NULL)
	{

	    if (feof (mq_rfp))
	    {
#ifdef DEBUG
		ll_log (logptr, LLOGFTR, "end of list");
#endif
		return (DONE);		/* simply an end of file */
	      }
	    if (mq_rfp == NULL){
	      ll_log(logptr,LLOGFTR,"mq_rfp = NULL");
	    }
	    if (ferror(mq_rfp)){
		ll_log(logptr,LLOGFTR,"ferror for mq_rfp");
		ll_log(logptr,LLOGFTR,"errno = %d",errno);
/*TEMP hack.*/
return(DONE);
	      }
	len = strlen (theadr -> adr_buf);
    ll_log (logptr, LLOGFTR, "adr: '%s' len = %d", theadr -> adr_buf,len);
	    ll_err (logptr, LLOGTMP, "Problem reading address");
	    ll_log (logptr, LLOGFTR, "Problem reading address");
	    return (NOTOK);
	  }
	ll_log(logptr,LLOGFTR,"mq_radr(): ret = '%s'",theadr->adr_buf);

	len = strlen (theadr -> adr_buf);
	if (theadr -> adr_buf[2] != ADR_DONE)
	    break;
	theadr -> adr_pos += len;
      }
    mq_curpos = theadr -> adr_pos + len;
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "adr: '%s'", theadr -> adr_buf);
#endif

    theadr -> adr_buf[len - 1] = '\0';
				/* so entry can be handled as string */
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "adr: '%s'", theadr -> adr_buf);
#endif
    argc = str2arg (theadr -> adr_buf, 20, arglist, (char *) 0);
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "adr: '%s'", theadr -> adr_buf);
#endif
    if (argc < 4)
    {
	ll_log (logptr, LLOGTMP, "error with queue address line");
	return (NOTOK);		/* error with address entry */
    }
    theadr -> adr_tmp   = arglist[0][0];
    theadr -> adr_delv  = arglist[1][0];
    theadr -> adr_que   = arglist[2];
    theadr -> adr_host  = arglist[3];
    theadr -> adr_local = arglist[4];


#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "adr parts: '%c' '%c' '%s' '%s' '%s'",
		theadr -> adr_tmp, theadr -> adr_delv, theadr -> adr_que,
		theadr -> adr_host, theadr -> adr_local);
#endif
    return (OK);
}
/**/

/*
 * If the message was successfully delivered, or if a permanent failure
 * resulted, indicate that this address has been handled by changing the
 * delimiter between the descriptor and address info.
 */
mq_wrply (rply, themsg, theadr)	/* modify addr's entry as per reply */
RP_Buf *rply;
Msg *themsg;
struct adr_struct *theadr;
{
    char donechr;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_wrply (%s)", themsg -> mg_mname);
#endif

    fflush (stdout);
    switch (rp_gval (rply -> rp_val))
    {
	case RP_AOK:		/* name ok; text later */
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR,
		"AOK @ offset(%ld)", (long) (theadr -> adr_pos + ADR_TMOFF));
#endif
	    donechr = ADR_AOK;		/* temporary mark */
	    ll_log(logptr,LLOGFTR,"Before lseek, errno = %d",errno);
	    lseek (mq_fd, (long) (theadr -> adr_pos + ADR_TMOFF), 0);
	ll_log(logptr,LLOGFTR,"seek result = %d",errno);
	    write (mq_fd, &donechr, 1);
	    break;

	case RP_NDEL:		/* won't be able to deliver */
	case RP_DONE:		/* finished processing this addr */
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR,
		"DONE @ offset(%ld)", (long) (theadr -> adr_pos + ADR_DLOFF));
#endif
	    donechr = ADR_DONE;
	    lseek (mq_fd, (long) (theadr -> adr_pos + ADR_DLOFF), 0);
	ll_log(logptr,LLOGFTR,"seek result = %d",errno);
	    write (mq_fd, &donechr, 1);
	    break;

	case RP_NOOP:		/* not processed, this time */
	case RP_NO:		/* no success, this time */
	default: 
	    if( theadr->adr_tmp == ADR_AOK )	/* We need to reset it */
	    {
#ifdef DEBUG
		ll_log (logptr, LLOGFTR,
		"CLR @ offset(%ld)", (long) (theadr -> adr_pos + ADR_TMOFF));
#endif
		donechr = ADR_CLR;
		lseek (mq_fd, (long) (theadr -> adr_pos + ADR_TMOFF), 0);
	ll_log(logptr,LLOGFTR,"seek result = %d",errno);
		write (mq_fd, &donechr, 1);
	    }
    }
/*TEMP -- lisa's temp change
#ifndef SYS5*/
#ifdef SYS5
    /*
     *  The following is needed due to a bug in V7/SysIII/4.nBSD Stdio
     *  which will not allow you to invalidate the incore buffer of
     *  of a file fopened for reading.   (DPK @ BRL)
     *
     *  This segment must be omitted on SYS5 machines because the act of
     *  closing and reopening the mq_rfp removes the lock that mq_fd is
     *  supposed to be holding. We can safely omit this because System V
     *  doesn't seem to have the stdio bug. (ECB 7/88)
     */
    fclose (mq_rfp);
    mq_rfp = NULL;
    if ((mq_rfp = fdopen (dup(mq_fd), "r+")) == NULL) {
	mq_rkill (NOTOK);
	ll_log (logptr, LLOGFAT, "mq_wrply(), cannot reopen address list");
    }
#endif
    ll_log(logptr,LLOGFTR,"fseek position = %ld",mq_curpos);
    fseek (mq_rfp, mq_curpos, 0);	
    ll_log(logptr,LLOGFTR,"seek result = %d",errno);
}
/**/

LOCFUN long
mq_gtnum ()			/* read a long number from adr queue	*/
{
    register long   thenum;
    register char   c;

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "mq_gtnum");
#endif
    for (thenum = 0; isdigit (c = getc (mq_rfp));
	    thenum = thenum * 10 + (c - '0'), mq_lststrt++);
    mq_lststrt++;

    mq_dlm = c;			/* note the first non-number read */
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, c < ' ' ? "(%ld)'\\0%o'" : "(%ld)'%c'",
	thenum, c);
#endif
    return (thenum);
}

mq_rwarn ()			/* note that warning has been sent	*/
{
    static char donechr = ADR_DONE;

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "WARN @ offset(%ld)", (long) (mq_optstrt));
#endif

    lseek (mq_fd, mq_optstrt, 0);
    ll_log(logptr,LLOGFTR,"seek result = %d",errno);
    write (mq_fd, &donechr, 1);
/*LISA TEMP
#ifndef SYS5*/
#ifdef SYS5
    /*
     *  The following is needed due to a bug in V7/SysIII/4.nBSD Stdio
     *  which will not allow you to invalidate the incore buffer of
     *  of a file fopened for reading.   (DPK @ BRL)
     *
     *  This segment must be omitted on SYS5 machines because the act of
     *  closing and reopening the mq_rfp removes the lock that mq_fd is
     *  supposed to be holding. We can safely omit this because System V
     *  doesn't seem to have the stdio bug. (ECB 7/88)
     */
    fclose (mq_rfp);
    mq_rfp = NULL;
    if ((mq_rfp = fdopen (dup(mq_fd), "r+")) == NULL) {
	mq_rkill (NOTOK);
	ll_log (logptr, LLOGFAT, "mq_rwarn(), cannot reopen address list");
    }
#endif
    fseek (mq_rfp, mq_curpos, 0);	/* Back to where we were */
    ll_log(logptr,LLOGFTR,"seek result = %d",errno);
    ll_log(logptr,LLOGPTR,"mq_curpos = %ld",mq_curpos);
}
