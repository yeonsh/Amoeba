/*	@(#)setup.c	1.2	92/05/14 10:42:41 */
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
/*      make directories to be used by mmdf
 *
 *  Jan 82  D. Crocker          convert to !traceit before chmod of lockdir
 */
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include "ch.h"

extern char *mmdflogin;        /* login name for mmdf processes */

/* logs, programs, delivered mail */

extern char
	*logdfldir,
	*phsdfldir,
	*tbldfldir,
	*cmddfldir,
	*chndfldir,
	*mldfldir;

extern char     *lckdfldir,
	      *pn_quedir;

#ifdef JNTMAIL
int daemonuid,
    daemongid;
#define	DAEMON	"daemon"		/* Can be overridden by CC option */
char daemonname[]  = DAEMON;
#endif /* JNTMAIL */

/* the queue */

extern int queprot;             /* protection on quedfldir[] parent */

extern char
	*quedfldir,
	*tquedir,
	*aquedir,
	*squepref,
	*mquedir;

extern	int	ch_numchans;
extern	Chan	**ch_tbsrch;

int mmdfuid,
    mmdfgid;
int traceit;

/**/

main (argc, argv)
	int argc;
	char *argv[];
{
    extern struct passwd *getpwnam ();
    struct passwd *mmdfpwd;
    int realid,
	effecid;
    register int i;

    mmdf_init (argv[0]);
#ifdef JNTMAIL
    if ((mmdfpwd = getpwnam (daemonname)) == (struct passwd *) NULL)
    {
	printf ("No daemon entry in paswd file (%s)\n", daemonname);
	exit (NOTOK);
    }
    daemonuid = mmdfpwd -> pw_uid;
    daemongid = mmdfpwd -> pw_gid;
#endif


    while (argc-- > 1)
	if (argv[argc][0] == '-')
	    switch (argv[argc][1])
	    {
		case 'd':       /* debug              */
		case 'n':       /* like make's switch */
		case 't':       /* trace              */
		    traceit = TRUE;
		    break;

		default:
		    printf ("Unknown switch '%s'\n", argv[argc]);
		    (void) fflush (stdout);
		    exit (-1);
	    }

    getwho (&realid, &effecid);
    if (effecid != 0)
	printf ("   [Not running with superuser rights]\n");

    if (traceit)
	printf ("\n   [Trace only; no actions will be taken]\n");
    else
    {
	printf ("\n   [Intermediate directories will be created, if]\n");
	printf ("   [necessary.  No changes will be made to]\n");
	printf ("   [directories which already exist.]\n");
    }
    putchar ('\n');
    (void) fflush (stdout);

/*  get uid & gid for mmdf login, for setting directory ownerships */

/*    if ((mmdfpwd = getpwnam (mmdflogin)) == (struct passwd *) NULL)
    {
	printf ("***\t Login name '%s' not in password file.\n", mmdflogin);
	(void) fflush (stdout);
	exit (NOTOK);
    }*/
    mmdfuid = 0; /*mmdfpwd -> pw_uid;*/
    mmdfgid = 1; /*mmdfpwd -> pw_gid;*/

/*    printf ("Login '%s':  uid (%d), gid (%d)\n",
		mmdflogin, mmdfuid, mmdfgid);
    printf ("***\t Make sure Makefile's $(MMDFLOGIN) specifies this name.\n");*/
/**/

/*  create standard directories */

    printf ("\nCommand library directory '%s'\n\t[protected at 0711]\n", cmddfldir);
    if (my_drcreat (cmddfldir, 0711, mmdfuid, mmdfgid) >= 0)
    {
	printf ("***\t Make sure Makefile's $(LIBDIR) specifies this directory.\n");
	(void) fflush (stdout);
    }
    printf ("\nLogging directory '%s'\n\t[protected at 0711]\n", logdfldir);
    if (my_drcreat (logdfldir, 0711, mmdfuid, mmdfgid) >= 0)
    {
	printf ("***\t Check and run 'setlogs'.\n");
	printf ("***\t It should chdir to this directory.\n");
    }
    printf ("\nPhase (timestamping) directory '%s'\n\t[protected at 0711]\n", phsdfldir);
    my_drcreat (phsdfldir, 0711, mmdfuid, mmdfgid);

#ifndef V4_2BSD
    printf ("\nLocking directory '%s'\n\t[protected at 0777]\n", lckdfldir);
    my_drcreat (lckdfldir, 0777, mmdfuid, mmdfgid);
#endif /* V4_2BSD */

#ifdef JNTMAIL
    printf ("\nNIFTP (JNT Mail) Spool directory '%s'\n\t[protected at 0700]\n",
		pn_quedir);
    my_drcreat (pn_quedir, 0700, daemonuid, daemongid);
#endif

    printf ("\nName table & telephone scripts '%s'\n\t[protected at 0711]\n", tbldfldir);
    my_drcreat (tbldfldir, 0711, mmdfuid, mmdfgid);

    printf ("\nChannel program directory '%s'\n\t[protected at 0700]\n", chndfldir);
    if (my_drcreat (chndfldir, queprot, mmdfuid, mmdfgid) >= 0)
    {
	printf ("***\t Make sure Makefile's $(CHANDIR) specifies this directory.\n");
	(void) fflush (stdout);
    }

    if (mldfldir != (char *)0 && !isnull (mldfldir[0]))
    {                           /* all mail delivered in common dir     */
	printf ("\nRecipient users' mailbox directory '%s'\n\t[protected at 0777]\n", mldfldir);
	my_drcreat (mldfldir, 0777, mmdfuid, mmdfgid);
    }
/**/

/*  create and set-up queue directory substructure */

    printf ("\nMail queue home directory '%s'\n\t[protected at 0777]\n", quedfldir);
    my_drcreat (quedfldir, 0777, mmdfuid, mmdfgid);
				 /* sub directories all open access      */

    printf ("\tChanging into queue home directory\n");
    if (chdir (quedfldir) < 0)
    {
	errprt (quedfldir);
	printf ("*** Cannot further process queue directories\n");
	(void) fflush (stdout);
	exit (-1);
    }
    printf ("\tLocking its parent [protected at %0o]\n", queprot);
    if (!traceit)
	if (chmod ("..", queprot) < 0)
		errprt ("quedfldir parent");
				      /* couldn't lock PARENT          */

    printf ("Temporary address directory '%s'\n\t[protected at 0777]\n", tquedir);
    my_drcreat (tquedir, 0777, mmdfuid, mmdfgid);

    printf ("Queued-address directory '%s'\n\t[protected at 0777]\n", aquedir);
    my_drcreat (aquedir, 0777, mmdfuid, mmdfgid);

    printf ("Message text directory '%s'\n\t[protected at 0777]\n", mquedir);
    my_drcreat (mquedir, 0777, mmdfuid, mmdfgid);

    for (i = 0; i < ch_numchans; i++) {
	char tmpname[64];

	if (ch_tbsrch[i] != (Chan *)NULL)
	{
	    (void) sprintf (tmpname, "%s%s", squepref, ch_tbsrch[i]->ch_queue);
	    printf ("Queue directory '%s'\n\t[protected at 0777]\n", tmpname);
	    my_drcreat (tmpname, 0777, mmdfuid, mmdfgid);
	}
    }

    if (traceit)
	printf ("\n   [Trace only; no actions were taken.]\n");

    (void) fflush (stdout);
    exit (OK);
}
/**/

