/*	@(#)cronsubmit.c	1.4	96/02/27 13:12:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* cronsubmit.c	
 * cronsubmit
 *
 * This program will read the file crontab in the users sak directory
 * (default /home/sak ) and wil convert each
 * command into a transaction that will be executed by the sak server.
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
 * your sak directory under the name crontab<nr>.job
 *
 * Author: Maarten J. Huisjes, 1990
 * Modified: Gregory J. Sharp, Aug 1991
 *		- more paranoia, better error messages, nicer crontab file
 *		  specification (spaces between columns as well as tabs).
 */
#include	"amtools.h"
#include	"ampolicy.h"
#include	"stdlib.h"
#include	"bullet/bullet.h"
#include	"caplist.h"
#include	"sak.h"
#include	"common.h"
#include	"cron_stub.h"

#define CRONTAB_FILE	"crontab"

struct caplist capsenv[MAX_CAPS] = { (char *) 0, (capability *) 0 };
char *strenv[MAX_ENVS];

char *progname;

/* Forward Declarations */
char * line_to_arg _ARGS((char *, int, char **));
int8 submitjob _ARGS((char *, char *, int, char *, int8 **));
void delete_cron_jobs _ARGS((char *, char *));
void create_transfile _ARGS((char *, capability *));


void usage ()
{
	fprintf (stderr, "Usage %s [options]\n", progname);
	fprintf (stderr, "Valid options:\n");
	fprintf (stderr, "\t-Cname=capability\n");
#ifdef DEBUG
	fprintf (stderr, "\t-D\n");
#endif
	fprintf (stderr, "\t-Ename=string\n");
	fprintf (stderr, "\t-d directory      (default = %s)\n", SAK_USERDIR);
	fprintf (stderr, "\t-f cronfile-name  (default = %s)\n", CRONTAB_FILE);
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
    int line, i, cnt;
    char *av[5];
    int ac;
    int8 **sched;
    char *p, *ptr;
    b_fsize offset, nr_read;
    capability ccap;
    capability *pcap;
    char *sakdir, *cname, *filename;
    char buffer[LINESIZE];

    timeout ((interval) (10 * 1000));
    progname = argv[0];

    if ((pcap = getcap ("ROOT")) == (capability *) 0) {
	fprintf (stderr, "%s: can't find capability of root dir\n", progname);
	exit (-1);
    }

    addcapenv ("ROOT", pcap);

    p = memalloc (strlen (DEF_WORK) + 6);	/* Hack */
    (void) sprintf (p, "WORK=%s", DEF_WORK);
    add2capenv (p);
    free ((_VOIDSTAR) p);

    addstrenv ("PATH",  DEF_PATH);
    addstrenv ("_WORK", DEF_WORK);
    addstrenv ("HOME",  DEFAULT_HOME); /* see ampolicy.h */
    addstrenv ("SHELL", DEF_SHELL);

    filename = CRONTAB_FILE;
    sakdir = SAK_USERDIR; /* see ampolicy.h */

    while ((i = getopt (argc, argv, "f:d:DE:C:")) != EOF) {
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
	case 'd':
		sakdir = optarg;
		break;
	case 'f':
		filename = optarg;
		break;
	default:
		usage ();
		/* NOTREACHED */
	}
    }

    if (optind != argc) {
	usage();
	/*NOTREACHED*/
    }

    cname = memalloc (strlen (sakdir) + strlen (filename) + 2);
    (void) sprintf (cname, "%s/%s", sakdir, filename);
    if ((err = name_lookup (cname, &ccap)) != STD_OK) {
	fprintf (stderr, "%s: name_lookup of %s failed: %s\n",
					    progname, cname, err_why (err));
	exit (-1);
    }
    free ((_VOIDSTAR) cname);

#ifdef DEBUG
    printf ("%s: deleting old crontab jobs.\n", progname);
#endif

    delete_cron_jobs (sakdir, filename);

#ifdef DEBUG
    printf ("%s: adding new crontab jobs.", progname);
#endif

    line = 0;
    cnt = 0;
    offset = 0;
    ptr = buffer;
    for (;;) {
	if (cnt == 0 || (p = strchr (ptr, '\n')) == (char *) 0) {
	    if (cnt == LINESIZE) {
		fprintf (stderr, "%s: line (%d) too long in %s\n",
						    progname, line, cname);
		    exit (-1);
	    }
	    if (cnt > 0)
		    (void) memmove(buffer, ptr, cnt);

	    if (b_read (&ccap, offset, &buffer[cnt], (b_fsize) LINESIZE - cnt,
							&nr_read) != STD_OK) {
		fprintf (stderr, "%s: error reading %s : %s\n",
						progname, cname, err_why (err));
		exit (1);
	    }
	    if (nr_read == 0) {
		if (cnt > 0) {
		    fprintf (stderr, "%s: missing newline on last line of %s\n",
							    progname, cname);
		    exit (-1);
		}
		break;
	    }
	    offset += nr_read;
	    cnt += nr_read;
	    ptr = buffer;
	    continue;
	}
	line++;
	*p = '\0';
	cnt -= ((p - ptr) + 1);
	if (*ptr == '#' || *ptr == '\0') {
	    ptr = p + 1;
	    continue;
	}
	ac = 5;
	if ((ptr = line_to_arg (ptr, ac, av)) == (char *) 0 || *ptr == '\0') {
	    fprintf (stderr, "%s: no valid job specification on line # %d ?\n",
								progname, line);
	    exit (-1);
	}
	if ((err = convert_cronline (ac, av, &sched)) != STD_OK) {
	    fprintf (stderr, "%s: convert_cronline failed for line # %d\n",
							    progname, line);
	    exit (-1);
	}
	if (! submitjob (sakdir, filename, line, ptr, sched)) {
	    fprintf (stderr, "%s: submitjob failed for line # %d\n",
								progname, line);
	    exit (-1);
	}
	for (i = 0; i < MAX_SPEC; i++) 
	    free ((_VOIDSTAR) sched[i]);
	free ((_VOIDSTAR)sched);
	ptr = p + 1;
    }

    exit (0);
    /*NOTREACHED*/
}


