/*	@(#)v6mail.c	1.1	91/11/21 11:46:05 */
#include "util.h"                 /* to get mmdf reply codes            */
#include "mmdf.h"                 /* to get mmdf reply codes            */
#include "cnvtdate.h"
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>

/*  Upgrade to use MMDF Submit; 10 Nov. 1978; David H. Crocker
 *    1 Dec 78 Dave Crocker:  add mail-reading, approx like old "mail"
 *   15 Dec 78 Dave Crocker:  add sndmsg & memo & "-" interfaces
 *   16 Dec 78 Dave Crocker:  "-" not generate return address
 *   25 Apr 80 Dave Crocker:  fix .mail acquisition, default to save
 *   14 Jun 80 Dave Crocker:  turn tty writing off/on
 *   17 Jul 80 Dave Crocker:  fix -c
 *   24 Sep 80 Dave Crocker:  add -r
 *   12 Oct 80 Dave Crocker:  add -g & signature file
 *   15 Nov 80 Dave Crocker:  conversion to V7 package & use of mm_io
 *    1 Dec 80 Dave Crocker:  check success of ALL system calls
 *   20 Jul 81 Dave Crocker:  fix dobody() to detect end from terminal
 *   16 Jul 82 Dave Crocker:  make addresses have '@' only; no " at "
 *    6 Dec 83 Doug Kingston: remove tty write stuff, this is not the place.
 *   19 May 83 Doug Kingston: fix -r to pass a flag to submit
 */

long int    curnewpos,
	    poshack;              /* keep track of current position     */
				  /*   because ftell isn't on V6        */
FILE *  newfp;                    /* handle on .mail file               */

char   *logdir;
char   *username;
char   *mbxname;                  /* full pathname to mailbox           */
char    *binbox;
char    ttyobuf[BUFSIZ];          /* for buffering ttyoutput            */
char	*index();
char    *strdup();
extern char *whoami();
char    *sbmtargs = (char *) 0;
char    *subject = (char *) 0;
char    *replyto = (char *) 0;
char    *sender = (char *) 0;
char    *from = (char *) 0;
char    tolist[LINESIZE];
char    cclist[LINESIZE];

/* mmdf globals */

extern int sentprotect;           /* default mailbox protection         */
extern char     *mldflfil;        /* default mailbox file               */
extern char     *mldfldir;        /* directory containing mailbox file  */
extern char     *delim1;          /* string delimiting messages         */
extern char     *locname;
extern char     *locdomain;

/* end of mmdf globals */


char    noret;                    /* no return address                  */
char    watchit;                  /* user wants to watch delivery       */
char    idflag;                   /* insert a Message ID header line    */
char    qrychar;                  /* user-specified default save status */

char    usertype;
#define U_MAIL  0                 /* the original mail command          */
#define U_SND   1                 /* sndmsg-type prompting              */
#define U_MEMO  2                 /* memo prompting                     */
#define U_LMEM  3                 /* logged in as memo, for memo        */

#define FRMNONE 0                 /* no From field specified            */
#define FRMTXT  1                 /* From field had text only           */
#define FRMSNDR 2                 /* field had sender info, too         */

char    addrtype;
#define ADDR_TO 0                 /* add addresses to the to list       */
#define ADDR_CC 1                 /* add addresses to the cc list       */

/* **************************  MAIN  ******************************* */

main (argc, argv)
int     argc;
char   *argv[];
{
    setbuf (stdout, ttyobuf);
    mmdf_init (argv[0]);
    pgminit ();

    if (initstr ("snd", argv[0], 3)
	    || strindex ("/snd", argv[0]) != -1
	    || initstr ("snd", &argv[0][1], 3)
	    || strindex ("sndmsg", argv[0]) != -1)
	usertype = U_SND;
    else
	if (strindex ("memo", argv[0]) != -1)
	    usertype = U_MEMO;
	else
	    if (argv[0][0] == '-' && argv[0][1] == '\0')
	    {                     /* "-" => login shell   */
		usertype = U_LMEM;
		signal (SIGHUP, SIG_IGN);
				  /* renable these for exiting  */
		prompt ("Send a memo\n");
	    }
    if (usertype == U_MAIL && (argc == 1 || (argc == 2 && *argv[1] == '-')))
	readmail (argc, argv);    /* only when invoked at "mail"  */
    else
	sendmail (argc, argv);
}
/**/

