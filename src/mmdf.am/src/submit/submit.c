/*	@(#)submit.c	1.4	96/02/27 09:58:45 */
#include "util.h"
#include "mmdf.h"
#include "amoeba.h"
#include "module/proc.h"
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
/*  SUBMIT:  Mail enqueuer
 *
 *  May, 80 Dave Crocker    fix switch-setting in prm_ & pro_
 *                          add eof check after text copy
 *                          adr_ghost return PAST initial string ->
 *                            modify use of pointer to be -1
 *  Jun, 80 Dave Crocker    fix adr_ghost return to be 1 past
 *                          mn_init, change fout to be 2, not dup(1)
 *                          hdr_psfrom handle FLGFROM properly
 *  Jul, 80 Dave Crocker    prm_parse, remove debug & fix default
 *  Sep, 80 Jim Lieb        Conversion to V7 and standard io
 *                            protocol replies go to stderr
 *  Oct, 80 Dave Crocker    Minor mods to make V6 also (again) work
 *  Nov, 80 Dave Crocker    Split off adr_submit, qf_submit, mgt_submit,
 *                            & hdr_parse, for easier policy changes
 *  Nov, 81 Dave Crocker    Switches case-insensitive
 *  Jun 82  Dave Crocker    Add adruid proc/file address protection
 *  Jul 82  Dave Crocker    qf_ -> mq_
 */

#include <sys/stat.h>
#include <pwd.h>
#include "nexec.h"
#include "ap.h"
#include "ns.h"

extern LLog msglog;
LLog *logptr = &msglog;
extern struct ap_prevstruct   *ap_fle; /* parse state top of stack       */

extern char *locfullname;
extern char *locfullmachine;
extern char *quedfldir;            /* directory w/mail queue directories */
extern char *cmddfldir;            /* directory w/mmdf commands          */
extern char *logdfldir;            /* directory w/mmdf log files	 */
extern char *supportaddr;
extern char *sitesignature;

jmp_buf retloc;                   /* address for longjmp()              */
int     userid,                   /* id of user running me              */
	effecid,                  /* id providing access priviledges    */
	adruid;                   /* authorized addr, for file access   */

char    *username;                /* string login name of user running me */
char    *mailid;                  /* string mailid of user running me     */

short   earlyret,                 /* caller already got return status   */
	readadrs,                 /* will be directly fed address list  */
	tracing;		  /* enable submission tracing on fd2   */

extern char *mgt_parm();
extern char *dupfpath();
extern char *strdup();
extern char *multcat();
extern char *getmailid();
extern char *malloc();
extern char *strdup();
extern struct passwd *getpwuid();
extern ap_flget();

extern char *namdeliver;      /* name of mailer process             */
extern char *pathdeliver;     /* file path to mailer proc.          */
extern int  *regfdary;
extern int  errno;
extern int  sys_nerr;
extern char *sys_errlist[];

char *prm_dupval();

LOCFUN prm_parse(), hdr_isacmpt();

/* *** constants  and temporaries *** */
char	nltrm[] = "\n\377",		/* for gcread() */
	nlnultrm[] = "\n\000\377",
	nultrm[] = "\000\377";

/* *** pro_ *** */
short   pro_doing = TRUE,         /* protocol-mode interaction vs. one- */
				  /*    message "batch" submission?     */
	pro_vfadr;                /* in protocol mode, indicate         */
				  /*    acceptance of each address      */
char    pro_buf[BUFSIZ];	  /* buffer pro_ output                 */

/**/
/* *** mq_ *** */

extern FILE *mq_mffp;             /* pointerto message text file buffer */
extern char *mq_munique;          /* unique part of message queue name  */

/* *** ch_ *** */
#include "ch.h"
extern Chan **ch_tbsrch;

/* *** adr_ *** */
#include "adr_queue.h"
#include "msg.h"
extern char *adr_fulldmn;       /* SEK - name for 'full' domain name    */
extern char *adr_fmc;           /* name of 'full' machine               */

/* *** tx_ *** */
long tx_msize;

/* *** hdr_ *** */
#include "hdr.h"
#define MAXACMPT    10

char   *hdr_cmpt[MAXACMPT];       /* where to extract addresses from    */

short     hdr_numadrcmpt;         /* number of components to search     */
				  /*    for addresses in msg text       */
short   hdr_extractadr;           /* tx_stormsg extract addrs?          */

/* *** mgt_ *** */
extern char   *mgt_txsrc;         /* text to add to Source-Info cmpnt   */
extern int    mgt_inalias;        /* SEK - is address extracted from    */
				  /* alias file                         */

/* *** auth_ *** */
extern char auth_msg[];         /* message of reason for auth refusal   */

/* *** dlv_ *** */
short   dlv_watch;                /* user want to watch the send?       */
int	dlv_flgs;                 /* flags to store into first line     */
				  /*  of address list (e.g. RETCITE)    */

/* *** ut_ *** */
char   *ut_alloc ();
short   ut_eofhit,                /* eof encountered on ut_stdin        */
	ut_msgend;                /* null/eof encountered on ut_stdin   */

/*******  (mn_)  MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN  ****** */

