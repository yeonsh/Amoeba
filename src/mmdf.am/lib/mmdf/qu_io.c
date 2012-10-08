/*	@(#)qu_io.c	1.1	91/11/21 09:53:27 */
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
/*  QU_IO:  Channel interaction with Deliver                            */

/*  Sep, 80 Dave Crocker    fix qu_minit, to allow no return address
 *  Nov, 80 Dave Crocker    convert to V7
 *  Mar, 81 John Pickens    add seek arg to qu_rsinit, add qu_fileno
 *  Dec, 82 Doug Kingston   added definition of hd_init() as a long.
 *  Mar, 84 Dennis Rockwell updated for 4.2 (s_io)
 */

#include <sys/stat.h>
#include "adr_queue.h"
#include "ch.h"
#include "phs.h"
#include "ap.h"

/*#define RUNALON */

extern LLog *logptr;
extern long hd_init();
extern char *strdup();
extern char *index();
extern char *mquedir;
extern int ap_outtype;          /* 733 or 822 */
extern FILE *hd_fmtfp;          /* handle on hd_format massaged header */

char	*qu_msgfile;            /* PUBLIC INFO: path to text file     */

long	qu_msglen;              /* byte count of message */

LOCVAR int  qu_nadrs;           /* number of addressees */

/*  some versions of stdio do not handle simultaneous reads and writes, so
 *  we must have two handles on it.  we can play this game only because the
 *  access characteristics are fairly constrained.
 */

LOCVAR FILE *qu_rfp,              /* address list read handle */
	    *qu_wfp;              /* address list write handle */

LOCVAR Chan *qu_chptr;            /* channel we are running as          */

LOCVAR long   qu_seek;            /* beginning of unmassaged body       */

LOCVAR int    qu_txfd;            /* msg text file file descriptor    */

LOCVAR char   qu_hdr;             /* take from massaged header          */

LOCVAR struct rp_construct
	    rp_nxerr =
{
    RP_FIO, 'U', 'n', 'a', 'b', 'l', 'e', ' ', 't', 'o', ' ', 'o', 'p',
    'e', 'n', ' ', 'n', 'e', 'x', 't', ' ', 'm', 'e', 's', 's', 'a', 'g',
    'e', ' ', 'f', 'i', 'l', 'e', '\0'
}          ,
	    rp_rrcerr =
{
    RP_LIO, 'q', 'u', '_', 'r', 'd', 'r', 'e', 'c', ' ', 'p', 'i', 'p',
    'e', ' ', 'e', 'r', 'r', 'o', 'r', '\0'
}          ,
	    rp_rsterr =
{
    RP_FIO, 'q', 'u', '_', 'r', 'd', 's', 't', 'm', ' ', 'f', 'i', 'l',
    'e', ' ', 'e', 'r', 'r', 'o', 'r', '\0'
}          ,
	    rp_tsterr =
{
    RP_NS, 'N', 'a', 'm', 'e', 's', 'e', 'r', 'v', 'e', 'r', ' ', 'T',
    'i', 'm', 'e', 'o', 'u', 't', '\0'
};
/* ****************  (qu_)  DELIVER I/O SUB_MODULE  ***************** */

qu_init (argc, argv)              /* get ready to process Deliver mail  */
int       argc;                     /* NOTE: other modules have no args   */
char   *argv[];
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_init");
#endif

    if ((qu_chptr = ch_nm2struct (argv[0])) == (Chan *) NOTOK)
    {
	ll_log (logptr, LLOGTMP, "chan '%s' unknown", argv[0]);
	qu_chptr = (Chan *) 0;
    }

#ifdef RUNALON
    qu_rfp = fdopen (0, "r");
    qu_wfp = fdopen (1, "w");

    qu_msgfile = strdup ("tmp");
    qu_txfd = open ("tmp", 0);
    qu_sender = strdup ("dcrocker");
    domsg = TRUE;
#else /* RUNALON */
    if (argc < 3)
    {
	ll_log (logptr, LLOGTMP, "Channel invoked with too few arguments (%d)",
				 argc);
	return (RP_NO);
    }

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "'%s' ('%s','%s')", argv[0], argv[1], argv[2]);
#endif

    /* fdopen input fd to get addresses from
     * and output fd to return responses
     */
    if ( !isdigit (*argv[1]) ||
	 (qu_rfp = fdopen (atoi (argv[1]), "r")) == NULL ||
	 !isdigit (*argv[2]) ||
	 (qu_wfp = fdopen (atoi (argv[2]), "w")) == NULL )
    {
	ll_log (logptr, LLOGTMP, "Invalid arguments given to channel");
	return (RP_NO);
    }

    if( argv[3] && index(argv[3], 'w') )
	domsg = TRUE;
