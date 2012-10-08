/*	@(#)sak.c	1.2	94/04/06 11:53:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	"saksvr.h"
#include	"capset.h"
#include	"module/rnd.h"
#include	"soap/soap.h"

static void usage _ARGS((void));
static void createdir _ARGS((char *));
static void chmoddir _ARGS((char *));
static void check_oldcap _ARGS((capability *));
static void publish _ARGS((void));
static void resume_server _ARGS((capability *));
static void init_server _ARGS((void));
static void get_oldwork _ARGS((void));
static char *memalloc _ARGS((unsigned));

int nr_of_threads = NR_OF_THREADS_DEFAULT;
capability supercap, bulletcap;
capability pub_cap;	/* public capability for the server */
port superrnd;
objnum maxident;

struct sak_job *joblist[MODVAL];	
/* all operation on joblist will be locked by a binary semaphore.
 */
mutex job_list_sem;

char *sak_dir;
char *publish_dir  = SAK_PUBLISH_DIR_DEFAULT;
char *publish_name = SAK_PUB_NAME_DEFAULT;

#ifdef WITH_EXEC
char *exec_name = SAK_PUB_EXECNAME_DEFAULT;
capability priv_exec_cap;
capability pub_exec_cap;
#endif

char *progname;

#ifdef DEBUG
int debug = 0;
#endif

#ifdef WITH_EXEC
int with_exec = 0;
#endif


static void usage ()
{
	fprintf (stderr, "Usage : %s [-n #threads] [-I(nstall)]", progname);
#ifdef DEBUG
	fprintf (stderr, " [-D(ebug)]");
#endif
#ifdef WITH_EXEC
	fprintf (stderr, " [-W(ith exec)] [-w publish_exec_name]");
#endif
	fprintf (stderr, " [-p publish_dir]\n\t\t[-c publish_name]");
	fprintf (stderr, " [-r(epublish)] <work directory>\n\n");
	fprintf (stderr, "Default publish_dir/name : %s/%s\n",
		 SAK_PUBLISH_DIR_DEFAULT, SAK_PUB_NAME_DEFAULT);
#ifdef WITH_EXEC
	fprintf (stderr, "Default publish_dir/execname : %s/%s\n",
		 SAK_PUBLISH_DIR_DEFAULT, SAK_PUB_EXECNAME_DEFAULT);
#endif
	fprintf (stderr, "Super cap only published with -I or -r flag\n");
	fprintf (stderr, "Default # threads %d\n", NR_OF_THREADS_DEFAULT);
	exit (-2);
}

main (argc, argv)
int argc;
char *argv[];
{
	capability mfilecap;
	int i, flag;
	int8 republish = FALSE;
	int8 install = FALSE;
	errstat	err;

	progname = strrchr (argv[0], '/');
	if (progname == (char *) 0)
		progname = argv[0];
	else
		progname++;


	timeout (NORMAL_TIMEOUT);
	while ((flag = getopt (argc, argv, "w:WDIrn:p:c:")) >= 0) {
		switch (flag) {
		case 'I':
			install = TRUE;
			break;
		case 'r':
			republish = TRUE;
			break;
		case 'p':
			publish_dir = optarg;
			break;
		case 'c':
			publish_name = optarg;
			break;
		case 'n':
			nr_of_threads = atoi (optarg);
			if (nr_of_threads <= NR_OF_REQ_THREADS) {
				fprintf (stderr,
					 "# threads must be greater %d\n",
					 NR_OF_REQ_THREADS);
				usage ();
				/* NOTREACHED */
			}
			break;
#ifdef WITH_EXEC
		case 'W':
			with_exec = TRUE;
			break;
#endif
#ifdef WITH_EXEC
		case 'w':
			exec_name = optarg;
			break;
#endif
#ifdef DEBUG
		case 'D':
			debug++;
			break;
#endif
		case  '?':
		default:
			usage ();
			/* NOTREACHED */
		}
	}

	if (optind + 1 != argc)
		usage ();

	sak_dir = argv[optind];
	init_server_sleep ();		/* must be done before resume
					 * because schedule can call wakeup
					 */
	init_pool_sems ();

	while ((err = cwd_set (sak_dir)) != STD_OK) {
		/* only if it is not there and the Install flag has
		 * been specified then we try to create the directory
		 */
		if (install == FALSE || err != STD_NOTFOUND) {
			fprintf (stderr, "Can't cwd_set to %s : %s\n",
				 sak_dir, err_why (err));
			exit (-1);
		}
		createdir (sak_dir); /* this exits if it fails! */
	}
	/*
	 * A little paranoia for our pains - make sure we haven't been
	 * cracked since we last started
	 */
	chmoddir (sak_dir);

	if ((err = name_lookup  (DEF_BULLETSVR, &bulletcap)) != STD_OK) {
		fprintf (stderr, "Lookup failure for %s : %s\n",
			 DEF_BULLETSVR, err_why (err));
		exit (-1);
	}

	if ((err = name_lookup (SAK_MASTERFILE, &mfilecap)) == STD_OK) {
		if  (install == TRUE) {
			fprintf (stderr, "%s already present\n", SAK_MASTERFILE);
			exit (-1);
		}	
		resume_server (&mfilecap);
	} else if (err == STD_NOTFOUND) {
		/* install flag has to be specified before trying to install */
		if (install == FALSE) {
			fprintf (stderr, "No %s, must install server (-I)\n",
			 SAK_MASTERFILE);
			exit (-1);
		}
		init_server ();
	} else {
		fprintf (stderr, "Lookup failure for %s : %s\n",
			 SAK_MASTERFILE, err_why (err));
		exit (-1);
	}
	
	if (install || republish) publish ();

#ifdef WITH_EXEC
	if (with_exec) {
		for (i = 0; i < NR_OF_EXECTHREADS; i++) {
			if (! create_newthread (NONE, EXEC_TYPE)) {
				fprintf (stderr,
				       "Could not start enough exec threads\n");
				exit (-1);
			}
		}

#ifdef DEBUG
		if (debug > 0)
			printf ("Started %d exec threads\n", NR_OF_EXECTHREADS);
#endif
	}
#endif
	for (i = 0; i < nr_of_threads; i++) {
		if (! create_newthread (NONE, (i < NR_OF_REQ_THREADS) ?
							REQ_TYPE : FREE)) {
			fprintf (stderr, "Not enough threads started\n");
			exit (-1);
		}
	}


#ifdef DEBUG
	if (debug > 0)
		printf ("Working directory %s\nStarted %d server threads\n", sak_dir, nr_of_threads);
#endif

	handle_jobs ();
	/* NOTREACHED */
}