leave (val)
int     val;
{
    if (newfp)
	lk_fclose(newfp, mbxname, (char *)0, (char *)0);
    exit (val == OK ? 0 : 99);
}

pipsig ()
{
    if (rp_gval (endchild (NOTOK)) == RP_NO)
	prompt ("Problem accessing mail submission program\n");
				  /* only comment if it looks like      */
				  /*  submit died ugly                  */
    leave (NOTOK);
}

pgminit ()
{
    extern struct passwd *getpwuid ();
    extern char *getmailid ();
    struct passwd  *pwdptr;
    int     realid,
	    effecid;

    signal (SIGPIPE, pipsig);    /* catch write to bad pipe            */

    getwho (&realid, &effecid);   /* who am i?                          */

    if ((pwdptr = getpwuid (realid)) == (struct passwd *) NULL)
	err_abrt (RP_PARM, "Unable to locate user's name");

    /*  save login directory   */
    logdir = strdup(pwdptr -> pw_dir);

/*    if ((username = getmailid(pwdptr -> pw_name)) == NULL)
	err_abrt (RP_PARM, "Unable to locate user's mailid");*/
    username = whoami();
}
/**/

readmail (argc, argv)
int     argc;
char   *argv[];
{
    register int    count;

    if (argc == 2)
	qrychar = argv[1][1];

    readinit ();
    for (count = 0; msgstrt () && showmsg () && savmsg (); count++);
    if (count == 0)
	err_abrt (RP_OK, "No new mail");
    endread ();
}

readinit ()
{
    extern char *multcat ();
    int     newfd;
    extern int  errno;

    mbxname = multcat (
		((mldfldir == 0 || isnull(*mldfldir)) ? logdir : mldfldir),
		 "/", username,"/",
		((mldflfil == 0 || isnull(*mldflfil)) ? username : mldflfil),
		  0);
    binbox = multcat (
		((mldfldir == 0 || isnull(*mldfldir)) ? logdir : mldfldir),
		 "/",username,"/._",
		((mldflfil == 0 || isnull(*mldflfil)) ? username : mldflfil),
		  0);
    if (access(binbox, 0) == 0) {
	fprintf(stderr, "Your mailbox has a binary box (%s),\n", binbox);
	fprintf(stderr, "probably created by another mail reading program like MSG.\n");
	fprintf(stderr, "You should remove the binary box before running this program\n");
	fprintf(stderr, "again, or use MSG instead to read your mail.\n");
	exit (99);
    }

    if ((newfd = lk_open (mbxname, 0, (char *)0, (char *)0, 5)) == -1)
	err_abrt (RP_OK,
		    (errno == EBUSY) ? "Mailbox is busy" : "No new mail");

    newfp = fdopen (newfd, "r");
    poshack = 0l;
}

endread ()
{
    fflush (stdout);

    if (creat (mbxname, sentprotect) == NOTOK)
				  /* truncate the file            */
	err_abrt (RP_FCRT, "Unable to reset %s mailbox", mbxname);
    leave (OK);
}
/**/

msgstrt ()
{
    char    linebuf[LINESIZE];

    FOREVER
    {
	curnewpos = poshack;
	if (fgets (linebuf, sizeof linebuf, newfp) == NULL)
	    return (FALSE);
	if (!strequ (linebuf, delim1))
	    break;
	poshack += strlen (linebuf);
    }
    fseek (newfp, curnewpos, 0);
    return (TRUE);
}