LOCVAR	char *mn_aptr;          /* current position on buffer   */
LOCVAR	char mn_abuf[LINESIZE]; /* address buffer               */

LOCFUN
	mn_adrin ()            /* main   input given to alst_proc    */
{
    if (mn_aptr == (char *)1) {
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "mn_adrin need newline");
#endif
	    if (ut_stdin(mn_abuf, sizeof(mn_abuf), nltrm, NOTOK) <= 0
	       || strequ ("!\n", mn_abuf))
		mn_aptr = (char *) 0;   /* disable future reading     */
	    else
		mn_aptr = mn_abuf;
    }
    if (mn_aptr == (char *)0) {
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "mn_adrin eof or err");
#endif
	    return (0);                 /* permanent eof                */
    }

    switch (*mn_aptr) {
	case '\n':
	case '\0':                      /* end on bang/newline/bang */
	    mn_aptr = (char *)1;      /* indicate we need another line */
	    return (0);                 /* note end of this address     */
    }

    return (*mn_aptr++);
}

/*
 *  Function to re-sync input.  This is needed for the interactive
 *  input function because the other side is expecting exactly one
 *  reply per line sent.  alst_proc might parse two addresses in one line.
 */
mn_arset()
{
	mn_aptr = (char *)1;
}

mn_aread ()
{
    mn_aptr = (char *)1;      /* flag as needing a new line */
    mgt_inalias = FALSE;      /* reading addresses from input          */

    (void) alst_proc (mn_adrin, TRUE, mn_arset, (char **) 0);

    mn_aptr = (char *) 0;
}

/**/

main (argc, argv)                 /*   MAIN MAIN MAIN MAIN MAIN MAIN    */
int       argc;
char   *argv[];
{
    umask(0);
    mmdf_init (argv[0]);

    adr_fulldmn = locfullname;
    if (locfullmachine && isstr (locfullmachine))
        adr_fmc = locfullmachine;
    else
        adr_fmc = locfullname;

#ifdef NAMESERVER
    ns_settimeo(NS_UIPTIME);	/* set an initial timeout */
#endif
    mn_usinit ();                 /* initial info on who is running us  */
    mn_init (argc, argv);         /* parse args, alloc buffers.         */
    mn_dirinit ();                /* chdir into quedfldir                 */
    mgt_init ();
    mq_winit ();                  /* info on queue directories          */

    for (setjmp (retloc);       /* return here, after err_msg         */
	    !ut_eofhit;)          /* no more input?                     */
    {
	if (pro_doing)
	    if (pro_init () != OK)
		break;            /* all done                           */

	mq_creat ();              /* set-up the queue files including   */
				  /*    status info in address list     */

	if (readadrs)            /* explicit address list being given? */
	    mn_aread ();

	if (ut_msgend)            /* can't do much with only addr list  */
	    err_abrt (RP_EOF, "Input ended prior to processing text");

	tx_stormsg ();            /* queue & authenticate message       */

	if (pro_doing && ut_eofhit)
	    err_abrt (RP_EOF, "Premature eof during text copy");

	mgt_aend ();             /* perform any ending address policy  */

	lnk_filall (dlv_flgs);
				  /* put address list into its file     */

	lnk_freeall ();

	mq_eomsg ();              /* move to real queue directory       */

#ifdef DEBUG
	ll_log (logptr, LLOGPTR, "msg done -- do deliver?");
#endif

	dlv_invoke ();            /* Maybe give the message a mailer    */
				  /* Will acknowledge message queueing */
    }
    exit (RP_OK);
}
/**/

mn_init (argc, argv)              /* basic process initialization       */
int       argc;
char   *argv[];
{
    register short     tmp;

    prm_init ();                  /* set default values                 */
    for (tmp = 1; tmp < argc; tmp++)
    {                             /* parse any arguments                */
	if (*argv[tmp] == '-')
	{
	    pro_doing = FALSE;    /* presence of arguments => one-shot  */
	    prm_parse (&argv[tmp][1]);
	}                         /* parse the switch string            */
	else
	    mgt_parm (argv[tmp]); /* let management module analyze it   */
    }

    if (!pro_doing)               /* protocol mode will do its own      */
	prm_end ();               /* cleanup & evaluate state           */
}
/**/

mn_usinit ()                      /* get info on who is running me      */
{
    register char *midp;
    register struct passwd *pw;

    getwho (&userid, &effecid);   /* who is running me?                 */
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "user=%d, effec=%d", userid, effecid);
#endif

    if((pw = getpwuid(userid)) == NULL
      || (midp = getmailid(pw->pw_name)) == NULL)
	err_abrt (RP_LIO, "No login/mailid for UID %d, call Support.",
		userid);

    username = strdup (pw -> pw_name);
    mailid = strdup (midp);

    if (effecid == userid)        /* being run by MMDF                  */
	adruid = 0;               /* full privileges                    */
    else
	adruid = userid;          /* uid for file/process redirection   */
#ifdef  UCL
    if (adruid == 2)            /* fix for authorization for UCL        */
	adruid = 0;
#endif
}

