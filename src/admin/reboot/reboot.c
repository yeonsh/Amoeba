/*	@(#)reboot.c	1.6	96/02/27 10:17:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * reboot.c
 */
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "stdio.h"
#include "stdlib.h"
#include "module/name.h"
#include "module/systask.h"
#include "module/host.h"

char *program;

extern int optind;
extern char *optarg;


void
usage()
{
    fprintf(stderr, "Usage: %s [-a args] kernel machine ...\n", program);
    exit(1);
}

void
reboot(kernel, commandline, machine)
    char *kernel, *commandline, *machine;
{
    capability mcap, kcap;
    errstat err;

    /* see if kernel exists */
    if ((err = name_lookup(kernel, &kcap)) != STD_OK) {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
	    program, kernel, err_why(err));
	exit(1);
    }

    /* see if machine exists */
    if ((err = syscap_lookup(machine, &mcap)) != STD_OK) {
	fprintf(stderr, "%s: directory lookup of '%s' failed: %s\n",
	    program, machine, err_why(err));
	exit(1);
    }

    /* boot ``kernel'' at ``machine'' */
    if ((err = sys_boot(&mcap, &kcap, commandline, 0)) != STD_OK) {
	fprintf(stderr, "%s: sys_boot failed: %s\n",
	    program, err_why(err));
	exit(1);
    }
}


main(argc, argv)
    int argc;
    char **argv;
{
    char *kernel;
    char *kernelargs;
    char commandline[BUFSIZ];
    int	 opt;

    program = argv[0];
    kernelargs = "";
    while ((opt = getopt(argc, argv, "a:")) != EOF) {
	switch (opt) {
	case 'a':
	    kernelargs = optarg;
	    break;
	default:
	    usage();
	}
    }
    if (argc < optind + 2)
	usage();

    kernel = argv[optind++];
    sprintf(commandline, "%s %s", kernel, kernelargs);

    while (optind < argc)
	reboot(kernel, commandline, argv[optind++]);

    exit(0);
}