showmsg ()
{
    char    linebuf[LINESIZE];
    register int    lines;

    for (lines = 0; fgets (linebuf, sizeof linebuf - 1, newfp) != NULL;)
    {
	poshack += strlen (linebuf);
	if (strequ (linebuf, delim1))
	    return (TRUE);

	fputs (linebuf, stdout);
	if (lines++ > 4)
	{
	    fflush (stdout);
	    lines = 0;
	}
    }
    return ((ferror (newfp) || ferror (stdout)) ? FALSE : TRUE);
}
/**/

savmsg ()
{
    static  FILE * mboxfp;
    char    linebuf[LINESIZE];

    if (!querysav ())
	return (TRUE);

    if (mboxfp == NULL)
    {
	if ((mboxfp = fopen ("mbox", "a")) == NULL ||
		chmod ("mbox", sentprotect) == NOTOK)
	    err_abrt (RP_FOPN, "Unable to access/create mbox");
    }

    poshack = curnewpos;
    for ( fseek (newfp, curnewpos, 0);
	    fgets (linebuf, sizeof linebuf - 1, newfp) != NULL;
	    fputs (linebuf, mboxfp))
    {
	poshack += strlen (linebuf);
	if (strequ (linebuf, delim1))
	    break;
    }
    fputs (delim1, mboxfp);
    fflush (mboxfp);
    return (ferror (newfp) ? FALSE : TRUE);
}

querysav ()
{
    register int  respchar;

    if (qrychar == 0)
    {
	prompt ("\nSave? ");
	switch (respchar = getchar ())
	{
	    case '\n':
		break;
	    case '\0':
		exit (-1);
	    default:
		while (getchar () != '\n');
	}
    }
    else
	respchar = qrychar;
    if (respchar == 'n' || respchar == 'N')
	return (0);               /* deletion must be explicit            */
    return (1);                   /* default to saving                    */
}
/* *************************  SENDING ****************************** */

static char regargs[] = "rmxto,cc*";
  /* quick NS timeout, return to sender, mail, extract addrs from to & cc  */

sendmail (argc, argv)             /* Send a message                     */
int     argc;
char   *argv[];
{
    extern char *sbargs ();
    int     retval;
    char    linebuf[ADDRSIZE];
    char    hadfrom,
	    hadto,
	    hadcc;

    parseargs(argc, argv);

    if (rp_isbad (mm_init ()) || rp_isbad (mm_sbinit ()))
	err_abrt (RP_MECH, "Unable to submit mail; please report this error");

    strcpy (linebuf, regargs);    /* standard stuff          */
    if (isstr(sbmtargs))
        strcat (linebuf, sbmtargs);   /* extra args for submit   */

    if (usertype == U_LMEM)
	strcat (linebuf, "u");    /* do not verify the from field       */
    if (noret)
	strcat (linebuf, "q");    /* quiet, do not return on errors  */
    if (watchit)
	strcat (linebuf, "w");    /* user wants to watch the process     */

    if (rp_isbad (mm_winit ((char *) 0, linebuf, (char *) 0)))
	err_abrt (RP_MECH, "problem with submit message initialization");

    dodate ();

    hadfrom = dofrom ();

    dosubject ();

    if (hadfrom == FRMTXT && usertype != U_LMEM)
	dosender ("Sender:  ", (char *) 0, TRUE);
				/* simple address */
    hadto = doto ();
    hadcc = docc ();

    if (!hadto && !hadcc && usertype == U_MAIL)
	err_abrt (RP_PARM, "No addressees specified");

    doreply ();

    doid ();

    if (usertype != U_MAIL)
    {
	doprompt ();              /* get headers          */
	prompt ("Text:\n");
    }
    mm_wtxt ("\n", 1);           /* start the body             */


    dobody ();
    retval = endbody ();

    endchild (OK);

    if (usertype != U_MAIL && rp_isgood (retval))
	prompt ("Message posted\n");

    leave ((rp_isbad (retval)) ? NOTOK : OK);
}
/*  *************  ARGUMENT PARSING FOR SENDMAIL  **************  */

