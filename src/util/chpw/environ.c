/*	@(#)environ.c	1.2	96/02/27 13:08:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Peter Keuning, May 1994.
 *	Originate from ``login'', changed for ``chpw''.
 *	(Only partly needed).
 */
#include <amtools.h>
#include <ampolicy.h>
#include <caplist.h>

/* tunable constants */
#define	MAX_ARGUMENTS	20
#define	MAX_LINE	2048

/* manifest constants */
#define	TRUE		1
#define	FALSE		0

#define	is_blank(ch)	((ch)==' ' || (ch)=='\t' || (ch)=='\n' || (ch)=='\r')

int debug = FALSE;			/* no debug output to begin with */

static FILE	*open_environ();
static void	close_environ();
static int	read_line();

/*
 * Get password entry from /Environ file, and login.
 * No passwd entry means, unfortunately, access.
 */
static int verify_password(/* passwd, encrypted */);

int
verify_user(pass)
    char *pass;
{
    char *argvec[MAX_ARGUMENTS];
    FILE *fp;

    if ((fp = open_environ()) == NULL)
	return FALSE;
    while (read_line(fp, argvec)) {
	/* format: passwd [<encrypted-password>] */
	if (strcmp(argvec[0], "passwd") == 0) {
	    if (!argvec[2]) {
		close_environ(fp);
	        return argvec[1] ? verify_password(pass, argvec[1]) : TRUE;
	    }
	}
    }
    close_environ(fp);
    (void)verify_password(pass, "\0\0\0\0\0\0\0\0\0\0\0\0");
    return FALSE;
}

static int
verify_password(passwd, encrypted)
    char *passwd, *encrypted;
{
    char salt[3], *crypt();

    salt[0] = encrypted[0];
    salt[1] = encrypted[1];
    salt[2] = '\0';
    return strcmp(encrypted, crypt(passwd, salt)) == 0;
}


/*
 * Open /Environ file to read password
 * and (eventually) its environment from.
 */
static FILE *
open_environ()
{
    char environ[BUFSIZ];
    FILE *fp;

    strncpy(environ, "/", BUFSIZ);
    strncat(environ, ENVIRONFILE, BUFSIZ);
    if ((fp = fopen(environ, "r")) == NULL && debug)
	fprintf(stderr, "Cannot open environment file (%s)\n", environ);
    return fp;
}

static void
close_environ(fp)
    FILE *fp;
{
    fclose(fp);
}

/*
 * Read Environment line. Each line contains a function specifier
 * followed by an optional number of argument. When a hash (#) appears
 * at the beginning of a line, it remained is taken as comment.
 */
static int
read_line(fp, argv)
    FILE *fp;
    char **argv;
{
    static char line[MAX_LINE];
    int ch, n, i;

    /* zero argument vector, to prevent races */
    for (i = 0; i < MAX_ARGUMENTS; i++)
	argv[i] = NULL;

    /* skip comment */
    while ((ch = getc(fp)) == '#') {
	do
	    ch = getc(fp);
	while (ch != '\n' && ch != EOF);
	if (ch == EOF) return FALSE;
    }
    if (ch == EOF) return FALSE;

    /* parse line, storing each argument in argv */
    i = n = 0;
    while (ch != '\n' && ch != EOF) {
	while (ch == ' ' || ch == '\t')
	    ch = getc(fp);
	if (ch == '\n' || ch == EOF) break;
	if (n >= MAX_ARGUMENTS - 1)
	    return FALSE;
	argv[n++] = &line[i];
	while (ch != EOF && !is_blank(ch)) {
	    if (i >= MAX_LINE - 1)
		return FALSE;
	    line[i++] = ch;
	    ch = getc(fp);
	}
	line[i++] = '\0';
    }
    argv[n] = NULL;
    return TRUE;
}