/* chmod the sakdir to have only "safe rights"
*/
static void chmoddir (dir)
char * dir;
{
	errstat	err;
	static long dir_masks[] = { (long) OWNERRIGHTS,
				    (long) GROUPRIGHTS,
				    (long) OTHERRIGHTS };

	if ((err = sp_chmod (SP_DEFAULT, dir, 3, dir_masks)) != STD_OK) {
		fprintf (stderr, "Can't chmod %s : %s\n", dir, err_why (err));
		exit (-1);
	}
}

/* create directory specified by dir and set apropriate masks
 * dir		in	name of new dir
 */
static void createdir (dir)
char *dir;
{
	errstat	err;

	fprintf (stderr, "Creating dir %s\n", dir);
	if ((err = sp_mkdir (SP_DEFAULT, dir, (char **) 0)) != STD_OK) {
		fprintf (stderr, "Can't create %s : %s\n", dir, err_why (err));
		exit (-1);
	}
	chmoddir (dir);
}

/* check capability to see if it's server is still running
 * pcap		in	pointer to capability of server
 */
static void check_oldcap (pcap)
capability *pcap;
{
	interval save_timeout;
	char buf[50];
	int n;
	errstat	err;

#ifdef DEBUG
	if (debug > 0) {
		printf ("Searching for old server.. ");
		fflush (stdout);
	}
#endif

	save_timeout = timeout (SHORT_TIMEOUT);
	if ((err = std_info (pcap, buf, 50, &n)) != RPC_NOTFOUND) {
		if (err == STD_OK)
			fprintf (stderr,
				"Old server still running !!. Exiting..\n");
		else
			fprintf (stderr,
			"Transaction failure while locating old server : %s\n",
			err_why (err));

		exit (-1);
	}
	timeout (save_timeout);

#ifdef DEBUG
	if (debug > 0) printf ("Not found, assuming it has died\n");
#endif
}

/* publish capability for sak server in default directory (unless requested
 * otherwise).
 */
static void publish ()
{
	capability tmpcap;
	char *where;
	errstat	err;

	where = memalloc (strlen (publish_dir) + strlen (publish_name) + 2);

	sprintf (where, "%s/%s", publish_dir, publish_name);

#ifdef DEBUG
	if (debug > 0) printf ("Publish super cap at %s\n", where);
#endif

	if ((err = name_lookup (where, &tmpcap)) != STD_NOTFOUND) {
		if (err != STD_OK) {
			fprintf (stderr, "Lookup failure for %s : %s\n",
				 where, err_why (err));
			exit (-1);
		}
		fprintf (stderr, "Warning %s already exists\n", where);
		check_oldcap (&tmpcap);		/* will exit if port alive */
		(void) name_delete (where);
	}

	/* publish put port for sak server */
	if ((err = name_append (where, &pub_cap)) != STD_OK) {
		fprintf (stderr, "cannot append %s to dir : %s\n",
			 where, err_why (err));
		exit (-1);
	}

#ifdef WITH_EXEC
	sprintf (where, "%s/%s", publish_dir, exec_name);
	(void) name_delete (where);

	/* publish put port for exec server thread */
	if ((err = name_append (where, &pub_exec_cap)) != STD_OK) {
		fprintf (stderr, "cannot append %s to dir : %s\n",
			 where, err_why (err));
		exit (-1);
	}
#endif

}