mn_dirinit ()                     /* current loc? chdir to home; setuid */
{
    if (chdir (quedfldir) == NOTOK) /* change wdir into mail queues       */
	err_abrt (RP_LIO, "Unable to change directory.");

    setuid (userid);              /* so we have the user's privileges   */
				  /*   to access address files          */
}
/*******************  (prm_) USER PARAMETER HANDLING  *************** */

prm_init ()                       /* set default user-settable vals     */
{

    mgt_minit ();                /* initialize management settings     */

    while (hdr_numadrcmpt-- > 0)   /* free any previous component names  */
	free (hdr_cmpt[hdr_numadrcmpt]);

    hdr_numadrcmpt =
	dlv_flgs = 0;

    readadrs =
	hdr_extractadr =
	pro_vfadr =
	dlv_watch =
	earlyret =  FALSE;
}

prm_end ()                        /* evaluate final settings            */
{
    if (!pro_doing)               /* no protocol => a unix "command"    */
	domsg = TRUE;

    if (!hdr_extractadr)           /* not building list from msg text,   */
	readadrs = TRUE;         /*    so take from explicit list      */

    mgt_pend ();                  /* end of parm specifications         */
}
/**/

LOCFUN
	prm_parse (parmstrt)      /* parse spec & set switches          */
    char *parmstrt;               /* note beginning of parm             */
{
    register char  *parmptr;      /* pointer to text of specification   */

    parmptr = parmstrt;
    FOREVER
	switch (*parmptr++)       /* read until end of list             */
    {
	case '\0':
	case '\n':
	    return;               /* all done                           */
	case ' ':
	case '\t':                /* skip linear white space            */
	case '-':                 /* switch-char got through            */
	    continue;

	case 'L':           /* Specify alternate logfile, SU/MMDF only */
	    if (adruid == 0 && *parmptr) {
	    	char *cp;
                
                switch (*(parmptr = prm_dupval(parmptr, &cp)))
                {                     
                    case ',':     /* skip to the last char processed    */
                        err_msg (RP_PARM, "Invalid parameter character ','");
                
                    case '*':
                        parmptr++;
                }
		ll_log (logptr, LLOGFST, "Logfile is '%s'", cp);
		logptr->ll_file = dupfpath(cp, logdfldir);
	    }
	    break;

	case 'V':               /* Specify alternate level, SU/MMDF only */
	    if (adruid == 0 && *parmptr) {
	    	char *cp;
		int level;
                
                switch (*(parmptr = prm_dupval(parmptr, &cp)))
                {                     
                    case ',':     /* skip to the last char processed    */
                        err_msg (RP_PARM, "Invalid parameter character ','");
                
                    case '*':
                        parmptr++;
                }
		ll_log (logptr, LLOGFST, "Loglevel '%s'", cp);
		if ((level = tai_llev (1, &cp)) > 0)
		    logptr->ll_level = level;
	    }
    	    break;

    	case 'W':
    	    tracing = TRUE;
    	    break;

	case 'b':                 /* deliver to mailbox AND tty (obsolete) */
	    break;

	case 'c':                 /* only CITATION in returm mail, not  */
	    dlv_flgs |= ADR_CITE;
	    break;                /*   full copy of text                */

	case 'd':                 /* don't force through DELAY channel */
	    mgt_parm ("d");
	    break;

	case 'f':                 /* add FROM text to Source-Info field */
	    switch (*(parmptr = prm_dupval (parmptr, &mgt_txsrc)))
	    {
		case ',':
		case '*':
		    parmptr++;
	    }
	    break;

	case 'h':                 /* hostname of relay source           */
	    switch (*(parmptr = mgt_parm (&parmptr[-1])))
	    {                     /* let management module process it   */
		case ',':         /* skip to the last char processed    */
		    err_msg (RP_PARM, "Invalid parameter character ','");

		case '*':
		    parmptr++;
	    }
	    break;

	case 'i':                 /* RELAYING, "via" us, "INDIRECTLY"   */
	    switch (*(parmptr = mgt_parm (&parmptr[-1])))
	    {                     /* let management module process it   */
		case ',':         /* skip to the last char processed    */
		    err_msg (RP_PARM, "Invalid parameter character ','");

		case '*':
		    parmptr++;
	    }
	    break;

	case 'j':                 /* we are the delay channel !!        */
	    mgt_parm ("j");
	    break;

	case 'k':                 /* Set nameserver timeouts */
	    switch (*(parmptr = mgt_parm (&parmptr[-1])))
	    {                     /* let management module process it   */
		case ',':         /* skip to the last char processed    */
		    err_msg (RP_PARM, "Invalid parameter character ','");

		case '*':
		    parmptr++;
	    }
	    break;

	case 'l':                 /* send LOCAL mail immediately        */
	    mgt_parm ("l");       /* fork a mailer for it               */
	    break;

	case 'm':                 /* send just to MAILBOX (obsolete option) */
	    break;

	case 'n':                 /* send "NET" mail immediately        */
	    mgt_parm ("n");       /* fork a mailer for it               */
	    break;

	case 'q':                 /* do not return a copy on errors     */
	    dlv_flgs |= ADR_NORET;
	    break;

	case 'r':                 /* send RETURN mail to invoker        */
	    mgt_parm ("r");       /*    and not explicit specification  */
	    break;

	case 's':                 /* get RETURN from Sender info        */
	    mgt_parm ("s");       /*    and not explicit specification  */
	    break;

	case 't':                 /* TRUST me; don't authenticate       */
	    mgt_parm ("t");
	    break;

	case 'u':                 /* (close to 't'); no authentication  */
	    mgt_parm ("u");
	    break;

	case 'v':                 /* Protocol mode; VERIFY each address */
	    pro_vfadr = TRUE;     /*    as received                     */
	    break;

	case 'w':                 /* user wants to WATCH a delivery try */
	    dlv_watch = TRUE;
	    break;

	case 'g':                 /* GET cmpts addrs AND explicit ones  */
	    readadrs = TRUE;     /* DROP ON TRHOUGH                    */
	case 'x':                 /* EXTRACT addrs from cmpnts          */
	    hdr_extractadr = TRUE; /* 'x' [name *(',' name)] '*'         */
	    while (hdr_numadrcmpt < MAXACMPT)
		switch (*(parmptr = prm_dupval (parmptr,
				&(hdr_cmpt[hdr_numadrcmpt++]))))
		{
		    case ',':
			parmptr++;
			continue;

		    case '*':
		    case '\n':
			parmptr++;

		    case '\0':
			goto cmptend;
		}
    cmptend:
	    hdr_cmpt[hdr_numadrcmpt] = 0;
	    break;

	case 'z':                 /* do not send non-delivery warnings  */
	    dlv_flgs |= ADR_NOWARN;
	    break;

	default:
	    err_msg (RP_PARM, "Invalid parameter character '%c' in '%s'",
			parmptr[-1], parmstrt);
    }
}
/**/

