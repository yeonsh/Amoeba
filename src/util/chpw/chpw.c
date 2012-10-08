/*	@(#)chpw.c	1.4	96/02/27 13:08:09 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Change password of an Amoeba user
 *	Author: Siebren van der Zee, CWI.
 *	Modified: Greg Sharp, March 1992 - less noise, more standard includes.
 *	Modified: Peter Keuning, May 1994 - check old password first.
 *	Modified: Greg Sharp, March 1995 - used library getpass() to save code
 */

#include <amoeba.h>
#include <ampolicy.h>
#include <stderr.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <posix/termios.h>
#include <class/tios.h>
#include <bullet/bullet.h>
#include <module/name.h>
#include <module/rnd.h>
#include <module/stdcmd.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#define PASSWORD	"passwd"
#define FN_LEN		255		/* Max length of a filename */

/*
 *	Amoeba stuff:
 */
extern char *getpass();

/*
 *	Standard C and Unix emulation stuff:
 */
extern char *crypt();
extern int sys_nerr;
extern char *sys_errlist[];

/* general global data */
char crypted[100];/* New password */
char salt[3];			/* Salt of new password */
char *prog;			/* argv[0] for error messages */
capability origcap;		/* Original cap of Environ file */

/* Commandline options */
int dflag = 0, nflag = 0;
char *filename;
char fname[FN_LEN];
char tempname[FN_LEN + 3];
FILE *outfp = NULL, *infp = NULL;

/*
 *	Pick a random number and initialise salt[]
 */
void
get_salt()
{
    static char salt_chars[] =	/* According to the crypt manual */
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./";
    unsigned rnd;
    errstat err;

    err = rnd_getrandom((char *) &rnd, sizeof(int));
    if (err != STD_OK) {
	fprintf(stderr, "%s: random number generator out of order (%s)\n",
							prog, err_why(err));
	exit(1);
    }
    /* assert(sizeof (salt_chars) == 65); */
    salt[0] = salt_chars[rnd % 64];
    salt[1] = salt_chars[(rnd >> 8) % 64];
    salt[2] = '\0';
} /* get_salt */

/*
 *	Get input environ-file
 */
void
open_input()
{
    errstat err;
    if (strlen(filename) > FN_LEN) {
	fprintf(stderr, "%s: filename '%s' too long\n", prog, filename);
	exit(1);
    }
    errno = 0;
    err = name_lookup(filename, &origcap);
    if (err != STD_OK) {
	fprintf(stderr, "%s: cannot lookup %s (%s)\n",
			prog, filename, err_why(err));
	exit(1);
    }
    infp = fopen(filename, "r");	/* Open the inputfile */
    if (infp == NULL) {
	fprintf(stderr, "%s: %s: %s\n", prog, filename,
		(errno >0 && errno < sys_nerr) ?
		sys_errlist[errno] : "cannot read file");
	exit(1);
    }
} /* open_input */

/*
 *	Open intermediate output environ-file.
 */
void
open_output()
{
    errno = 0;
    /* avoid salt when constructing the tempname since it may contain a '/' */
    sprintf(tempname, "%s.xx", filename);
    outfp = fopen(tempname, "w");	/* Open the inputfile */
    if (outfp == NULL) {
	fprintf(stderr, "%s: %s: %s\nPassword not changed\n",
		prog, tempname,
		(errno >0 && errno < sys_nerr) ?
		sys_errlist[errno] : "cannot write file");
	exit(1);
    }
    if (dflag) fprintf(stderr, "Output: %s\n", tempname);
} /* open_output */

void
close_files()
{
    errno = 0;
    fclose(infp);
    if (errno) {
	fprintf(stderr, "%s: %s - Password not changed\n",
				filename, sys_errlist[errno]);
	return;
    }
    errno = 0;
    fclose(outfp);
    if (errno) {
	fprintf(stderr, "%s: cannot close %s (%s) - Password not changed\n",
	    prog, tempname[0], sys_errlist[errno]);
	return;
    }

    /* Move new file over old */
    errno = 0;
    rename(tempname, filename);
    if (errno)
	fprintf(stderr, "%s: cannot rename: %s\nPassword not changed\n",
					tempname, sys_errlist[errno]);
    else {
	if (dflag) fprintf(stderr, "%s: moved %s over %s\n",
				prog, tempname, filename);
	if (!nflag) {
	    errstat err;
	    err = std_destroy(&origcap);
	    if (err != STD_OK)
		fprintf(stderr,
			"%s (warning): could not destroy old %s (%s)\n",
			prog, filename, err_why(err));
	    else if (dflag) fprintf(stderr, "std_destroy(%s)\n", filename);
	}
    }
} /* close_files */