parseargs (argc, argv)               
int     argc;
char   *argv[];
{
    register int    i;

    tolist[0] = cclist[0] = '\0';
    addrtype = ADDR_TO;

    for (i = 1; i < argc; i++)
    {
	switch (argv[i][0])
	{
	default:
	    if (addrtype == ADDR_TO) {
		if (!strlen(tolist))
		    strcpy (tolist, argv[i]);
		else {
		    strcat (tolist, ", ");
		    strcat (tolist, argv[i]);
		}
	    } else { /* ADDR_CC */
		if (!strlen(cclist))
		    strcpy (cclist, argv[i]);
		else {
		    strcat (cclist, ", ");
		    strcat (cclist, argv[i]);
		}
	    }
	    break;
	case '-':
	    switch (argv[i][1])
	    {
		case '-':
		    sbmtargs = &(argv[i][2]);
		    break;

		case 'r':
		    noret = TRUE;
		    break;

		case 'w':
		    watchit = TRUE;
		    break;

		case 'i':
		    idflag = TRUE;
		    break;

		case 's':
		    subject = argv[++i];
		    break;

		case 'a':
		    replyto = argv[++i];
		    break;

		case 'f':
		    if ((++i < argc) && !isstr(sender))
     		         from = argv[i];
		    break;

		case 'g':
		    if ((++i < argc) && !isstr(from))
		        sender = argv[i];
		    break;

		case 't':
		    addrtype = ADDR_TO;
		    break;

		case 'c':
		    addrtype = ADDR_CC;    
		    break;
	    }
	    break;
	}
    }
}

sndhdr (name, contents)
char	*name,
	*contents;
{
    char    linebuf[LINESIZE];

    sprintf (linebuf, "%-10s%s\n", name, contents);
    mm_wtxt (linebuf, strlen (linebuf));
}
/**/

dodate ()
{
    char    datbuf[64];

    cnvtdate (TIMREG, datbuf);    /* rfc733 format date                 */
    sndhdr ("Date:  ", datbuf);
}

doid ()
{
    char    linebuf[LINESIZE];
    char    datbuf[64];

    if (idflag)
	{
	    cnvtdate (TIMCOM, datbuf);
	    sprintf (linebuf, "<%s.%d@%s.%s>", datbuf, getpid(),
			locname, locdomain);
	    sndhdr ("Message-Id:  ", linebuf);
	}
}

dosubject ()
{
    if (isstr(subject))
	sndhdr ("Subject:  ", subject);
}

doreply ()
{
    if (isstr(replyto))
	sndhdr ("Reply-To: ", replyto);
}
/**/

dofrom ()
{
    static char fldnam[] = "From:  ";
    register int    i;

    if (isstr(sender))
	{
	    dosender (fldnam, sender, TRUE);
	    return (FRMSNDR);
	}
    else if (isstr(from))
	{
	    sndhdr (fldnam, from);
	    return (FRMTXT);
	}
    else if (usertype != U_LMEM)       /* not memo                     */
	dosender (fldnam, (char *) 0, TRUE);

    return (FRMNONE);
}
/**/

dosender (cmpnt, name, fancy)
char    cmpnt[],
	name[];