char *
	prm_dupval (start, into)  /* get text; dup to into; return ptr  */
				  /*     to last char in orig str       */
char   *start;                    /* beginning of text to parse         */
char  **into;                     /* where to put dup'd str             */
{
    register char   tchar;
    register char  *strptr;

    for (strptr = start;; strptr++)
	switch (*strptr)
	{
	    case '*':
	    case ',':
	    case '\0':
	    case '\n':
		tchar = *strptr;
		*strptr = '\0';
		*into = strdup (start);
		*strptr = tchar;
		return (strptr);  /* pass back pointer to last char     */
	}
}
/**************  (pro_)  PROTOCOL WITH CALLER  **************** */

pro_init ()                       /* read switches in protocol mode     */
{
    char linebuf[LINESIZE];

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "pro_init ()");
#endif

    (void) setbuf(stderr,pro_buf);

    ut_msgend = FALSE;

    if (ut_stdin (linebuf, sizeof linebuf, nlnultrm, NOTOK) == OK)
	return (NOTOK);          /* done before we began.  tsk. tsk.   */

    prm_init ();
    prm_parse (linebuf);         /* parse & use switches               */
    prm_end ();
    return (OK);
}
/**/

/* VARARGS2 */
pro_reply (code, fmt, b, c, d) /* inform user of a status            */
short     code;                     /* value from mmdfrply.h              */
char   *fmt,
       *b,
       *c,
       *d;
{
    register char  *errchar;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "pro_reply(%s)", rp_valstr (code));
    ll_log (logptr, LLOGFTR, fmt, b, c, d);
#endif

    if (pro_doing)                /* print each field, for exactness    */
	putc(rp_gval (code), stderr);
    else
    {                             /* assume human watching              */
	if (rp_isgood (code))  /* keep silent during success         */
	    return;
	fprintf(stderr, "submit: ");      /* and noisy on errors                */
    }
    if (rp_isbad (code))
	switch (rp_gval (code))
	{
	    case RP_HUH:          /* not if it was a user error         */
	    case RP_PARM:
	    case RP_USER:
		break;

	    default:
		if (errno > 0 && errno < sys_nerr)
				  /* a system-call error; have to dance */
		{                 /* around ending \n in sys_errlist    */
		    fprintf (stderr, "[ ");
		    for (errchar = sys_errlist[errno];
			    !isnull (*errchar) && *errchar != '\n';
			    errchar++)
			putc(*errchar, stderr);
		    fprintf (stderr, " ] ");
		}
	}
    fprintf(stderr, fmt, b, c, d);
    putc('\n', stderr);
    fflush(stderr);
}
/*****************  (alst_)  PROCESS AN ADDRESS LIST  *************** */

alst_proc (inproc, errabrt, resync, badptr)  /* process text stream of addrs */
int     (*inproc) ();           /* the function which gets address     */
				/*    chars; decides when finished    */
int     errabrt;                /* abort on error?                    */
int     (*resync) ();           /* the function that resyncs input      */
char    **badptr;               /* place to stuff list of bad addresses */
{
    AP_ptr  local,             /* beginning of local-part */
	    domain,            /* basic domain reference */
	    route;             /* beginning of 733 forward routing */
    AP_ptr  theptr;
    int     retval;
    int     anybad;
    char    *msgptr;
    char    *addrp;
    char    *cp;
    int     inalias;
    char    *oldbad;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "alst_proc ()");
