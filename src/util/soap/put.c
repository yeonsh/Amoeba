/*	@(#)put.c	1.3	94/04/07 16:05:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Take a capability from standard output and store it in the
** directory server
*/
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

#include "stdio.h"
#include "stdlib.h"

int force_flag = 0;

main(argc, argv)
char **argv;
{
	char buf[512];
	capset cs;
	register n;
	int ncols;	/* number of columns for which we have masks */
	long cols[SP_MAXCOLUMNS];
	errstat err;

	if (argc == 3 && strcmp(argv[1], "-f") == 0) {
		argv[1] = argv[2];
		argv[2] = argv[3];
		force_flag = 1;
		argc--;
	}

	if (argc != 2) {
		fprintf(stderr, "Usage: %s [ -f ] pathname\n", argv[0]);
		exit(1);
	}
	if ((n = fread(buf, 1, sizeof (buf), stdin)) < 0) {
		fprintf(stderr, "%s: read error\n", argv[0]);
		exit(1);
	}
	if (buf_get_capset(buf, &buf[n], &cs) == 0) {
		fprintf(stderr, "%s: bad capability set\n", argv[0]);
		exit(1);
	}
	ncols = sp_mask(3, cols);	/* default # columns is 3 */

	err = (errstat) sp_append(SP_DEFAULT, argv[1], &cs, ncols, cols);
	if (force_flag && n == STD_EXISTS)
		err = (errstat) sp_replace(SP_DEFAULT, argv[1], &cs);

	if (err != STD_OK) {
		fprintf(stderr, "%s: can't append %s (%s)\n",
						argv[0], argv[1], err_why(err));
		exit(1);
	}
	exit(0);
}
