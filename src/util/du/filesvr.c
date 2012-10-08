/*	@(#)filesvr.c	1.4	94/04/07 15:05:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include <string.h>

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/direct.h"
#include "module/name.h"
#include "module/stdcmd.h"

#include "filesvr.h"
#include "du.h"
#include "pathmax.h"

extern char *strdup();

/*
 * Maintains information about the file servers
 */

struct filesvr *filesvr_list = NULL;
int n_file_svrs = 0;

void
add_file_server(name, report_name)
char *name;
char *report_name;
{
    /* add it to the list if the corresponding server is up,
     * and is not already present in the list
     */
    int i;
    errstat err;
    capability server_cap;

    /*
     * look it up
     */
    if ((err = name_lookup(name, &server_cap)) != STD_OK) {
	verbose(1, "cannot lookup %s (%s)\n", name, err_why(err));
	return;
    }

    /*
     * maybe we already have it in the list
     */
    for (i = 0; i < n_file_svrs; i++) {
	if (PORTCMP(&filesvr_list[i].fsvr_cap.cap_port,
		    &server_cap.cap_port)) {
	    verbose(opt_debug,
		    "file server %s already in list (ignored)", name);
	    return;
	}
    }

    /*
     * see if it responds
     */
    {
	int len;
	interval old;

	old = timeout(2*SMALL_TIMEOUT);
	err = std_info(&server_cap, (char *) NULL, 0, &len);
	(void) timeout(old);
	if (err != STD_OK && err != STD_OVERFLOW) {
	    verbose(opt_debug, "%s did not respond (%s)", name, err_why(err));
	    return;
	}
    }

    /*
     * add it to the list
     */
    n_file_svrs++;
    if (filesvr_list == NULL) {
	filesvr_list = (struct filesvr *)malloc(sizeof(struct filesvr));
    } else {
	filesvr_list = (struct filesvr *)realloc((_VOIDSTAR)filesvr_list,
			       (size_t)(n_file_svrs * sizeof(struct filesvr)));
    }
    if (filesvr_list == NULL) {
	fatal("Out of memory");
    }

    filesvr_list[n_file_svrs - 1].fsvr_name = strdup(report_name);
    filesvr_list[n_file_svrs - 1].fsvr_cap = server_cap;
}

void
lookup_file_servers(dir)
char *dir;
/* add the the file servers in dir to the list */
{
    struct dir_open *dp;
    char 	    *name, *slash, svr_path[PATH_MAX];
    capability	     dircap;
    errstat 	     err;

    strcpy(svr_path, dir);
    strcat(svr_path, "/");
    slash = svr_path + strlen(svr_path) - 1;
    
    if ((err = name_lookup(dir, &dircap)) != STD_OK) {
	fatal("could not look up `%s' (%s)", dir, err_why(err));
    }
    if ((dp = dir_open(&dircap)) == NULL) {
	fatal("could not open directory `%s'", dir);
    }
    while ((name = dir_next(dp)) != NULL) {
	strcpy(slash + 1, name);
	add_file_server(svr_path, name);
    }
    (void) dir_close(dp);
}
	
int
file_svr_index(filecap)
capability *filecap;
{
    int i;

    for (i = 0; i < n_file_svrs; i++) {
	if (PORTCMP(&filecap->cap_port,
		    &filesvr_list[i].fsvr_cap.cap_port)) {
	    return i;
	}
    }
    /* not found; return the last column indicating other file servers. */
    return n_file_svrs;
}
