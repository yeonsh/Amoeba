/*	@(#)mkd.c	1.4	96/02/27 13:14:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include "soap/soap.h"

#include "stdio.h"
#include "stdlib.h"


extern int   getopt();
extern int   optind;
extern char *optarg;

static void
usage(prog)
char *prog;
{
    fprintf(stderr, "Usage: %s [-c column_name]* name ...\n", prog);
    exit(1);
}

main(argc, argv)
int argc;
char **argv;
{
    char   *col_names[SP_MAXCOLUMNS + 1];
    int	    opt;
    int	    i;
    int	    ncols;
    errstat err;
    int     sts;

    /* Get the names for the columns (if any), specified by the
     * '-c' options.
     */
    ncols = 0;
    while ((opt = getopt(argc, argv, "c:?")) != EOF) {
	switch (opt) {
	case 'c':
	    if (ncols >= SP_MAXCOLUMNS) {
		fprintf(stderr, "%s: too many columns (max %d)\n",
			argv[0], SP_MAXCOLUMNS);
		exit(2);
	    }
	    col_names[ncols++] = optarg;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    break;
	}
    }
    /* The list of column names must be NULL terminated */
    col_names[ncols] = NULL;

    /* The remaining arguments (at least one) are the names for the
     * directories.
     */
    if (optind >= argc) {
	usage(argv[0]);
    }
	
    sts = 0;
    for (i = optind; i < argc; ++i) {
	/* If no column names were specified, give sp_mkdir() a
	 * third NULL argument, telling it to create a default
	 * directory with 3 columns.
	 */
	err = sp_mkdir(SP_DEFAULT, argv[i],
		       (ncols > 0) ? col_names : (char **)NULL);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: cannot create directory %s (%s)\n",
		    argv[0], argv[i], err_why(err));
	    sts = 1;
	}
    }
    exit(sts);
}