/*
 *	Read password twice: return 1 on success, 0 otherwise.
 */
int
get_pw()
{
    char	pass_copy[MAX_PASSWD_SZ + 1];
    char *	pass;
    int		retries;

    for (retries = 0;;)
    {
	pass = getpass("New password: ");
	strcpy(pass_copy, pass);
	pass = getpass("Retype password: ");
	if (strcmp(pass, pass_copy) != 0)
	{
	    printf("Mismatch. ");
	    if (++retries > 2)
	    {
		printf("Giving up.\n");
		return 0;
	    }
	    else
		printf("Try again.\n");
	}
	else
	{
	    strcpy(crypted, crypt(pass_copy, salt));
	    return 1;
	}
    }
    /*NOTREACHED*/
} /* get_pw */


/*
 *	Read the old file, writing the new one
 */
void
chpw()
{
    int saw_pw = 0;
    char line[1000];	/* That should be enough */
    while (fgets(line, sizeof(line), infp) != NULL) {
	char *p, *q;
	q = (p = line) + strlen(line);
	if (*--q != '\n') {
	    fprintf(stderr,
		"%s: input line buffer overflow\nPassword not changed\n", prog);
	    exit(1);
	}
	while (p < q && (*p == ' ' || *p == '\t'))
	    ++p;
	/* This a "passwd" line? */
	if (strncmp(p, PASSWORD, sizeof(PASSWORD) - 1))
	    fputs(line, outfp);	/* No, copy it */
	else if (saw_pw)
	    fprintf(stderr, "%s: stripping extra password\n");
	else {	/* Change the password */
	    saw_pw = 1;
	    p += sizeof(PASSWORD) - 1;
	    if (*p == ' ' || *p == '\t')
		do ++p; while (p < q && (*p == ' ' || *p == '\t'));
	    else *p++ = '\t';
	    strcpy(p, crypted);
	    strcat(p, "\n");
	    fputs(line, outfp);
	}
    }
    if (!saw_pw) {
	register int ch;
	fprintf(stderr, "Warning: there was no password in original\n");
	/* Re-read file prepending the passwd line */
	rewind(outfp);
	rewind(infp);
	fprintf(outfp, "%s %s\n", PASSWORD, crypted);
	while ((ch = getc(infp)) != EOF)
	    putc(ch, outfp);
    }
} /* chpw */

Usage()
{
    fprintf(stderr,
	"Usage: %s [-n] [file] - 'file' defaults to %s\n", prog, ENVIRONFILE);
    exit(2);
} /* Usage */

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int opt;
    char *pass;
    extern char *optarg;
    extern int optind;

    /* Setup: */
    prog = argc > 0 ? argv[0] : "CHPW";
    while ((opt = getopt(argc, argv, "dn")) != EOF) switch (opt) {
	case 'd':	/* Debug; undocumented on purpose */
	    ++dflag;
	    break;
	case 'n':	/* Don't destroy original */
	    ++nflag;
	    break;
	default:
	    Usage();
    }
    if (nflag > 1) Usage();

    /* The [optional] argument is the name of the file to edit */
    argc -= optind; argv += optind;
    switch (argc) {
    case 0:
	break;
    case 1:
	filename = argv[0];
	break;
    default:
	Usage();
	/*NOTREACHED*/
    }

    if (filename == 0)
    {
	/* We look at the ENVIRONFILE in the caller's root */
	fname[0] = '/';
	fname[1] = '\0';
	strcat(fname, ENVIRONFILE);
	filename = fname;
    }

    get_salt();
    open_input();
    pass = getpass("Old password: ");
    if (!verify_user(pass)) {
	fprintf(stderr, "Incorrect password for current user.\n\n");
	return 1;
    }

    /* Get the new password */
    if (!get_pw())
    {
	fprintf(stderr, "\nPassword not changed!\n");
	exit(1);
    }
    
    /* Update the Environ file */
    open_output();
    chpw();
    close_files();
    exit(0);
    /*NOTREACHED*/
} /* main */
