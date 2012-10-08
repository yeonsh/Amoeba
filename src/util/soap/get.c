/*	@(#)get.c	1.3	94/04/07 16:05:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** look up a capability and writes it on standard output
*/
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

#include "stdio.h"
#include "stdlib.h"

main(argc, argv)
int argc;
char **argv;
{
	char buf[512];
	char *p;
	capset cs;
	errstat n;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s pathname\n", argv[0]);
		exit(1);
	}
	if ((n = (errstat) sp_lookup(SP_DEFAULT, argv[1], &cs)) != STD_OK) {
		fprintf(stderr, "%s: can't lookup %s (%s)\n",
						argv[0], argv[1], err_why(n));
		exit(1);
	}
	if ((p = buf_put_capset(buf, &buf[512], &cs)) == 0) {
		fprintf(stderr, "%s: can't put capset\n", argv[0]);
		exit(1);
	}
	(void) fwrite(buf, 1, (unsigned) (p - buf), stdout);
	exit(0);
}