errprt (pref)
	char pref[];
{
    (void) fflush (stdout);
    perror (pref);
    (void) fflush (stderr);
}

my_drcreat (path, prot, uid, gid)
	char *path;
	int prot,
	    uid,
	    gid;
{
    extern struct passwd *getpwuid ();
    struct stat statbuf;
    struct passwd *pwdptr;
    register int retval;

    if (traceit)
	return (0);

    if (stat (path, &statbuf) >= OK)
    {                             /* already exists  */
	printf ("\t['%s' already exists.]\n", path);
	statbuf.st_mode &= 04777;
	if (statbuf.st_uid != uid)
	{
	    if ((pwdptr = getpwuid (statbuf.st_uid)) != NULL)
		printf ("***\t uid (%s) is not %s\n",
			    pwdptr -> pw_name, mmdflogin);
	    else
		printf ("UNREGISTERED owner uid (%d) is not %s\n",
			    statbuf.st_uid, mmdflogin);
	}
	if (statbuf.st_gid != gid)
	    printf ("***\t group id is (%d) and not (%d)\n",
			statbuf.st_gid, gid);

    	/* Allow liberal group permissions to ease management */
	if ((statbuf.st_mode&04707) != (prot&04707))
	    printf ("***\t protections (%o) should be (%o)\n",
			statbuf.st_mode, prot);
	return (OK);
    }

    printf ("\tCreate it? ");
    (void) fflush (stdout);
    switch (getchar ())
    {
	case 'Y':
	case 'y':
	    while (getchar () != '\n')
		continue;       /* DROP ON THROUGH */
	case '\n':
	    if ((retval = creatdir (path, prot, uid, gid)) < 0)
		errprt (path);
	    return (retval);
    }
    while (getchar () != '\n');
    return (NOTOK);
}

