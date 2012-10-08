/*	@(#)getdelput.c	1.3	94/04/07 16:05:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Getdelput
**
**	This program is primarily intended for use by the boot server to
**	install the time of day server and random server capabilities at
**	boot time.  It replaces the capability specified by its second
**	argument with that of its first.
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
char *argv[];
{
    capset cs;
    register n;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s src-pathname dst-pathname\n", argv[0]);
	exit(1);
    }
    if ((n = sp_lookup(SP_DEFAULT, argv[1], &cs)) != STD_OK) {
	fprintf(stderr, "%s: can't lookup %s (%s)\n",
				    argv[0], argv[1], err_why((errstat) n));
	exit(1);
    }
    if ((n = sp_replace(SP_DEFAULT, argv[2], &cs)) != STD_OK) {
	fprintf(stderr, "%s: can't replace %s (%s)\n",
				    argv[0], argv[2], err_why((errstat) n));
	exit(1);
    }
    exit(0);
}