#endif /* RUNALON */
    return (RP_OK);
}

qu_end (type)                     /* done performing Deliver function   */
int     type;                     /* ignore type of termination         */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_end");
#endif

#ifdef RUNALON
    return (RP_OK);
#endif

    if (qu_wfp != (FILE *) EOF && qu_wfp != NULL)
	fclose (qu_wfp);
    qu_rfp =
	qu_wfp = (FILE *) EOF;

    return (RP_OK);
}

qu_pkinit ()                      /* get ready to receive mail          */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_pkinit");
#endif

    return (RP_OK);
}

qu_pkend ()                       /* done receiving mail                */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_pkend");
#endif

    return (RP_OK);
}
/**/

qu_minit (msgfile, dohdr)         /* set up for next message            */
char   msgfile[];                 /* name of message to be processed    */
int     dohdr;                    /* perform address massaging          */
{
    struct stat statbuf;          /* for getting the message size       */
    char    linebuf[LINESIZE];

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_minit (%s, dohdr=%d)", msgfile, dohdr);
#endif

    sprintf (linebuf, "%s%s", mquedir, msgfile);
    qu_msgfile = strdup (linebuf);

    if (qu_txfd > 0)
	(void) close (qu_txfd);          /* reset file descriptor */

    if ((qu_txfd = open (qu_msgfile, 0)) == NOTOK)
    {
	ll_err (logptr, LLOGFAT, "Unable to open message file '%s'.",
			qu_msgfile);
	qu_wrply ((RP_Buf *) &rp_nxerr, rp_conlen (rp_nxerr));
	return (RP_FIO);
    }

    qu_seek = 0L;

    fstat (qu_txfd, &statbuf);
    qu_msglen = st_gsize (&statbuf); /* size of basic message              */
    qu_nadrs = 0;
    qu_hdr = FALSE;
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "basic msglen = %ld", qu_msglen);
#endif

    if (dohdr != AP_SAME)          /* fix up the addresses               */
    {
	ap_outtype = dohdr;
	qu_seek = hd_init (qu_chptr, qu_fileno ());
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "qu_seek = %ld", qu_seek);
#endif
	if(qu_seek == (long) MAYBE){
	    qu_wrply ((RP_Buf *) &rp_tsterr, rp_conlen (rp_tsterr));
	    return (RP_NS);
	}
	if (qu_seek != 0L) {
	    qu_hdr = TRUE;

	    /*  fix message length count, by subtracting size of original
	     *  header and adding size of new one.
	     */
	    qu_msglen -= qu_seek;
	    fstat (fileno (hd_fmtfp), &statbuf);
	    qu_msglen += st_gsize (&statbuf);
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "modified msglen = %ld, qu_seek = %ld",
					qu_msglen, qu_seek);
#endif
	}
    }

    return (RP_OK);
}

qu_mend ()                        /* end of current message             */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_mend");
#endif

    if (qu_seek > 0L)
	qu_hdr = FALSE;

    hd_end ();                /* get rid of massaged data           */
    if (qu_txfd >= 0)
	(void) close (qu_txfd);          /* free unneeded fd's      */
    qu_txfd = NOTOK;
    if (qu_msgfile != NULL)
	free (qu_msgfile);
    qu_msgfile = NULL;
    return (RP_OK);
}
/*  ************  TELL DELIVER OF RESULT ****************** */

qu_wrply (valstr, len)           /* pass a reply to local process      */
RP_Buf   *valstr;    /* structure containing reply         */
int     len;                      /* length of the structure            */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_wrply()");
    ll_log (logptr, LLOGFTR, "(%s)'%s'",
	    rp_valstr (valstr -> rp_val), valstr -> rp_line);
#endif

    switch (valstr -> rp_val)
    {                             /* do msg stats for typical channels */
        case RP_DOK:
	case RP_AOK:
	    qu_nadrs++;
	    break;

	case RP_MOK:
	    if (qu_nadrs == 0)    /* addresses weren't batched  */
		qu_nadrs++;       /*  => one addr/msg           */
	    phs_msg (qu_chptr, qu_nadrs, qu_msglen);
	    qu_nadrs = 0;         /* reset */
	    break;
    }
    return (qu_wrec ((char    *) valstr, len));
}
/*                    BASIC I/O WITH DELIVER                          */

