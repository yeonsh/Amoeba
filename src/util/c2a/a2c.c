/*	@(#)a2c.c	1.4	94/04/07 15:03:39 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/name.h>
#include <module/ar.h>


int
main(argc, argv)
    int argc;
    char *argv[];
{
    capability cap;
    char *s;
    errstat err;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s cap_in_string_fmt newcap\n", argv[0]);
	exit(2);
    }

    s = ar_tocap(argv[1], &cap);
    if (s == NULL || *s != '\0') {
	fprintf(stderr, "'%s' is not in legal format\n", argv[1]);
	if (s != NULL) fprintf(stderr, "Leftover: %s\n", s);
	exit(2);
    }

    if (strcmp(argv[2], "-")) {
	err = name_append(argv[2], &cap);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: append of '%s' failed: %s\n",
					argv[0], argv[2], err_why(err));
	    exit(1);
	}
    } else
	fwrite((char *)&cap, sizeof(cap), 1, stdout);

    exit(0);
} /* main */
