/*	@(#)at.c	1.3	96/02/27 13:11:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* at.c	
 * at time [date] prog
 *
 * This program will take as arguments a time followed by a optional date
 * last is a prog which specifies the programm to be executed at the
 * specified time.
 *
 * It will convert the time and date into a format accepted by the sak(S)
 * server with a call to convert_atline(L). See convert_atline for recog-
 * nised formats for time and date. at(U) will then convert prog into a
 * transaction that will be executed by the sak server.
 * Since the sak server can only execute transactions, We needs the
 * capability of a server that will accept requests for execution of
 * a specified file, this server is defined a DEF_EXECSVR. For now
 * this is a special thread within the sak server, but this should be
 * changed at a later date. Make sure that when a new exec file server
 * is installed it might expect different arguments in its buffer.
 * This transaction is saved in a bullet file which is passed as
 * parameter to the sak server.
 * Since this file is copied by the sak server there is no need to keep it
 * around and thus destroyed.
 * The capability returned by the server is appended to 
 * your sak directory under the name prog.job
 *
 * Author: Maarten J. Huisjes, 1990
 * Modified: Gregory J. Sharp, Aug 1991
 *		Delinted, made it more paranoid about arguments & security
 */

#include	"amtools.h"
#include	"ampolicy.h"
#include	"bullet/bullet.h"
#include	"caplist.h"
#include	"sak.h"
#include	"common.h"
#include	"at_stub.h"

struct caplist capsenv[MAX_CAPS] = { (char *) 0, (capability *) 0 };
char *strenv[MAX_ENVS];

char *progname, *sakdir;

extern char **environ;
extern struct caplist *capv;

char *env_lookup _ARGS((char **, char *));

/* Forward declarations */
void empty_capenv _ARGS((void));
int8 submitjob _ARGS((char *, char *, char **, int8 **, int8));
void givename _ARGS((char *, char *, char *, int));
void create_transfile _ARGS((char *, char **, capability *));
void copy_strenv _ARGS((void));
void copy_capenv _ARGS((void));
char *fromstdin _ARGS((char *, char *));


void usage ()
{
    fprintf (stderr, "Usage %s [options] time [date] [+ increment] [script]\n",
								    progname);
    fprintf (stderr, "Valid options\n");
    fprintf (stderr, "\t-Cname=capability\n");
#ifdef DEBUG
    fprintf (stderr, "\t-D\n");
#endif /* DEBUG */
    fprintf (stderr, "\t-Ename=string or -E 0\n");
    fprintf (stderr, "\t-c\n");
    fprintf (stderr, "\t-d dirname   (default = %s)\n", SAK_USERDIR);
    fprintf (stderr, "\t-n cmd_name  (default = name of script file)\n");
    fprintf (stderr, "\t-s shell     (default = %s)\n", DEF_SHELL);
    exit (-1);
}

#ifdef DEBUG
int debug = 0;
#endif

main (argc, argv)
int argc;
char *argv[];
{
	errstat err;
	extern int atoptind;
	extern int optind;
	char *name, *shell;
	int8 **sched;
	int8 catchup;
	int i;

	timeout ((interval) (10 * 1000));
	progname = argv[0];
	name = (char *) 0;
	shell = (char *) 0;
	catchup = FALSE;

	copy_strenv ();
	copy_capenv (); /* Only ROOT and WORK are copied */

	sakdir = SAK_USERDIR; /* see ampolicy.h */

	while ((i = getopt (argc, argv, "cs:n:d:DE:C:")) >= 0) {
		switch (i) {
		case 'C':
			add2capenv (optarg);
			break;
#ifdef DEBUG
		case 'D':
			debug++;
			break;
#endif
		case 'E':
			if (strcmp (optarg, "0") == 0)
				empty_strenv ();
			else
				add2strenv (optarg);
			break;
		case 'c':
			catchup = TRUE;
			break;
		case 'd':
			sakdir = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case 's':
			shell = optarg;
			break;
		default:
			usage ();
			/* NOTREACHED */
		}
	}

	if (shell == (char *) 0) {
	    shell = env_lookup (strenv, "SHELL");
	    if (shell == (char *) 0)
		shell = DEF_SHELL;
	}

	i = argc - optind;
	if (i != 1 && i != 2)
	    usage();

	err = convert_atline (i, argv + optind, &sched);
	if (err != STD_OK)
		exit (-1);

	if (! submitjob (shell, name, &argv[optind + atoptind], sched, catchup))
		exit (-1);
		
	exit (0);
	/*NOTREACHED*/
}


