/*	@(#)sendmail.c	1.1	92/05/14 10:43:23 */
/*
**  A Sendmail fake.
*/

#include <signal.h>
#include <sys/types.h>
#include <pwd.h>
#include "util.h"
#include "mmdf.h"
#include "cnvtdate.h"

extern	char	*locname;
extern	char	*locdomain;
extern	char	*chndfldir;

extern struct passwd *getpwuid ();
extern char *getmailid ();
extern char *getenv();
extern char *dupfpath();
extern char *multcat();
extern int exit();

char	*SMTPSRVR = "smtpsrvr";

char	*FullName;	/* sender's full name */
char	*from;		/* sender's mail address */
char	subflags[128];	/* flags for submit */
int	watch;
int	verify;
int	extract;
int	badaddrs;
int	rewritefrom;

int	die();

/*ARGSUSED*/
main(argc, argv)
int argc;
char **argv;
{
	struct passwd  *pwdptr;
	register char *p;
	register char **av;
	struct rp_bufstruct thereply;
	int	retval;
	int     len;

	mmdf_init(argv[0]);

	if ((pwdptr = getpwuid (getuid())) == (struct passwd *) NULL)
		syserr("Unable to locate user's name");

	if ((from = getmailid(pwdptr -> pw_name)) == NULL)
		syserr("Unable to locate user's mailid");

	from = multcat(from, "@", locname, ".", locdomain, (char *)0);

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) signal(SIGINT, die);
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		(void) signal(SIGHUP, die);
	(void) signal(SIGTERM, die);
	(void) signal(SIGPIPE, die);

	FullName = getenv("NAME");

	av = argv;
	while ((p = *++av) != NULL && p[0] == '-') {
		switch (p[1]) {
		case 'b':	/* operations mode */
			switch (p[2]) {
			case 'a':	/* smtp on stdin */
			case 's':	/* smtp on stdin */
				smtp();
				exit(98);	/* should never happen */
			case 'm':	/* just send mail */
				continue;
			case 'v':	/* verify mode */
				verify++;
				continue;
			default:
				syserr("Invalid operation mode %c", p[2]);
			}
			continue;

		case 'f':	/* from address */
		case 'r':	/* obsolete -f flag */
			p += 2;
			if (*p == '\0' && ((p = *++av) == NULL || *p == '-'))
			{
				p = *++av;
				if (p == NULL || *p == '-') {
					syserr("No \"from\" person");
					av--;
					continue;
				}
			}
			if (rewritefrom) {
				syserr("More than one \"from\" person");
				continue;
			}
			from = p;
			rewritefrom++;
			continue;

		case 'F':	/* set full name */
			p += 2;
			if (*p == '\0' && ((p = *++av) == NULL || *p == '-'))
			{
				syserr("Bad -F flag");
				av--;
				continue;
			}
			FullName = p;
			continue;

		case 'h':	/* hop count */
			p += 2;
			if (*p == '\0' && ((p = *++av) == NULL || !isdigit(*p)))
			{
				syserr("Bad hop count (%s)", p);
				av--;
				continue;
			}
			continue;	/* Ignore */

		case 't':	/* read recipients from message */
			extract++;
			continue;


		case 'v':	/* give blow-by-blow description */
			watch++;
			continue;

		case 'T':	/* set timeout interval */
		case 'C':	/* select configuration file (already done) */
		case 'c':	/* connect to non-local mailers */
		case 'd':	/* debug */
		case 'e':	/* error message disposition */
		case 'i':	/* don't let dot stop me */
		case 'm':	/* send to me too */
		case 'n':	/* don't alias */
		case 'o':	/* set option, handled in line */
		case 's':	/* save From lines in headers */
			continue;
		}
	}

	setuid(getuid());

	if (verify && extract)
		syserr("Verify mode not supported on header components");

	strcpy(subflags, "ml");
	if (rewritefrom)
		strcat(subflags, "t");
	if (watch)
		strcat(subflags, "w");
	if (!extract)
		strcat(subflags, "v");
	else
		strcat(subflags, "xto,cc,bcc*");

	if (rp_isbad (mm_init ()) || rp_isbad (mm_sbinit ()))
		syserr("Unable to submit mail at this time");
	if (rp_isbad (mm_winit ((char *) 0, subflags, from)))
		syserr("Problem with submit message initialization");
	if (!extract) {
		if (rp_isbad (retval = mm_rrply (&thereply, &len)))
			syserr("Initialization reply failure");
		switch (rp_gbval (thereply.rp_val)) {
		case RP_BNO:
		case RP_BTNO:
			syserr("Initialization failure: %s", thereply.rp_line);
		}
	}

	if (*av == NULL && !extract) {
		syserr("Usage: /usr/lib/sendmail [flags] addr...");
	}
	if (!extract) {
		while (*av)
			send_address(*av++);
		if (rp_isbad (mm_waend ()))
			syserr("Problem with address list.");
	}
	if (verify) {
		mm_end(NOTOK);
		exit(badaddrs ? 1 : 0);
	}

	doheader();

	exit(dobody());
}

