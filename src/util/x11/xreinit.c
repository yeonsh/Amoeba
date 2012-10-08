/*	@(#)xreinit.c	1.4	96/02/27 13:18:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <ampolicy.h>
#include <stderr.h>
#include <stdio.h>
#include <stdlib.h>
#include <module/name.h>
#include <server/x11/Xamoeba.h>

char xname[1000];
capability xcap;
char *ProgName;
int exitst;


void
do_reinit(svr)
    char*svr;
{
    errstat rv;

    sprintf(xname, "%s/%s", DEF_XSVRDIR, svr);
    if (name_lookup(xname, &xcap) < 0 &&
			    (rv=name_lookup(svr, &xcap)) < 0) {
	fprintf(stderr, "%s: lookup %s: %s\n", ProgName, svr, err_why(rv));
	exitst = 1;
	return;
    }
    if ((rv=x11_reinit(&xcap)) < 0) {
	exitst = 1;
	fprintf(stderr, "%s: reinit %s: %s\n", ProgName, svr, err_why(rv));
    }
}


main(argc, argv)
    int argc;
    char **argv;
{
    int i;
    char *server;

    ProgName = argv[0];
    if (argc == 1) {
	if (server=getenv("DISPLAY")) {
	    do_reinit(server);
	    exit(0);
	} else {
	    fprintf(stderr, "%s: No argument and $DISPLAY not set\n", ProgName);
	    exit(1);
	}
    }
    for(i=1; i<argc; i++) {
	do_reinit(argv[i]);
    }
    exit(0);
}