/* try to resume work of a old server
 * pmfilecap	in	pointer to capability of masterfile of old server
 */
static void resume_server (pmfilecap)
capability *pmfilecap;
{
	char *p, *tmpbuf;
	rights_bits dummy;
	b_fsize lsize;
	errstat	err;

#ifdef DEBUG
	if (debug > 0) printf ("Reading %s\n", SAK_MASTERFILE);
#endif
	tmpbuf = memalloc (MFBUFSIZE);

	MON_EVENT ("Resumed server");
	err = b_read (pmfilecap, (b_fsize) 0, tmpbuf,
						(b_fsize) MFBUFSIZE, &lsize);
	if (err != STD_OK) {
		fprintf (stderr, "cannot read masterfile %s : %s",
					     SAK_MASTERFILE, err_why (err));
		exit (-1);
	}

	maxident = (objnum) 1; /* will be updated by get_oldjobs () */

	p = tmpbuf;
#ifdef WITH_EXEC
	p = buf_get_cap  (p, tmpbuf + lsize, &priv_exec_cap);
#endif
	p = buf_get_cap  (p, tmpbuf + lsize, &supercap);
	p = buf_get_port (p, tmpbuf + lsize, &superrnd);
	if (p != tmpbuf + lsize) {
		fprintf (stderr, "Corrupted %s : size incorrect\n", SAK_MASTERFILE);
		exit (-1);
	}

	free ((_VOIDSTAR) tmpbuf);

	/* init the various public ports for publishing */
	priv2pub (&supercap.cap_port, &pub_cap.cap_port);
	pub_cap.cap_priv = supercap.cap_priv;
#ifdef WITH_EXEC
	priv2pub (&priv_exec_cap.cap_port, &pub_exec_cap.cap_port);
	pub_exec_cap.cap_priv = priv_exec_cap.cap_priv;
#endif

	/* sanity check, just to be sure */

	if (prv_decode (&supercap.cap_priv, &dummy, &superrnd) != 0) {
		fprintf (stderr, "Corrupted super capability in %s\n",
				 SAK_MASTERFILE);
		exit (-1);
	}

	check_oldcap (&supercap);
	/* end of sanity checks */

	get_oldwork ();

#ifdef DEBUG
	if (debug > 1) printf ("Resumed Server\n");
#endif
}

/* create a new masterfile */
static void init_server ()
{
	capability mfilecap; /* capability of masterfile */
	char *p, *tmpbuf;
	errstat	err;

	MON_EVENT ("Initial server");
#ifdef DEBUG
	if (debug > 0) printf ("creating %s in %s\n", SAK_MASTERFILE, sak_dir);
#endif

#ifdef WITH_EXEC
	uniqport (&priv_exec_cap.cap_port);	/* create a uniq exec thread port */
	priv2pub (&priv_exec_cap.cap_port, &pub_exec_cap.cap_port);
	pub_exec_cap.cap_priv = priv_exec_cap.cap_priv;
#endif
	uniqport (&supercap.cap_port);	/* create a uniq server port */
	uniqport (&superrnd);		/* create a generatic cap */

	(void) prv_encode (&supercap.cap_priv, GENERATIC, OWNERRIGHTS,
			   &superrnd);

	priv2pub (&supercap.cap_port, &pub_cap.cap_port);
	pub_cap.cap_priv = supercap.cap_priv;

	tmpbuf = memalloc (MFBUFSIZE);

	p = tmpbuf;
#ifdef WITH_EXEC
	/* save the private exec port, not the public! */
	p = buf_put_cap  (p, tmpbuf + MFBUFSIZE, &priv_exec_cap);
#endif
	/* save the private super port, not the public! */
	p = buf_put_cap  (p, tmpbuf + MFBUFSIZE, &supercap);
	p = buf_put_port (p, tmpbuf + MFBUFSIZE, &superrnd);

	if (p == (char *) 0) {
		fprintf (stderr, "MFBUFSIZE to small. Enlarge and recompile\n");
		exit (-3);
	}

	/* create masterfile */
	if ((err = b_create (&bulletcap, tmpbuf, (b_fsize) (p - tmpbuf),
			     BS_COMMIT, &mfilecap)) != STD_OK) {
		fprintf (stderr, "cannot create master bullet file : %s\n",
			 err_why (err));
		exit (-1);
	}

	free ((_VOIDSTAR) tmpbuf);

	/* put masterfile in directory */
	if ((err = name_append (SAK_MASTERFILE, &mfilecap)) != STD_OK) {
		(void) std_destroy (&mfilecap); /* cleanup */
		fprintf (stderr, "cannot append %s to dir %s : %s\n",
			 SAK_MASTERFILE, sak_dir, err_why (err));
		exit (-1);
	}

	maxident = (objnum) 1;

	createdir (JOBDIR);
	createdir (TRANSDIR);

#ifdef DEBUG
	if (debug > 1) printf ("Initialized Server\n");
#endif
}