send_address (addr)
char    *addr;
{
	struct rp_bufstruct thereply;
	int     retval;
	int     len;

	if (watch) {
		printf ("%s:  ", addr);
		fflush (stdout);
	}

	if (rp_isbad (retval = mm_wadr ((char *)0, addr)) ||
	    rp_isbad (retval = mm_rrply (&thereply, &len)))
		syserr("Problem in send_address [%s].", rp_valstr (retval));
	switch (rp_gval (thereply.rp_val)) {
	case RP_AOK: 
		if(watch) printf ("address ok\n");
		break;

	case RP_NO: 
		if(watch) printf ("not deliverable; unknown problem\n");
		badaddrs = TRUE;
		break;

	case RP_USER: 
		if(watch) printf ("not deliverable; unknown address.\n");
		badaddrs = TRUE;
		break;

	case RP_NDEL: 
		if(watch) printf ("not deliverable; permanent error.\n");
		badaddrs = TRUE;
		break;

	case RP_AGN: 
		if(watch) printf ("failed, this attempt; try later\n");
		badaddrs = TRUE;
		break;

	case RP_NOOP: 
		if(watch) printf ("not attempted, this time; perhaps try later.\n");
		badaddrs = TRUE;
		break;

	default: 
		syserr("Unexpected address response:  %s", thereply.rp_line);
	}
	fflush (stdout);
}

doheader()
{
	int	gotfrom, gotsender, gotdate;
	char	line[LINESIZE];

	gotfrom = gotsender = gotdate = 0;
	while (fgets (line, LINESIZE, stdin) != NULL) {
		if (line[0] == '\n')
			break;
		if (prefix ("Date:", line))
			gotdate++;
		if (prefix ("From:", line)) {
			gotfrom++;
			if (rewritefrom) {
				dofrom();
				continue;
			}
		}
		if (prefix ("Sender:", line))
			gotsender++;
		mm_wtxt (line, strlen (line));
	}
	if (!gotdate) {
		strcpy (line, "Date: ");
		cnvtdate (TIMREG, line+6);
		strcat (line, "\n");
		mm_wtxt (line, strlen(line));
	}
	if (!gotfrom)
		dofrom();
	if (!gotsender) {
		sprintf(line, "Sender: %s\n", from);
		mm_wtxt (line, strlen(line));
	}
	mm_wtxt("\n", 1);
}

dofrom()
{
	char	line[128];

	if (isstr(FullName))
		sprintf(line, "From: %s <%s>\n", FullName, from);
	else
		sprintf(line, "From: %s\n", from);
	mm_wtxt(line, strlen(line));
}

dobody()
{
	struct rp_bufstruct thereply;
	char    buffer[BUFSIZ];
	register int    i;
	int     len;

	while (!feof (stdin) && !ferror (stdin) &&
	    (i = fread (buffer, sizeof (char), sizeof (buffer), stdin)) > 0)
		if (rp_isbad (i = mm_wtxt (buffer, i)))
			syserr("Problem writing body");

	if (ferror (stdin))
		syserr("Problem reading body");

	if (rp_isbad (mm_wtend ()) || rp_isbad (mm_rrply (&thereply, &len)))
		syserr("problem ending submission");

	if (rp_isbad (thereply.rp_val))
		syserr("%s", thereply.rp_line);

	return(0);	/* eventually the program exit value */
}

smtp()
{
	char	*smtpd = dupfpath(SMTPSRVR, chndfldir);

	setuid(geteuid());	/* Must become "mmdf" for real */
	execl (smtpd, "sendmail-smtp", from, locname, "local", (char *)0);
	perror(smtpd);
	exit(9);
}

/*VARARGS1*/
syserr(fmt, a, b)
char	*fmt, *a, *b;
{
	fprintf(stderr, fmt, a, b);
	fputc('\n', stderr);
	exit(9);
}

die(sig)
int sig;
{
	mm_end(NOTOK);
	fprintf(stderr, "sendmail: dying from signal %d\n", sig);
	exit(99);
}