#endif
    anybad = RP_AOK;
    inalias = mgt_inalias;      /* SEK hold value at this level */

    FOREVER
    {
	if ((theptr = ap_pinit (inproc)) == (AP_ptr) NOTOK)
	    err_abrt (LLOGTMP, "problem initializing for address parse");
			      /* problem during parse               */

	switch( retval = ap_1adr ())
	{                   /* get next address                         */
	    case NOTOK:
		ap_sqdelete (theptr, (AP_ptr) 0);
		ap_free (theptr);
		if (!errabrt)           /* Keep on trucking...     */
		    anybad = RP_USER;
		else if (pro_vfadr)    /* just tell caller & continue  */
		    err_gen (RP_USER, "Unable to parse address");
		else              /* kiss off the whole message         */
		    err_msg (RP_USER, "Unable to parse address");
		if (resync != 0)
			(*resync)();
		continue;       /*  Go get another address...  */

	    case DONE:
#ifdef DEBUG
		ll_log( logptr, LLOGFTR, "alst_proc() done");
#endif
		ap_sqdelete (theptr, (AP_ptr) 0);
		ap_free (theptr);
		return (anybad);
	}
	mgt_inalias  = inalias;         /* take iniial value */

	if(ap_t2s (theptr, &addrp) != (AP_ptr)MAYBE) {
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "canonical addr: '%s'", addrp);
#endif
    	    if (tracing) {
    		printf("%s:  ", addrp);
		fflush(stdout);
	    }
	    ap_t2parts (theptr, (AP_ptr *) 0, (AP_ptr*) 0,
				    &local, &domain, &route);
	    if (theptr -> ap_obtype == APV_DTYP &&
		lexequ (theptr -> ap_obvalue, "include"))
	    {
		Chan    *lrval;
		/*
		 *  Handle :include:/foo/bar@host, :include:file, <file, etc.
		 */
		if (domain && (domain -> ap_obvalue)
		  && (lrval = ch_h2chan (domain -> ap_obvalue, 1))
						    != (Chan *) OK)
		{
		    if(lrval == (Chan *)MAYBE)
			retval = RP_NS;
		    else {
			cp = multcat (":Include:", local -> ap_obvalue, (char *)0);
			free (local  -> ap_obvalue);
			local -> ap_obvalue = cp;
			retval = adr_check (local, domain, route);
		    }
		}
		else
		    retval = alst_gfil (local -> ap_obvalue);
	    }
	    else                      /* regular address */
		retval = adr_check (local, domain, route);

	    if(rp_gval(retval) == RP_NS)  /* can we use the delay channel */
		retval = adr_dsubmit(addrp);
	}
    	else
    	    retval = RP_NS;

	if (errabrt)              /* we care about this address         */
	{
	    if (rp_isbad (retval))
	    {                     /* error                              */
		switch (rp_gval (retval))
		{
		    case RP_NS:
			if (tracing) {
			    printf("Nameserver Timeout\n");
			    fflush(stdout);
			}
			msgptr = "(%s) Nameserver Timeout in \"%s\"";
			if (pro_vfadr)    /* just tell caller & continue */
			    err_gen(RP_NS, msgptr, rp_valstr(retval), addrp);
			else              /* kiss off the whole message */
			    err_msg(RP_NS, msgptr, rp_valstr(retval), addrp);
			goto bugout1;

		    case RP_USER:
			msgptr = "(%s) Unknown user name in \"%s\"";
			break;

		    case RP_BHST:
			msgptr = "(%s) Unknown host/domain name in \"%s\"";
			break;

		    case RP_FOPN:
			msgptr = "(%s) Unable to open address file \"%s\"";
			break;

		    case RP_NAUTH:
			msgptr = "(%s) Bad authorization: %s";
			if (pro_vfadr)    /* just tell caller & continue        */
			    err_gen (RP_USER, msgptr, rp_valstr (retval), auth_msg);
			else              /* kiss off the whole message         */
			    err_msg (RP_USER, msgptr, rp_valstr (retval), auth_msg);
			goto bugout1;

		    default:
			msgptr = "(%s) Cannot process address \"%s\"";
		}
		if (tracing) {
		    printf("Bad address\n");
		    fflush(stdout);
		}
		if (pro_vfadr)    /* just tell caller & continue        */
		    err_gen (RP_USER, msgptr, rp_valstr (retval),
						      addrp, auth_msg);
		else              /* kiss off the whole message         */
		    err_msg (RP_USER, msgptr, rp_valstr (retval),
						      addrp, auth_msg);
	    }
	    else if (pro_vfadr) {
		/* it's ok and we want to know that, too   */
		if (pro_doing) {
		    if(rp_gval(retval) == RP_DOK)
			pro_reply (RP_DOK, "Possibly a nice address \"%s\"",
								     addrp);
		    else
			pro_reply (RP_AOK, "Nice address \"%s\"", addrp);

		}
		else
		    printf ("%s: OK\n", addrp);
	    }
	}
	else if (rp_isbad (retval)) {
	    switch (retval)
	    {
		case RP_NAUTH:
		case RP_NS:
		    anybad = retval;
		    break;

		default:
		    anybad = RP_USER;
		    break;
	    }

	    if (badptr) {
	        if (*badptr == (char *)0)
	            *badptr = multcat(addrp, "\n", (char *) 0);
	        else {
	            oldbad = *badptr;
	            *badptr = multcat(oldbad, addrp, "\n", (char *) 0);
	            free (oldbad);
	        }
	    }
	}

