/*	@(#)whoami.c	1.4	96/02/27 13:18:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	Who am I?
 *	Compares the current ROOT capability (a ka "/")
 *	with the capabilities in /public/users.
 *	All capabilities are searched.
 */

#include <amoeba.h>
#include <stdcom.h>
#include <cmdreg.h>
#include <stderr.h>
#include <ampolicy.h>
#include <module/direct.h>
#include <module/name.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _VOIDSTAR
#define	_VOIDSTAR	void *
#endif


/*
 *	Compare two capabilities, see if they point to the same object.
 *	This assumes that the rights and random part of the capability
 *	should not be compared; which is true for SOAP directories.
 *	In general, systems programmers who violate this convention
 *	should be shot, errr, fired :-)
 */
int
capequ(c1, c2)
    capability *c1, *c2;
{
    return PORTCMP(&c1->cap_port, &c2->cap_port) &&
	memcmp((_VOIDSTAR) c1->cap_priv.prv_object,
		(_VOIDSTAR) c2->cap_priv.prv_object,
		sizeof(c1->cap_priv.prv_object)) == 0;
} /* capequ */

main(argc, argv)
    int argc;
    char *argv[];
{
    capability *slash, udir, scratch;
    char *p; int n_found = 0;
    errstat err;
    struct dir_open *dp;

    switch (argc) {
    case 0:
	argv[0] = "WHOAMI";
	break;
    case 1:
	break;
    default:
	fprintf(stderr, "Usage: %s\n", argv[0]);
	exit(2);
    }

    slash = getcap("ROOT");
    if (slash == NULL) {
	fprintf(stderr, "%s: cannot find ROOT in capenv\n", argv[0]);
	exit(1);
    }

    if ((err = name_lookup(DEF_USERDIR, &udir)) != STD_OK) {
	fprintf(stderr, "%s: cannot lookup %s (%s)\n",
	    argv[0], DEF_USERDIR, err_why(err));
	exit(1);
    }

    if ((dp = dir_open(&udir)) == NULL) {
	fprintf(stderr, "%s: dir_open %s failed\n",
	    argv[0], DEF_USERDIR);
	exit(1);
    }
    while ((p = dir_next(dp)) != NULL) {
	if ((err = dir_lookup(&udir, p, &scratch)) != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup %s in %s (%s)\n",
		argv[0], p, DEF_USERDIR, err_why(err));
	    exit(1);
	}
	if (capequ(slash, &scratch)) {
	    printf(n_found ? " %s" : "%s", p);
	    ++n_found;
	}
    }
    (void)dir_close(dp);
    if (n_found) putchar('\n');
    else {
	fprintf(stderr, "%s: you are not a user\n", argv[0]);
	exit(1);
    }
    exit(0);
}
