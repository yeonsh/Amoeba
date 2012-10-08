/*	@(#)resend.c	1.1	91/11/21 11:45:56 */
#include "util.h"                 /* to get mmdf reply codes            */
#include "mmdf.h"                 /* to get mmdf reply codes            */
#include "cnvtdate.h"
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>

extern char     *locname;
extern char     *locdomain;

char   *logdir;
char   *username;

extern	char *strdup();
extern  char *index();
extern	char *multcat();

char    noret;                    /* no return address                  */
char    watchit;                  /* user wants to watch delivery       */
int	hadto;

#define FRMNONE 0                 /* no From field specified            */
#define FRMTXT  1                 /* From field had text only           */
#define FRMSNDR 2                 /* field had sender info, too         */


/* **************************  MAIN  ******************************* */

main (argc, argv)
int     argc;
char   *argv[];
{
	mmdf_init (argv[0]);

	pgminit ();

	get_aliasfile ();

	sendmail (argc, argv);
}

pipsig ()
{
	if (rp_gval (endchild (NOTOK)) == RP_NO)
		err_abrt(RP_LIO, "Abnormal return from submit");
	exit (NOTOK);
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

	if ((username = getmailid(pwdptr -> pw_name)) == NULL)
		err_abrt (RP_PARM, "Unable to locate user's mailid");
}

#define	NARGS	20

get_aliasfile ()
{
	struct passwd  *pwdptr;
	char	rcfilename[128];
	char	linebuf[128];
	char	*rcname = ".sendrc";
	char 	*av[NARGS+1], *aliasfile[128];
	FILE 	*fp;
	int 	realid, effecid;

	getwho (&realid, &effecid);

	if ((pwdptr = getpwuid (realid)) == (struct passwd *) NULL)
		err_abrt (RP_PARM, "Unable to locate user's name");

	/* Get info from the ".sendrc" file */

	sprintf( rcfilename, "%s/%s", pwdptr->pw_dir, rcname );
	if(( fp = fopen(rcfilename, "r")) != NULL ) {
		char *cp;

		/* Process info a line at a time until we find the alias file */
		while( fgets(linebuf, sizeof(linebuf), fp ) != NULL )  {
			if (cp = index(linebuf, '\n'))
				*cp = 0;
			if (sstr2arg(linebuf, NARGS, av, " \t") < 0)
				continue;
			if ( strncmp(linebuf,"aliases",7) == 0 )  {

				if (av[1][0] != '/')  {
					sprintf( aliasfile, "%s/%s", pwdptr->pw_dir, av[1]);
					aliasinit ( aliasfile );
				} else
					aliasinit ( av[1] );

			}	/* end of if on strncmp */
		}	/* end of while on lines */
	
	}	/* end of if statement on file open */
}	/* end of get_aliasfile routine */


/*^L*/

static char regargs[] = "rmxresent-to,resent-cc*";
/* return to sender, mail, extract addrs from to & cc   */

sendmail (argc, argv)             /* Send a message                     */
int     argc;
char   *argv[];
{
	extern char *sbargs ();
	int     retval;
	char    linebuf[ADDRSIZE];
	char   *sbmtargs;

	if (rp_isbad (mm_init ()) || rp_isbad (mm_sbinit ()))
		err_abrt (RP_MECH, "Unable to submit mail; please report this error");

	strcpy (linebuf, regargs);    /* standard stuff                     */

	if ((sbmtargs = sbargs (argc, argv)) != 0)
		/* any args to be args to submit?       */
		strcat (linebuf, sbmtargs);

	if (noret)
		strcat (linebuf, "q");    /* quiet, do not return on errors  */
	if (watchit)
		strcat (linebuf, "w");    /* user want so watch the process     */

	if (rp_isbad (mm_winit ((char *) 0, linebuf, (char *) 0)))
		err_abrt (RP_MECH, "problem with submit message initialization");

	dumpheader();
	doresent(argc, argv);

	if (!hadto)
		err_abrt (RP_PARM, "No addressees specified");

	dobody ();
	retval = endbody ();

	endchild (OK);

	exit ((rp_isbad (retval)) ? NOTOK : OK);
}
/*  *************  ARGUMENT PARSING FOR SENDMAIL  **************  */