bugout1:
	ap_sqdelete (theptr, (AP_ptr) 0);
	ap_free (theptr);
	free (addrp);
	if (resync != 0)
		(*resync)();
    }
    /* NOTREACHED */
}
/*******************  ADDRESS FILE INPUT  *************************** */

alst_gfil (file)                 /* read file address list             */
    char    file[];               /* basic name of file                 */
{
    struct stat statbuf;
    int oadruid;                 /* push old adruid onto stack */
    static char themsg[] = "Can't get addresses from file '%s'";
    char *badlist = (char *) 0;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "alst_gfil (%s)", file);
#endif
    if (tracing) {
	printf("Including file %s\n", file);
	fflush(stdout);
    }
    oadruid = adruid;

    if (ap_fpush (file) != OK)
    {                           /* could not open the file */
	ll_err (logptr, LLOGGEN, themsg, file);
	return (RP_FOPN);
    }

    fstat (fileno (ap_fle -> ap_curfp), &statbuf);

    if (effecid == statbuf.st_uid)
	adruid = 0;             /* full privileges                    */
    else                        /* uid for file/process redirection   */
	adruid = statbuf.st_uid;

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "adruid for file = %d", adruid);
#endif

    mgt_inalias = FALSE;        /* getting addresses from file          */

    if (rp_isbad (alst_proc (ap_flget, FALSE, (int (*)())0 , &badlist))) {
	struct  passwd *pw;
	char    *cp;
	char    linebuf[LINESIZE];

	ll_log (logptr, LLOGTMP, "Bad address in list-file %s", file);
	if (tracing) {
	    printf ("Bad address in file %s\n", file);
	    fflush(stdout);
	}
	sprintf (linebuf, "%s %s", locfullname, sitesignature);
	ml_init (NO, NO, linebuf, "Bad address in file");
	ml_adr (supportaddr);
	if( (pw = getpwuid (statbuf.st_uid))
	 && (cp = getmailid (pw->pw_name)) )
		ml_adr (cp);
	ml_aend();
	ml_tinit();
	sprintf (linebuf, "Found bad address in the file '%s'.\n\n", file);
	ml_txt (linebuf);
	if (badlist) {
	    sprintf (linebuf, "There were problems with:\n");
	    ml_txt (linebuf);
	    ml_txt (badlist);
	    free (badlist);
	    sprintf (linebuf, 
	"\nThe remaining addresses in the file were used for submission.\n\n");
	    ml_txt (linebuf);
	}
	if (ml_end(OK) != OK)
	    ll_log (logptr, LLOGFAT, "Can't send to supportaddr");
    }
    ap_fpop ();

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "done w/file");
#endif
    adruid = oadruid;
    return (RP_AOK);
}
/****************  (tx_)  TRANSFER MESSAGE TEXT  ******************** */

tx_stormsg ()                     /* store message text into queue      */
{                                 /* perform required processing        */
    struct stat statbuf;
    short len,
	retval;
    char linebuf[LINESIZE],       /* input line                         */
	 thename[LINESIZE];       /* name of the component              */
    register char   lastchar;     /* to make sure message end with \n   */

#ifdef DEBUG
    ll_log (logptr, LLOGPTR, "tx_stormsg ()");
#endif

    mgt_hinit ();

    for (mq_txinit (), lastchar = '\n';
	 (len = ut_stdin (linebuf, sizeof linebuf, nlnultrm, NOTOK)) != OK &&
	 (retval = hdr_proc (linebuf, thename)) > HDR_EOH;
	 fputs (linebuf, mq_mffp))
	    lastchar = linebuf[len - 1];
				  /* process headers & copy to file     */

    if (lastchar != '\n')         /* msgs must end with newline         */
	putc ('\n', mq_mffp);

    mgt_hend ();		  /* whatever policies apply overall    */
				  /* includes Source-Info & Via         */

    putc ('\n', mq_mffp);         /* put out the blank line             */

    if (retval == HDR_NAM)        /* put out illegal first body line    */
	fputs (linebuf, mq_mffp); /*  if any                            */

    if (!ut_msgend)               /* nothing to copy                    */
    {
    	char buf[BUFSIZ];	  /* Lets be efficient now		*/

	for (lastchar = '\n';     /* copy the body                      */
	  (len = ut_stdin (buf, sizeof buf, nultrm, OK)) > 0;
	  fwrite (buf, sizeof(char), len, mq_mffp))
	    lastchar = buf[len - 1];

	if (lastchar != '\n')     /* msgs must end with newline         */
	    putc ('\n', mq_mffp);
    }
    fflush (mq_mffp);
    fstat (fileno (mq_mffp), &statbuf);
    tx_msize = st_gsize (&statbuf);
}
/*******************  (hdr_)  PROCESS A HEADER  ********************* */

LOCVAR char *hdr_atxt;             /* hdr_in() passes to alst()      */

