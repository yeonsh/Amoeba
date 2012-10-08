/*	@(#)del.c	1.3	94/04/07 16:05:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** Utility to delete one or more directory entries
*/
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "module/stdcmd.h"

/* Getopt(): */
extern int optind;
extern char *optarg;

usage(prog)
char * prog;
{
	fprintf(stderr, "Usage: %s [-df] name...\n", prog);
	exit(2);
}

main(argc, argv)
int argc;
char **argv;
{
	static capset cs;
	register n, status = 0;
	int dflag = 0, fflag = 0;
	char *prog;
	int opt;
	errstat err;

	prog = *argv;

	/* Parse option string */
	while ((opt = getopt(argc, argv, "df")) != EOF)
		switch (opt) {
		case 'd':
			dflag = 1; /* Require this if object is a directory,
				    * to avoid accidents */
			break;
		case 'f':
			fflag = 1; /* Force a destroy of the objects denoted
				    * by the capset, instead of relying on
				    * eventual garbage collection.
				    */
			break;
		default:
			usage(prog);
			break;
		}

	argv += optind;
	if (*argv == NULL)
		usage(prog);

	do {
		interval timeout(), savetimeout;

		if (!dflag || fflag) {
			err = sp_lookup(SP_DEFAULT, *argv, &cs);
			if (err != STD_OK) {
				fprintf(stderr, "%s: can't lookup %s (%s)\n",
						prog, *argv, err_why(err));
				status = 1;
				continue;
			}
		}
		if (!dflag) {
			/* Ensure that the object is not a directory: */
			capability cap;
			char buf[10];
			int  len;

			savetimeout = timeout((interval) 5000);
			if ((err = cs_goodcap(&cs, &cap)) == STD_OK) {
			    err = std_info(&cap, buf, (int) sizeof(buf), &len);
			    if ((err == STD_OK || err == STD_OVERFLOW) &&
			        (len > 0) && (buf[0] == '/'))
			    {
				/* It's a directory.  Must give -d flag: */
				fprintf(stderr,
				 "%s: %s is a directory (use -d to override)\n",
						prog, *argv);
				status = 1;
				(void) timeout(savetimeout);
				continue;
			    }
			}
			if (err == RPC_NOTFOUND) {
				/* It may be a directory.  Must give -d flag: */
				fprintf(stderr,
			  "%s: can't get type of %s (%s). Use -d to override\n",
						prog, *argv, err_why(err));
				status = 1;
				(void) timeout(savetimeout);
				continue;
			}
		}
		if (fflag) {
			/* Destroy all the objects specified by the capset: */
			int count;

			/* Count the number of capabilities: */
			for (n = count = 0; n < cs.cs_final; n++)
				if (cs.cs_suite[n].s_current)
					count++;

			savetimeout = timeout((interval) 5000);
			err = cs_purge(&cs);
			(void) timeout(savetimeout);

			if (err != STD_OK) {
				int newcount;

				/* Re-count the number of capabilities: */
				for (n = newcount = 0; n < cs.cs_final; n++)
					if (cs.cs_suite[n].s_current)
						newcount++;

				if (newcount == count)
					fprintf(stderr,
						"%s: can't destroy %s (%s)\n",
						prog, *argv, err_why(err));
				else
					fprintf(stderr,
			"%s: Warning: not all replicas of %s destroyed (%s)\n",
						prog, *argv, err_why(err));
				status = 1;
			}
		}
		if ((n = sp_delete(SP_DEFAULT, *argv)) != STD_OK) {
			fprintf(stderr, "%s: can't delete entry %s (%s)\n",
					    prog, *argv, err_why((errstat) n));
			status = 1;
		}
	} while (*++argv != 0);
	exit(status);
}