int     fancy;          /* make address fancy? */
{
    FILE   *sigfp;
    char   *ptr,
	    linebuf[ADDRSIZE],
	    sigtxt[FILNSIZE];     /* where is signature text?           */
    char    gotsig;

    gotsig = FALSE;

    if (!fancy || name != 0)
    {
	strcpy (sigtxt, name);
	gotsig = TRUE;
    }
    else
    {                             /* user didn't give us a signature    */
	sprintf (linebuf, "%s/%s/.fullname", mldfldir,username);

	if ((sigfp = fopen (linebuf, "r")) != NULL)
	{
	    if (fgets (sigtxt, sizeof sigtxt, sigfp) != NULL) {
		char *cp;

		if (cp = index(sigtxt, '\n'))
		    *cp = '\0';
	    	if (sigtxt[0])
		    gotsig = TRUE;
	    }
	}
    }

    if (gotsig)                   /* real name + mailbox                */
	sprintf (linebuf, "%s <%s@%s.%s>", sigtxt, username,
		locname, locdomain);
    else                          /* just the mailbox info              */
	sprintf (linebuf, "%s@%s.%s", username, locname, locdomain);

    sndhdr (cmpnt, linebuf);
}
/**/

doto ()
{

    if (strlen(tolist) > 0) {
	sndhdr ("To:  ", tolist);
	return (TRUE);
    } else
        return (FALSE);

}
/**/

docc ()
{
    if (strlen(cclist) > 0) {
	sndhdr ("cc:  ", cclist);
	return (TRUE);
    } else
	return (FALSE);
}
/**/

doprompt ()
{
    if (usertype == U_LMEM)       /* "free" memo          */
	cpyprompt ("From:  ");
    cpyprompt ("To:  ");
    if (usertype == U_SND)        /* sndmsg               */
	cpyprompt ("cc:  ");
    cpyprompt ("Subject:  ");
}

prompt (str)
char   *str;
{
    printf ("%s", str);
    fflush (stdout);
}

cpyprompt (prom)
char    prom[];
{
    prompt (prom);
    copylin (prom);
}

copylin (prelim)
char   *prelim;
{
    char    linebuf[LINESIZE];
    register int    morin;
    register int    c;

    for (morin = TRUE; morin; sndhdr (prelim, linebuf), prelim = "")
    {
	morin = FALSE;
	if (fgets (linebuf, sizeof linebuf, stdin) == NULL)
	    err_abrt (RP_LIO, "Input error");

	if (linebuf[0] == '\n')
	    return;

	c = strlen (linebuf);
	linebuf[c - 1] = '\0';
	if (linebuf[c - 2] == '\\')
	{
	    morin = TRUE;
	    linebuf[c-- - 2] = '\n';
	}
    }
}
/**/

dobody ()
{
    char    buffer[BUFSIZ];
    register int    i;

    while (!feof (stdin) && !ferror (stdin) &&
	    (i = fread (buffer, sizeof (char), sizeof (buffer), stdin)) > 0)
	if (rp_isbad (i = mm_wtxt (buffer, i)))
		err_abrt (i, "Problem writing body");

    if (ferror (stdin))
	err_abrt (RP_FIO, "Problem reading body");
}

endbody ()
{
    struct rp_bufstruct thereply;
    int     len;

    if (rp_isbad (mm_wtend ()))
	err_abrt (RP_MECH, "problem ending submission");

    if (rp_isbad (mm_rrply (&thereply, &len)))
	err_abrt (RP_MECH, "problem getting submission status");

    if (rp_isbad (thereply.rp_val))
	err_abrt (thereply.rp_val, "%s", thereply.rp_line);

    return (thereply.rp_val);
}

doreset ()
{
    exit (NOTOK);
}

/* *********************  UTILITIES  ***************************  */

endchild (type)
int     type;
{
    int retval;


    if (rp_isgood (retval = mm_sbend ()))
	retval = mm_end (type);

    return ((retval == NOTOK) ? RP_FIO : (retval >> 8));
}

/*VARARGS2*/
err_abrt (code, fmt, b, c, d)     /* terminate the process              */
int     code;                     /* a mmdfrply.h termination code      */
char   *fmt, *b, *c, *d;
{
    if (fmt) {
	printf (fmt, b, c, d);
    	putchar ('\n');
    }
    if (code != RP_OK)
	printf ("Message posting aborted\n");
    fflush (stdout);
    endchild (NOTOK);
    leave (code);
}

