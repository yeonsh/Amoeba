/*	@(#)c2a.c	1.3	94/04/07 15:03:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	c2a [ -v ] cap...:
 *	Print ar-format of capabilities
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/name.h>
#include <module/ar.h>

extern int optind, opterr;
extern char *optarg;

#ifndef EOF
#define EOF -1
#endif

char Usage[] = "Usage: %s [-v] cap...\n";

main(argc, argv)
    int argc;
    char *argv[];
{
    char * prog;
    int opt;
    int vflag = 0;

    prog = argc > 0 ? argv[0] : "C2A";

    /* Parse commandline: */
    while ((opt = getopt(argc, argv, "v")) != EOF)
	switch (opt) {
	case 'v':
	    vflag = 1;
	    break;
	default:
	    fprintf(stderr, Usage, prog);
	    exit(2);
	}

    if ((argc -= optind) <= 0) {
	fprintf(stderr, Usage, prog);
	exit(2);
    }
    for (argv += optind; argc > 0; --argc, ++argv) {
	errstat err;
	capability cap;

	err = name_lookup(*argv, &cap);
	if (err == STD_OK) {
	    if (vflag)
		printf("%s\t%s\n", *argv, ar_cap(&cap));
	    else
		printf("%s\n", ar_cap(&cap));
	} else {
	    fprintf(stderr, "%s: lookup of '%s' failed: %s\n",
						prog, *argv, err_why(err));
	    exit(1);
	}
    }
    exit(0);
} /* main */