/* Get old jobs from previous server
 */
static void get_oldwork ()
{
	struct sak_job *pnewjob;
	capability fcap, dircap;
	char *fname, *p, *tmpbuf;
	b_fsize lsize;
	errstat	err;
	struct dir_open *pdir;

#ifdef DEBUG
	if (debug > 0) printf ("Scanning directory %s/%s for old jobs\n",
				sak_dir, JOBDIR);
#endif

	if ((err = name_lookup  (JOBDIR, &dircap)) != STD_OK) {
		fprintf (stderr, "Lookup failure for %s : %s\n",
			 JOBDIR, err_why (err));
		exit (-1);
	}

	if ((pdir = dir_open (&dircap)) == (struct dir_open *) 0) {
		fprintf (stderr, "Can't read dir %s/%s\n", sak_dir, JOBDIR);
		exit (-1);
	}

	tmpbuf = memalloc (JFBUFSIZE);

	while ((fname = dir_next (pdir)) != (char *) 0) {

		if (STRN_NOT_EQUAL (fname, JOBPREFIX, JOBPREFIX_LEN) ||
		    *(fname + JOBPREFIX_LEN) < '0' ||
		    *(fname + JOBPREFIX_LEN) > '9')
			/* not a legal filename for a job, just ignore it */
			continue;
#ifdef DEBUG
		if (debug > 1) printf ("\nfound old job %s", fname);
#endif
		/* find file */
		if ((err = dir_lookup (&dircap, fname, &fcap)) != STD_OK) {
			fprintf (stderr,
				 "\nCan't get capability of file `%s' : %s\n",
				 fname, err_why (err));
			continue;
		}

		/* read file into buffer */
		if ((err = b_read (&fcap, 0L, tmpbuf, (b_fsize) JFBUFSIZE,
				       &lsize)) < 0) {
			fprintf (stderr,
				 "\nCan't read file `%s' : %s\n",
				 fname, err_why (err));
			continue;
		}
		
		if ((pnewjob = newjob ()) == NILCELL)
			exit (1);

		pnewjob->ident = ato_objnum (fname + JOBPREFIX_LEN);
		if (pnewjob->ident > maxident)
			maxident = pnewjob->ident;

		p = tmpbuf;
		p = buf_get_port	    (p, tmpbuf + lsize, &pnewjob->rnd);
		p = buf_get_sak_job_options (p, tmpbuf + lsize, &pnewjob->opt);
		err = STD_OK;
		p = buf_get_sched (p, tmpbuf + lsize, pnewjob->schedule, &err);
		if (p != tmpbuf + lsize) {
			if (err == STD_NOMEM) {
				fprintf (stderr,
				 "Malloc failed for schedule of job %s\n",
				 fname);
				 exit (1);
			} else {
				fprintf (stderr, "Bad file format of job %s\n",
					 fname);
				freejob (pnewjob);
				fprintf (stderr, "Job ignored\n");
				continue;
			}
		}
#ifdef DEBUG
		if (debug > 1) printf (" named %s\n", pnewjob->opt.name);
#endif

		if (! schedule (pnewjob, time ((time_t *) 0))) {
			send_failure (&pnewjob->opt,
				"(internal) time past while server not running",
				(errstat) SAK_TOOLATE);
			deletejobfiles (pnewjob->ident, TRUE);
			freejob (pnewjob);
			continue;
		}

		insert_joblist (pnewjob);
		insert_runlist (pnewjob);
	}
#ifdef DEBUG
	if (debug > 1) printf ("\n");
#endif

	(void) dir_close (pdir);
	free ((_VOIDSTAR) tmpbuf);
}

static char *memalloc (bytes)
unsigned bytes;
{
	char *p;

	if ((p = (char *) malloc (bytes)) == (char *) 0) {
		fprintf (stderr, "Couldn't alloc %u bytes\n", bytes);
		exit (-1);
	}

	return (p);
}