int8 submitjob (sakdir, filename, nr, job, sched)
char *sakdir, *filename;
int nr;
char *job;
int8 **sched;
{
	errstat err;
	capability jcap, tcap;
	struct sak_job_options opts;
	char *tmpbuf;

	err = name_lookup (sakdir, &opts.where);
	if (err != STD_OK) {
		fprintf (stderr, "%s: name_lookup of %s failed: %s\n",
					    progname, sakdir, err_why (err));
		return (FALSE);
	}

	opts.catchup = FALSE;
	opts.save_result = TRUE;
	sprintf (opts.name, "%s line %d", filename, nr);
	create_transfile (job, &tcap);

	err = sak_submit_job ((char *) 0, sched, &tcap, &opts, &jcap);

	if (err != STD_OK) {
		(void) std_destroy (&tcap);
		fprintf (stderr, "%s: sak_submit_job failed: %s\n",
						    progname, err_why (err));
		return (FALSE);
	}

  	(void) std_destroy (&tcap);

	tmpbuf = memalloc (strlen (sakdir) + 20);
	(void) sprintf (tmpbuf, "%s/%s%d.job", sakdir, filename, nr);
	if ((err = name_append (tmpbuf, &jcap)) != STD_OK) {
		fprintf (stderr, "%s: cannot name_append job %s : %s\n",
			progname, tmpbuf, err_why (err));
		(void) std_destroy (&jcap);
		free ((_VOIDSTAR) tmpbuf);
		return (FALSE);
	}
	free ((_VOIDSTAR) tmpbuf);
	printf ("%s: %s submitted to sak server\n", progname, opts.name);
	return (TRUE);
}

/*
 * line_to_arg returns in 'av' pointers in the 'line' to the time spec for
 *                     the 'ac' time entries on a crontab line.  Returns a
 *		       pointer to the start of the command if all is well.
 */

char *line_to_arg (line, ac, av)
char *line;
int ac;
char **av;
{
	int i;
	char *start, *p;

	p = line;
	/* skip leading space on crontab line */
	while (*p == ' ' || *p == '\t')
	    p++;
	start = p;
	i = 0;
	/* get pointers to each time spec entry */
	while (*p) {
		if (*p == ' ' || *p == '\t') {
			*p = '\0';
			av[i++] = start;
			start = ++p;
			if (i == ac) {
				while (*start == ' ')
				    start++;
				return (start);
			}
		} else
			p++;
	}

	return ((char *) 0);
}

void delete_cron_jobs (sakdir, filename)
char *sakdir, *filename;
{
	capability fcap, dircap;
	char fmt[512], *fname, *p;
	errstat	err;
	struct dir_open *pdir;
	int nr;

	if ((err = name_lookup (sakdir, &dircap)) != STD_OK) {
		fprintf (stderr, "%s: name_lookup failure for %s : %s\n",
					     progname, sakdir, err_why (err));
		exit (-1);
	}

	if ((pdir = dir_open (&dircap)) == (struct dir_open *) 0) {
		fprintf (stderr, "%s: can't read dir `%s'\n", progname, sakdir);
		exit (-1);
	}

	sprintf (fmt, "%s%%d.job", filename);
	while ((fname = dir_next (pdir)) != (char *) 0) {
		if (sscanf (fname, fmt, &nr) != 1 ||
		    (p = strchr (fname, '.')) == (char *) 0 ||
		    strcmp (p, ".job"))
			continue;

		if ((err = dir_lookup (&dircap, fname, &fcap)) != STD_OK) {
			fprintf (stderr,
			    "\n%s: can't get capability of file `%s/%s' : %s\n",
				 progname, sakdir, fname, err_why (err));
			continue;
		}

		if ((err = std_destroy (&fcap)) != STD_OK) {
			fprintf (stderr, "%s: can't destroy %s/%s : %s\n",
				progname, sakdir, fname, err_why (err));
			exit (-1);
		}
		if ((err = name_delete (fname)) != STD_OK) {
			fprintf (stderr, "%s can't remove %s/%s : %s\n",
				    progname, sakdir, fname, err_why (err));
			exit (-1);
		}

	}

	(void) dir_close (pdir);
}

void create_transfile (job, pcap)
char *job;
capability *pcap;
{
	char *argv[10];
	errstat	err;

	argv[0] = DEFAULT_SESSION_SVR;
	argv[1] = DEFAULT_SESSION_SVR;
	argv[2] = SESSION_ARGS;
	argv[3] = "/bin/sh";
	argv[4] = "-c";
	argv[5] = job;
	argv[6] = (char *) 0;

	err = sak_exec_trans (argv, strenv, capsenv, pcap);
	if (err != STD_OK)
	{
		fprintf (stderr, "%s: couldn't create transaction file : %s\n",
						    progname, err_why (err));
		exit (-1);
	}

	return;
}