#if defined(__STDC__)
int8 submitjob (
char *shell,
char *name,
char **jobargs,
int8 **sched,
int8 catchup)
#else
int8 submitjob (shell, name, jobargs, sched, catchup)
char *shell;
char *name;
char **jobargs;
int8 **sched;
int8 catchup;
#endif
{
	errstat err;
	capability jcap, tcap;
	struct sak_job_options opts;
	char *tmpbuf;

	err = name_lookup (sakdir, &opts.where);
	if (err != STD_OK) {
		fprintf (stderr,
			    "%s: submitjob: name_lookup of %s failed: %s\n",
					    progname, sakdir, err_why (err));
		return (FALSE);
	}

	opts.catchup = catchup;
	opts.save_result = TRUE;

	givename (name, *jobargs, opts.name, MAX_NAME_LEN);

	if (*jobargs == (char *) NULL)
	{
		jobargs = (char **) memalloc (2 * sizeof (char *));
		jobargs[0] = fromstdin (opts.name, shell);
		jobargs[1] = (char *) 0;
	}

	create_transfile (shell, jobargs, &tcap);

	err = sak_submit_job ((char *) 0, sched, &tcap, &opts, &jcap);
  	(void) std_destroy (&tcap);

	if (err != STD_OK) {
		fprintf (stderr, "%s: sak_submit_ job failed: %s\n",
						    progname, err_why (err));
		return (FALSE);
	}

	tmpbuf = memalloc (strlen (sakdir) + strlen (opts.name) + 6);
	(void) sprintf (tmpbuf, "%s/%s.job", sakdir, opts.name);
	if ((err = name_append (tmpbuf, &jcap)) != STD_OK) {
		fprintf (stderr,
			 "%s: name_append append for job %s failed: %s\n",
					    progname, tmpbuf, err_why (err));
		(void) std_destroy (&jcap);
		free ((_VOIDSTAR) tmpbuf);
		return (FALSE);
	}
	free ((_VOIDSTAR) tmpbuf);

	return (TRUE);
}

static void getuniqatname (to, defname, len)
char *to;
char *defname;
int len;
{
	int nr = 2;
	errstat err;
	capability dummy;
	char tmpbuf[512];

	len -= 4;
	sprintf (tmpbuf, "%s/at_%.*s.job", sakdir, len, defname);
	for (;;) {
		if ((err = name_lookup (tmpbuf, &dummy)) == STD_NOTFOUND) {
			if (nr == 2)
				(void) sprintf (to, "at_%.*s", len, defname);
			else
				(void) sprintf (to, "at%d_%.*s", nr - 1,
								len, defname);
			return;
		}
		if (err != STD_OK) {
			fprintf (stderr,
				   "%s: can't generate unique jobname : %s",
						    progname, err_why (err));
			exit (-1);
		}
		if (nr == 2) len--;
		if (nr == 10) len--;
		if (nr == 100) len--;
		if (nr == 1000) len--;
		(void) sprintf (tmpbuf, "%s/at%d_%.*s.job",
						sakdir, nr, len, defname);
		nr++;
	}
}

