/*	@(#)mq_wtmail.c	1.1	91/11/21 11:38:29 */
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
 *     version  v7    David H. Crocker    May     1981
 *     version   1    David H. Crocker    October 1981
 *
 */
/* *****************  (mq_)  CREATE QUEUED MESSAGES   **************** */

/*
 *  Nov 81 Dave Crocker     mq_msinit make message name begin "msg."
 *                          mq_adinit permit no-warning specification
 */

#include "ch.h"
#include "adr_queue.h"
#include <sys/file.h>

extern char *multcpy();

extern LLog *logptr;
extern Chan **ch_tbsrch;
extern char *tquedir,            /* temp directory for address list    */
	    *aquedir,            /* actual address list queue          */
	    *squepref,           /* string to preface sub-queue name   */
	    *mquedir,            /* message text queue                 */
	    *supportaddr;	 /* where orphaned messages will go    */

FILE *mq_mffp;                   /* pointerto message text file buffer */
char    *mq_munique;              /* unique-part of mquedir file name   */
				  /*  public, for use as msg's name     */

LOCVAR char *mq_tunique,          /* unique-part of tquedir file name   */
	    *mq_aunique,          /* unique-part of aquedir file name   */
	    mq_tfname[FILNSIZE],  /* full pathname to tquedir file      */
	    mq_afname[FILNSIZE],  /* full pathname to aquedir file      */
	    mq_mfname[FILNSIZE];  /* full pathname to mquedir file      */
LOCVAR FILE *mq_tffp;            /* buffer to temporary address file   */
/**/

mq_winit ()                       /* initialize to write queued msgs    */
{                                 /* set-up for queue file names        */
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_init ()");
#endif

    /* multcpy used to get pointer to end of base strings */

    mq_tunique = multcpy (mq_tfname, tquedir, (char *)0);
    mq_aunique = multcpy (mq_afname, aquedir, (char *)0);
    mq_munique = multcpy (mq_mfname, mquedir, (char *)0);
}
/**/

mq_creat ()                      /* initialize for new message         */
{
    static char template[128];
    int fd;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_creat ()");
#endif
/* make the unique part of the queue entry file name.
 * modelled in behaviour on mktemp() but mktemp() iterates through
 * all the possibilities -- which means too many namei()'s get done
 * so we fake it locally.  allows a maximum of 676 messages per submit...
 */

    /* fake like mktemp */
    if (*mq_munique == '\0') {                             

	/* get pid */
	sprintf(template,"msg.aa%05.5d",getpid());

	/* append template to standard part of name */
	strcpy (mq_munique, template);
    }

    while (access(mq_mfname,0) == 0) {
	if (mq_munique[5] >= 'z') {
	    if (++mq_munique[4] > 'z')
		break;	/* error caught below */
	    mq_munique[5] = 'a';
	}
	else
	    mq_munique[5]++;
    }
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "mname = \"%s\"", mq_mfname);
#endif

    if ((fd = creat(mq_mfname, 0444)) < 0 ||
	(mq_mffp = fdopen (fd, "w")) == NULL)
	err_abrt (RP_FCRT, "Can't create text file to be queued.");
				  /* open it now, to reserve the name   */

    (void) strcpy (mq_aunique, mq_munique);
    (void) strcpy (mq_tunique, mq_munique);

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "tname = \"%s\"", mq_tfname);
#endif
}

/* initialize adddress queue          */
mq_adinit (dowarn, msgflgs, retadr)
    int dowarn;                  /* permit warning message?            */
    int msgflgs;                 /* misc. bits                         */
    char *retadr;                /* return address (NULL or char pointer */
{
    time_t clockdate;            /* variable for clock/date  */
    register Chan **chanptr;
    int fd;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_adinit ()");
#endif

    for (chanptr = ch_tbsrch; *chanptr != 0; chanptr++)
	(*chanptr) -> ch_access &= ~MQ_INQ;
				  /* reset "used" bit, for channel      */

    if ((fd = creat(mq_tfname, 0666)) < 0 ||
	(mq_tffp = fdopen (fd, "w")) == NULL)
	err_abrt (RP_FCRT, "Can't create address file to be queued.");

    time(&clockdate);             /* store current time, for queue sort */

/* Creation, Delivery status, Flags(retcite, for now), return addr */

    if (retadr && *retadr)	/* Paranoid */
	fprintf(mq_tffp, "%ld%c%d\n%s\n",
		(long)clockdate, ((dowarn) ? ADR_MAIL : ADR_DONE),
		msgflgs, retadr);
    else
	fprintf(mq_tffp, "%ld%c%d\n%s (Orphanage)\n",
		(long)clockdate, ((dowarn) ? ADR_MAIL : ADR_DONE),
		msgflgs, supportaddr);
}

