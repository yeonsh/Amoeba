/*	@(#)dir_info.c	1.2	94/04/07 15:04:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "_ARGS.h"
#include "amoeba.h"
#include "capset.h"

#include "dir_info.h"
#include "filesvr.h"
#include "du.h"

/*
 * directory contents maintenance
 */

static struct dir_info *dir_first = NULL;

static int
capset_equal(cs1, cs2)
capset *cs1, *cs2;
/* Assume two capsets are equal when at least one valid capability of the
 * first one is a member of the second one.  Returns 1 if equal, 0 otherwise.
 */
{
    int i;

    for (i = 0; i < cs1->cs_final; i++) {
	if (cs1->cs_suite[i].s_current &&
	    (cs_member(cs2, &cs1->cs_suite[i].s_object) == 1)) {
	    return 1;
	}
    }
    return 0;
}

static struct dir_info *
new_dir_info()
{
    register struct dir_info *newdir;

    newdir = (struct dir_info *)malloc(sizeof(struct dir_info));
    if (newdir == NULL) {
	fatal("Out of memory");
    }

    /* initialise */
    newdir->dir_flags = 0;
    newdir->n_sub_dirs = 0;
    newdir->n_files = 0;
    newdir->fsizes = (long *)calloc((size_t)(n_file_svrs + 1), sizeof(long));
    newdir->cumfsizes = (long *)calloc((size_t)(n_file_svrs + 1),sizeof(long));
    newdir->dir_name = "";	/* let caller deal with it */
    newdir->dir_parent = newdir->dir_childs = newdir->dir_sister = NULL;
    return newdir;
}

void
add_dir_to_list(newdir)
struct dir_info *newdir;
{
    newdir->dir_next = dir_first;
    dir_first = newdir;
}

struct dir_info *
search_dir(dircap)
capset *dircap;
/*
 * If the directory with capset *dirp is on the list, return a pointer
 * to the existing one.  Otherwise add a new one to the list and return it.
 */
{
    register struct dir_info *dirp, *newdir;

    for (dirp = dir_first; dirp != NULL; dirp = dirp->dir_next) {
	if (capset_equal(dircap, &dirp->dir_cap)) {
	    return dirp;
	}
    }

    /* not found; allocate new one */
    newdir = new_dir_info();

    if (cs_copy(&newdir->dir_cap, dircap) != 1) {
	fatal("Out of memory");
    }

    /* add to front of directory list */
    add_dir_to_list(newdir);

    return(newdir);
}

struct dir_info *
search_dir_by_cap(dircap)
capability *dircap;
/*
 * If the directory with capability dircap is on the list, return a pointer
 * to the existing one. Otherwise return NULL.
 */
{
    register struct dir_info *dirp;

    for (dirp = dir_first; dirp != NULL; dirp = dirp->dir_next) {
	if (cs_member(&dirp->dir_cap, dircap)) {
	    return dirp;
	}
    }
    /* not found */
    return NULL;
}