LOCFUN
	hdr_in ()               /* adrs extracted from component text */
{
    if (hdr_atxt == (char *) 0)  /* nothing to give it                 */
	return (EOF);

    switch (*hdr_atxt)
    {
	case '\0':
	case '\n':            /* end of string, DROP ON THROUGH     */
	    hdr_atxt = (char *) 0;
	    return (0);
    }
    return (*hdr_atxt++);
}
/**/

hdr_proc (theline, name)          /* process one header line            */
    char theline[],               /* the text                           */
	 *name;                   /* old or new name of header          */
{
    char contents[LINESIZE];      /* where to put header contents       */
    int  thestate;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "hdr_proc (%s)", theline);
#endif

    switch (thestate = hdr_parse (theline, name, contents))
    {
	case HDR_NAM:
	    return (HDR_NAM);

	case HDR_EOH:
	    return (HDR_EOH);

	case HDR_NEW:
	case HDR_MOR:
	    break;
    }

    if (!mgt_hdr (name, contents, thestate))
	err_msg (RP_HUH, "Problem with component '%s'", name);

    if (hdr_extractadr && hdr_numadrcmpt > 0 && hdr_isacmpt (name))
    {                             /* add the addresses                  */
	hdr_atxt = contents;
	mgt_inalias = FALSE;      /* getting addresses from header      */
	(void) alst_proc (hdr_in, TRUE, (int (*)())0, (char **) 0);
    }

    return (HDR_OK);
}

LOCFUN
	hdr_isacmpt (name)        /* a component on extraction list?    */
register char   name[];           /* name of the test component         */
{
    register short    entry;

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "hdr_isacmpt(%s)", name);
#endif
    for (entry = 0; entry < hdr_numadrcmpt && hdr_cmpt[entry]; entry++)
	if (lexequ (name, hdr_cmpt[entry]))
	    return (TRUE);
    return (FALSE);
}
/******************  (dlv_)  INVOKE DELIVER PROCESS  *************** */

dlv_invoke ()                     /* maybe try immediate transmission   */
{
    Chan    **chanptr;
    char    temppath[LINESIZE];
    char dochans[LINESIZE];
    int     oldpid;               /* what channels to send              */
    int     proctyp,              /* type of process invocation         */
	    dowait;               /* parent status after child invoke   */
    char *argv[5];
    int i;
    capability process;


#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "dlv_invoke ()");
#endif

    strcpy (dochans, "-c");       /* switch, indicating chans to run    */

    for (proctyp = dowait = 0, chanptr = ch_tbsrch; *chanptr != 0; chanptr++)
	if ((*chanptr) -> ch_access & DLVRDID)
	{
#ifdef DEBUG
	    ll_log (logptr, LLOGFTR, "do: %s (%s)",
		(*chanptr) -> ch_show, (*chanptr) -> ch_name);
#endif
	    strcat (dochans, (*chanptr) -> ch_name);
	    strcat (dochans, ",");
	    proctyp = SPNEXEC;    /* default to detached send           */
	}
    dochans[strlen (dochans) - 1] = '\0';	/* zap last comma */

    if (proctyp == 0)             /* nothing to send immediately        */
    {
	pro_reply (RP_MOK, "Submitted & queued (%s)", mq_munique);
	earlyret = TRUE;          /* note that we already ok'd          */
	return;
    }
    if (dlv_watch)                /* watch immediate transmissions?     */
    {
	proctyp = FRKEXEC;        /* do it via attached child           */
	dowait = FRKWAIT | FRKPIGO;
				  /* parent not interruptable til end   */
	regfdary[1] = 1;
    }
    else
    {
	pro_reply (RP_MOK, "Submitted & immediates started (%s)", mq_munique);
	earlyret = TRUE;          /* note that we already ok'd          */
/*	regfdary[1] = open ("/dev/null", 1);*/
				  /* no output to caller                */
#ifdef DEBUG
	ll_log (logptr, LLOGFTR, "null opened");
#endif
    }

    getfpath (pathdeliver, cmddfldir, temppath);

#ifdef DEBUG
    ll_log (logptr, LLOGBTR,
		"%s (%s, %s)", temppath, mq_munique, dochans);
#endif
    ll_close (logptr);        /* free it for deliver                */
#ifdef OLDSELF
    oldpid = getpid ();

    if (nexecl (proctyp, dowait|FRKCIGO|FRKERRR , regfdary,
	    temppath, namdeliver, mq_munique, "-d",
	    dochans, (dlv_watch) ? "-w" : (char *)0, (char *)0) == NOTOK)
	if (getpid () != oldpid)      /* problem with the child             */
	{
	    ll_err (logptr, LLOGTMP,
			"Unable to run %s '%s'", namdeliver, temppath);
	    exit (RP_MECH);
	}
#endif

    argv[0] = strdup(temppath);
    argv[1] = strdup(mq_munique);
    argv[2] = strdup("-d");
    argv[3] = strdup(dochans);
    argv[4] = (char *)0;

    if ( !argv[0] || !argv[1] || !argv[2] || !argv[3] ) {
	err_abrt (RP_HUH, "Memory allocation failure");
    }

/*    newproc(argv[0],argv,(char **)0,-1,(int *)0,-1L);*/
    exec_file((capability *)0,(capability *)0,(capability *)0,0,argv,(char **)0,(struct caplist *)0,&process);

/*    if (regfdary[1] != 1)          it was opened on /dev/null         
	close (regfdary[1]);*/
    if (dlv_watch)            /* if not already noted               */
	pro_reply (RP_MOK, "Done mailing (%s)", mq_munique);


}
/*****************  (err_)  GENERAL ERROR-HANDLING  ***************** */