mq_adwrite (achan, ahost, ambox)  /* put an address in the queue */
    register Chan *achan;
    register char *ahost;
    char    *ambox;
{
    char linebuf[LINESIZE];
    static char tmpchr[2] = { ADR_CLR };
    static char typchr[2] = { ADR_MAIL };
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "%s %8s/%s", achan -> ch_queue, ahost, ambox);
#endif

/* this format conforms to the structure specified in adr_queue.h       */

    arg2lstr (0, LINESIZE, linebuf, tmpchr, typchr, achan -> ch_queue,
	ahost, ambox, (char *)0);
    fprintf (mq_tffp, "%s\n", linebuf);
    achan -> ch_access |= MQ_INQ; /* message goes into this sub-queue   */
}

mq_txinit ()                      /* get ready to write message text    */
{                                 /* text file already opened           */
}
/**/

mq_eomsg ()                       /* done submitting message            */
{
    Chan **chanptr;
    char subname[LINESIZE];

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_eomsg ()");
#endif

    fflush (mq_mffp);
    if (ferror (mq_mffp))
	err_abrt (RP_FIO, "Problem writing %s", mq_mfname);

    if (fclose (mq_mffp) == EOF)
	err_abrt (RP_FIO, "Problem writing %s", mq_mfname);
	
    mq_mffp = (FILE *) NULL;    /* mark it as unusable */

    fflush (mq_tffp);
    if (ferror (mq_tffp))
	err_abrt (RP_FIO, "Problem writing %s", mq_tfname);
    if (fclose (mq_tffp) == EOF)
	err_abrt (RP_FIO, "Problem writing %s", mq_tfname);
    mq_tffp = (FILE *) NULL;    /* mark it as unusable */

    for (chanptr = ch_tbsrch; *chanptr != 0; chanptr++)
	if ((*chanptr) -> ch_access & MQ_INQ)
	{
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR,
			"linking to sub-queue '%s'", (*chanptr) -> ch_queue);
#endif
	    sprintf (subname, "%s%s/%s",
				squepref, (*chanptr) -> ch_queue, mq_munique);
	    if (link (mq_tfname, subname) == NOTOK)
	    {                         /* add it to the subqueue         */
		sprintf (subname, "%s%s",
				squepref, (*chanptr) -> ch_queue);
		err_abrt (RP_FCRT, "sub-directory '%s' missing", subname);
	    }
	}

#ifdef DEBUG
    ll_log (logptr, LLOGFTR,
	    "linking to '%s', unlinking '%s'", mq_afname, mq_tfname);
#endif

    if (link (mq_tfname, mq_afname) == NOTOK)
	err_abrt (RP_FCRT, "unable to move file to mail queue");
    unlink (mq_tfname);           /* IGNORE ANY ERROR HERE              */
}
/**/

mq_clear ()                       /* error termination of queue's files */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "mq_clear ()");
#endif

    if (mq_tffp != (FILE *) EOF && mq_tffp != (FILE *) NULL)
    {                             /* address file                       */
	fclose (mq_tffp);
	mq_tffp = (FILE *) NULL;
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "unlinking '%s'", mq_tfname);
#endif
	unlink (mq_tfname);
    }

    if (mq_mffp != (FILE *) EOF && mq_mffp != (FILE *) NULL)
    {                             /* message text file                  */
	fclose (mq_mffp);
	mq_mffp = (FILE *) NULL;
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "unlinking '%s'", mq_mfname);
#endif
	unlink (mq_mfname);
    }
}