qu_rrec (linebuf, len)           /* read a record from Deliver         */
char   *linebuf;                  /* where to stuff the address         */
int      *len;                      /* where to stuff address length      */
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_rrec ()");
#endif

#ifdef RUNALON
    qu_wrec ("qu_rrec: ", 10);
#endif
    switch (*len = gcread (qu_rfp, linebuf, LINESIZE - 1, "\000\n\377"))
    {
	case NOTOK:
	    ll_err (logptr, LLOGFAT, rp_rrcerr.rp_cline);
	    qu_wrply ((RP_Buf *) &rp_rrcerr,
			rp_conlen (rp_rrcerr));
	    return (RP_LIO);

	case OK:                  /* closed the pipe                    */
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "qu_rrec (EOF)");
#endif
	    return (RP_EOF);

	case 1:
	    if (isnull (linebuf[0]) || linebuf[0] == '\n')
	    {
#ifdef DEBUG
		ll_log (logptr, LLOGBTR, "DONE");
#endif
		return (RP_DONE); /* the only valid one-char records    */
	    }
	    ll_err (logptr, LLOGFAT, "1-char record ('%c')", linebuf[0]);
	    return (RP_PARM);     /* bad parameter                        */
    }
    *len -= 1;
    linebuf[*len] = '\0';         /*   so it can be lloged as a string  */
#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "(%d)\"%s\"", *len, linebuf);
#endif
    return (RP_OK);
}
/**/

qu_rsinit (theseek)             /* initialize stream (text) position  */
long theseek;
{
    extern long lseek ();

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_rsinit (%ld)", theseek);
#endif

    if ((theseek == 0L) && (qu_seek > 0L))
    {
	lseek (qu_txfd, qu_seek, 0);
	hd_minit ();
	qu_hdr = TRUE;
    }
    else
	lseek (qu_txfd, theseek, 0);
}

qu_fileno ()                      /* return fd for message text file    */
{
    return (qu_txfd);
}

qu_rstm (buffer, len)            /* read next part of message stream   */
char   *buffer;                   /* where to stuff next part of text   */
int    *len;                      /* where to stuff length of part      */
{
    int want = *len - 1;
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_rstm ()");
#endif

    if (qu_hdr) {                 /* take from massaged header          */
	switch (*len = hd_read (buffer, want))
	{
	    case NOTOK:
		qu_hdr = FALSE;
		goto fioerr;

	    case OK:
#ifdef DEBUG
		ll_log (logptr, LLOGBTR, "qu_rstm (hdr DONE)");
#endif
		qu_hdr = FALSE;   /* start taking from regular file     */
		break;            /* drop down to file read             */

	    default:
		buffer[*len] = '\0';
#ifdef DEBUG
		ll_log (logptr, LLOGFTR, "\"%s\"", buffer);
#endif
		return (RP_OK);
	}
    }

    switch (*len = read (qu_txfd, buffer, want))
    {                             /* raw read of the file               */
	case NOTOK:
    fioerr:
	    ll_err (logptr, LLOGFAT, rp_rsterr.rp_cline);
	    qu_wrply ((RP_Buf *) &rp_rsterr, rp_conlen (rp_rsterr));
	    return (RP_FIO);

	case 0:                  /* end of message; not treated as EOF */
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "qu_rstm (DONE)");
#endif
	    return (RP_DONE);

	case 1:
	    if (isnull (buffer[0]))
		return (RP_DONE);
    	    /* Drop Through */

	default:
	    buffer[*len] = '\0';  /*   so it can be ll_loged as a string  */
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "\"%s\"", buffer);
#endif
	    return (RP_OK);
    }
    /* NOTREACHED */
}
/**/

qu_wrec (str, len)               /* write a record to Deliver          */
char    *str;
int     len;
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "qu_wrec() (%d)\"%s\"", len, str ? str : "");
#endif

    if (str != 0)
	fnput (str, len, qu_wfp);
    (void) putc ('\0', qu_wfp);       /* null is record terminator     */
    if (fflush (qu_wfp) == NOTOK)
	return (RP_LIO);

    return (RP_OK);
}
