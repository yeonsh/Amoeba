/*	@(#)makepool.c	1.1	96/02/27 12:40:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "amoeba.h"
#include "ampolicy.h"
#include "capset.h"
#include "module/name.h"
#include "module/stdcmd.h"
#include "cmdreg.h"
#include "stderr.h"
#include "class/runrights.h"
#include "class/run.h"
#include "_ARGS.h"
#include "server/soap/soap.h"

#define DEF_RUNNAME	".run"


char *progname = "makepool";

void
usage()
{
    fprintf(stderr, "usage: %s [-s runserver] [-p publishname] pooldir\n\n",
	    progname);
    fprintf(stderr, "default runserver is \"%s\"\n", DEF_RUNSERVER);
    fprintf(stderr, "default publishing name is \"<pooldir>/%s\"\n",
	    DEF_RUNNAME);
    exit(1);
}

errstat
publish(dircap, name, object)
capability *dircap;
char *name;
capability *object;
{
    static long pool_mask[] = {
	RUN_RGT_ALL,
	RUN_RGT_FINDHOST | RUN_RGT_STATUS, /* i.e., allow others to use it */
	RUN_RGT_FINDHOST | RUN_RGT_STATUS
    };
    capset dir_cs, obj_cs;
    errstat err;

    if (!cs_singleton(&dir_cs, dircap)) {
	return STD_NOSPACE;
    }
    if (!cs_singleton(&obj_cs, object)) {
	cs_free(&dir_cs);
	return STD_NOSPACE;
    }

    err = sp_append(&dir_cs, name, &obj_cs,
		    sizeof(pool_mask)/sizeof(pool_mask[0]), pool_mask);
    /* if the sp_append fails, don't try to "fix" this by doing a sp_replace()
     * instead, because the entry present may well be the capability of
     * an existing pool object.
     */
    cs_free(&dir_cs);
    cs_free(&obj_cs);
    return err;
}

main(argc, argv)
int argc;
char *argv[];
{
    extern int optind;
    extern char *optarg;

    capability	server_cap;	/* the run server */
    char       *server_path = DEF_RUNSERVER;
    capability	pooldir_cap;	/* the pool directory */
    char       *pooldir_path;
    capability	publish_cap;	/* run server pool object */
    char       *publish_name;
    char       *publish_path = NULL;
    capability  publish_dir_cap;
    int		opt;
    errstat	err;

    progname = argv[0];

#   define fatal(list)	{ fprintf(stderr, "%s: ", progname);\
			  fprintf list; exit(1); }

    while ((opt = getopt(argc, argv, "s:p:?")) != EOF) {
	switch (opt) {
	case 's':
	    server_path = optarg;
	    break;
	case 'p':
	    publish_path = optarg;
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
    }

    /* last argument should be pooldir */
    if (argc - optind == 1) {
	pooldir_path = argv[optind];
    } else {
	usage();
    }

    /* lookup pool directory */
    if ((err = name_lookup(pooldir_path, &pooldir_cap)) != STD_OK) {
	/* we could try to create a directory if it does not yet exist */
	fatal((stderr, "cannot lookup pool directory %s (%s)",
	       pooldir_path, err_why(err)));
    }
    
    /* publishing name not given, so set to default */
    if (publish_path == NULL) {
        publish_path = (char *)malloc((size_t)(strlen(pooldir_path) + 1 +
					       strlen(DEF_RUNNAME) + 1));
        if (publish_path == NULL) {
	    fatal((stderr, "out of memory"));
        }
        (void) sprintf(publish_path, "%s/%s", pooldir_path, DEF_RUNNAME);
    } 
    publish_name = name_breakpath(publish_path, &publish_dir_cap);
    if (publish_name == NULL) {
	fatal((stderr, "\"%s\" is not a valid publish path\n", publish_path));
    }

    /* lookup run server */
    if ((err = name_lookup(server_path, &server_cap)) != STD_OK) {
	fatal((stderr, "cannot lookup run server %s (%s)\n",
	       server_path, err_why(err)));
    }

    /* create pool capability */
    err = run_create(&server_cap, &pooldir_cap, &publish_cap);
    if (err != STD_OK) {
	fatal((stderr, "runsvr %s did not create pool cap (%s)\n",
	       server_path, err_why(err)));
    }

    /* publish pool capability */
    err = publish(&publish_dir_cap, publish_name, &publish_cap);
    if (err != STD_OK) {
	(void) std_destroy(&pooldir_cap);
	fatal((stderr, "cannot publish capability in %s (%s)\n",
	       publish_path, err_why(err)));
    }

    exit(0);
}