void givename (name, from , to, len)
char *name, *from, *to;
int len;
{
	char *start;
	int real_len;
	char *tmpbuf;

	if (name != (char *) 0) {
		getuniqatname (to, name, len);
		return;
	}

	if (from == (char *) 0) {
		getuniqatname (to, "stdin", len);
		return;
	}

	start = from;
	while (*from != ' ' && *from != '\0') {
		if (*from == '/') start = from + 1;
		from++;
	}

	if ((real_len = (from - start)) >= (len - 3))
		real_len = len - 1;

	tmpbuf = memalloc ((size_t) real_len);
	strncpy (tmpbuf, start, (size_t) real_len);
	tmpbuf[real_len] = '\0';

	getuniqatname (to, tmpbuf, len);
}

void create_transfile (shell, jobargs, pcap)
char *shell, **jobargs;
capability *pcap;
{
	char *p;
	char *argv[10];
	errstat	err;
	char *tmpbuf;
	int i, len;

	len = 0;
	for (i = 0; jobargs [i] != (char *) 0; i++)
		len += strlen (jobargs [i]) + 1;
	
	p = tmpbuf = memalloc ((size_t) (len + 1));
	for (i = 0; jobargs [i] != (char *) 0; i++)
	{
		sprintf (p, "%s ", jobargs [i]);
		p += strlen (jobargs [i]) + 1;
	}
	*--p = '\0';

	argv[0] = DEFAULT_SESSION_SVR;
	argv[1] = DEFAULT_SESSION_SVR;
	argv[2] = SESSION_ARGS;
	argv[3] = shell;
	argv[4] = "-c";
	argv[5] = tmpbuf;
	argv[6] = (char *) 0;

	err = sak_exec_trans (argv, strenv, capsenv, pcap);
	if (err != STD_OK)
	{
		fprintf (stderr, "%s: couldn't create transaction file : %s\n",
						    progname, err_why (err));
		exit (-1);
	}

	free ((_VOIDSTAR) tmpbuf);
	return;
}

void copy_strenv ()
{
	int i;

	for (i = 0; environ[i] != (char *) 0; i++)
		add2strenv (environ[i]);
}

void copy_capenv ()
{
	capability *pcap;

/*
 *	int i;
 *
 *	just to much, lets only copy WORK and ROOT
 * 	for (i = 0; capv[i]. cl_name != (char *) 0; i++)
 * 		if (strcmp (capv[i]. cl_name, "_SESSION") != 0 &&
 * 		    strcmp (capv[i]. cl_name, "TTY") != 0 &&
 * 		    strcmp (capv[i]. cl_name, "STDIN") != 0 &&
 * 		    strcmp (capv[i]. cl_name, "STDOUT") != 0 &&
 * 		    strcmp (capv[i]. cl_name, "STDERR") != 0 &&
 * 		    strncmp (capv[i]. cl_name, "_f:", 3) != 0)
 * 			addcapenv (capv[i]. cl_name, capv[i]. cl_cap);
 */
	if ((pcap = getcap ("ROOT")) == (capability *) 0) {
		fprintf (stderr, "%s: can't find capability of root dir\n",
								    progname);
		exit (-1);
	}
	addcapenv ("ROOT", pcap);

	if ((pcap = getcap ("WORK")) == (capability *) 0) {
		fprintf (stderr, "%s: can't find capability of cwd (WORK)\n",
								    progname);
		exit (-1);
	}
	addcapenv ("WORK", pcap);
}

char *fromstdin (fname, shell)
char *fname;
char *shell;
{
	FILE *fp;
	char *tmp, buffer[512];


	tmp = memalloc (strlen (fname) + strlen (sakdir) + 9);
	sprintf (tmp, "%s/%s.script", sakdir, fname);
	if ((fp = fopen (tmp, "w")) == (FILE *) 0)
	{
		perror (fname);
		exit (1);
	}
	fprintf(fp, "#! %s\n", shell);

	printf ("at> ");
	fflush (stdout);
	while (! feof (stdin) && fgets (buffer, 512, stdin) != (char *) 0)
	{
		fputs (buffer, fp);
		printf ("at> ");
		fflush (stdout);
	}
	printf("\n"); /* For the last line which has a ^D */

	fclose (fp);
	return tmp;
}