/* VARARGS2 */
err_abrt (code, fmt, b, c, d)  /* terminate the process              */
short     code;                     /* a mmdfrply.h termination code      */
char   *fmt,
       *b,
       *c,
       *d;
{
    mq_clear();                 /* Just to be sure. */
    if (setjmp (retloc) == 0)   /* hack; force err_msg return to here */
	err_msg (code, fmt, b, c, d);
    exit (code);                  /* pass the reply code to caller      */
}


/* VARARGS2 */
err_msg (code, fmt, b, c, d)   /* end processing for current message */
short     code;                     /* see err_abrt explanation           */
char   *fmt,
       *b,
       *c,
       *d;
{
#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "err_msg ()");
#endif
    err_gen (code, fmt, b, c, d); /* includes sending reply             */
    if (!ut_eofhit)               /* get rid of rest of input stream    */
	ut_suckup ();
    mq_clear ();                  /* eliminate temporary files          */
    lnk_freeall();                /* wipe out the current address list  */
    if (pro_doing)                /* more messages ok in protocl mode   */
	longjmp (retloc, TRUE);
    else
	printx ("submit: message submission aborted\n");
    exit (code);                  /* otherwise, goodbye                 */
}

/* VARARGS2 */
err_gen (code, fmt, b, c, d)   /* standard error processing          */
short     code;                /* see err_abrt, for explanation      */
char   *fmt,
       *b,
       *c,
       *d;
{
    char newfmt[LINESIZE];        /* for printf                         */

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "err_gen(%s)", rp_valstr (code));
#endif
    if (rp_isgood (code))         /* it wasn't an error                 */
	return;

    switch (rp_gval (code))       /* log the error?                     */
    {
	case RP_HUH:              /* not if it was a user error         */
	case RP_PARM:
	case RP_USER:
	    sprintf (newfmt, "%s %s", "xin", fmt);
	    ll_log (logptr, LLOGFST, newfmt, b, c, d);
	    break;

	default:
	    sprintf (newfmt, "%s  %s", "err [ ABEND (%s) ] ", fmt);
	    ll_err (logptr, LLOGFAT, newfmt, rp_valstr (code), b, c, d);
    }

    if (!earlyret)                /* haven't already sent reply         */
	pro_reply (code, fmt, b, c, d);
}
/*******************  (ut_)  UTILITY ROUTINES  ********************** */

ut_stdin (buffer, buflen, brkset, longok) /* read from primary input    */
register char  *buffer;           /* where to put null-ended input      */
int	buflen;			  /* length of buffer */
char    *brkset;                  /* what characters to end on          */
int     longok;                   /* long lines are allowed if == OK    */
{
    register int      len;        /* number of characters read          */

/*  brkset must always include null ('\000'), which is the end-of-message
 *  indicator.  if null is the terminator, len is decremented.  i.e.,
 *  null is not allowed to pass as an input character, tho its occurrence
 *  is noted by ut_msgend.
 */

#ifdef DEBUG
    ll_log (logptr, LLOGBTR, "ut_stdin (brk='%s')", brkset);
#endif

    if (ut_msgend || ut_eofhit)
    {
#ifdef DEBUG
	ll_log (logptr, LLOGBTR, "(eof terminated last read)");
#endif
	return (0);              /* signal eof, for this read          */
    }

    len = gcread (stdin, buffer, buflen - 1, brkset);
    switch (len)
    {
	case -1:
	    err_abrt (RP_LIO, "Error reading data from control stream");

	case 0:
#ifdef DEBUG
	    ll_log (logptr, LLOGBTR, "EOF");
#endif
	    ut_msgend =
		ut_eofhit = TRUE;
	    buffer[0] = '\0';
	    return(0);
    }
    if (longok != OK && len == (buflen - 1))
	err_abrt (RP_NO, "Input line to submit longer than %d characters",
			buflen - 2);

    if (isnull (buffer[len - 1])) {
	--len;            /* strip null off end                 */
	ut_msgend = TRUE; /* also note end of message         */
    } else
	buffer[len] = '\0';

#ifdef DEBUG
    ll_log (logptr, LLOGFTR, "returning: \"%s\"", buffer);
#endif
    return (len);
}
/**/

ut_suckup ()                      /* skip rest of message input stream  */
{                                 /*    to null (not eof)               */
    register int c;

    if (!ut_msgend && !ut_eofhit)
    {
	while ((c = getc (stdin)) != EOF)
	    if (isnull (c) && pro_doing)
	    {
		ut_msgend = TRUE;
		return;
	    }
	ut_eofhit = TRUE;
    }
}

char   *
	ut_alloc (numbytes)       /* allocate some memory               */
unsigned  numbytes;                 /* duh... number of bytes to allocate */
{
    register char  *thebytes;

    if ((thebytes = malloc (numbytes)) == NULL)
	err_abrt (RP_MECH, "No more storage available");
    return (thebytes);
}