char *
sbargs (argc, argv)               /* any args to be args to submit?   */
int     argc;
char   *argv[];
{
	register int    i;
	char   *argptr;

	for (i = 1, argptr = 0; i < argc; i++)
	{                             /* create message header fields */
		if (argv[i][0] == '-')
			switch (argv[i][1])
			{
			case '-':
				argptr = &(argv[i][2]);
				continue;

			case 'r':
				noret = TRUE;
				break;

			case 'w':
				watchit = TRUE;
				break;
			}
	}
	return (argptr);
}

/**/

dumpheader()
{
	char	line[LINESIZE];

	while (fgets (line, LINESIZE, stdin) != NULL) {
		if (line[0] == '\n')
			break;
		if (prefix ("resent-", line))
			mm_wtxt ("Old-", 4);
		mm_wtxt (line, strlen (line));
	}
}

doresent(argc, argv)
int	argc;
char	**argv;
{
	char    datbuf[64];

	cnvtdate (TIMREG, datbuf);		/* rfc822 format date */
	sndhdr ("Resent-Date:  ", datbuf);
	dosender ();
	doto (argc, argv);
	mm_wtxt ("\n", 1);
}

sndhdr (name, contents)
char    name[],
contents[];
{
	char    linebuf[LINESIZE];

	sprintf (linebuf, "%-10s%s\n", name, contents);
	mm_wtxt (linebuf, strlen (linebuf));
}

dosender ()
{
	int     sigfd;
	int	sigsiz;
	char	linebuf[ADDRSIZE];
	char	sigtxt[FILNSIZE];     /* where is signature text?           */
	char    gotsig;

	gotsig = FALSE;

	sprintf (linebuf, "%s/.fullname", logdir);

	if ((sigfd = open (linebuf, 0)) >= 0)
	{                         /* there is a file w/signature?       */
		sigsiz = read (sigfd, sigtxt, sizeof sigtxt);
		if (sigsiz > 0) {
			sigtxt[sigsiz - 1] = '\0';
			gotsig = TRUE;
		}
	}

	if (gotsig)                   /* real name + mailbox                */
		sprintf (linebuf, "%s <%s@%s.%s>",
			 sigtxt, username, locname, locdomain);
	else                          /* just the mailbox info              */
		sprintf (linebuf, "%s@%s.%s", username, locname, locdomain);

	sndhdr ("Resent-From:  ", linebuf);
}
/**/

doto (argc, argv)
int     argc;
char   *argv[];
{
	char    linebuf[ADDRSIZE];
	char    someto;
	register int    i;
	register char *host = multcat(locname, ".", locdomain, (char *) 0);

	someto = FALSE;
	i = 1;
	while( i < argc )
	{
		linebuf[0] = '\0';
		if( argv[i][0] != '-' )
		{
			someto = TRUE;
			aliasmap( linebuf, argv[i], host );
			sndhdr( "Resent-To:  ", linebuf );
		}
		else switch( argv[i][1] )
		{
		case 't':
			someto = TRUE;
			i++;
			aliasmap( linebuf, argv[i], host );
			sndhdr( "Resent-To:  ", linebuf );
			break;
		case 'c':
			someto = TRUE;
			i++;
			aliasmap( linebuf, argv[i], host );
			sndhdr( "Resent-Cc:  ", linebuf );
			break;
		case 'w':
		case 'r':
		case '-':
			break;
		default:
			fprintf(stderr,"Unknown flag '%c'\n",argv[i][1]);
			break;
		}
		i++;
	}
	free(host);
		
	hadto = (someto ? TRUE : FALSE);
	return;
}

dolist (i, argc, argv, dest)
register int    i;
int     argc;
char   *argv[];
char   *dest;                     /* where to put the list                */
{
    *dest = '\0';                 /* in case nothing found                */
    if (i < argc && *argv[i] != '-')
	for (strcat (dest, argv[i++]); i < argc && *argv[i] != '-';
		strcat (dest, ", "), strcat (dest, argv[i++]));

    return (i - 1);
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

/* *********************  UTILITIES  ***************************  */

endchild (type)
int     type;
{
	int retval;


	if (rp_isgood (retval = mm_sbend ()))
		retval = mm_end (type);

	return ((retval == NOTOK) ? RP_FIO : (retval >> 8));
}

/* VARARGS2 */

err_abrt (code, fmt, b, c, d)     /* terminate the process              */
int     code;                     /* a mmdfrply.h termination code      */
char   *fmt, *b, *c, *d;
{
	if (fmt)
		printf (fmt, b, c, d);
	printf ("\nMessage resending aborted\n");
	fflush (stdout);
	endchild (NOTOK);
	exit (code);
}
