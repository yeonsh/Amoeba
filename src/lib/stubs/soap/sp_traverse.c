/*	@(#)sp_traverse.c	1.3	94/04/07 11:10:27 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"

errstat
sp_traverse(dir, path, last)
capset      *dir;
const char **path;
capset      *last;
{
    /* NOTE: This function will return, in the *path argument, an unnormalized
     * "." or "..", if that was the last or only component of the input path.
     * This is correct, since this function is used only when the path name is
     * logically specifying a directory and the name of an entry (in that
     * directory) that is to be modified or have its attributes returned.
     * 
     * E.g. The path "/foo/bar/.." is supposed to refer to the (virtual) ".."
     * entry in directory "/foo/bar", not to "/foo", and similarly for paths
     * ending in ".".  If you don't see the difference, consider that
     * "del -d /foo/." should return an error (can't delete "." from a
     * directory), while "del -d /foo" should actually delete the directory
     * /foo.  They are not equivalent in UNIX.
     */
    register char *q, *end;
    register const char *p;
    char    buf[PATH_MAX+1];
    char   *slash;
    errstat err;

    p = *path;
    q = buf;
    slash = NULL;
    end = &buf[sizeof(buf)];

    while (q < end && (*q = *p++) != '\0') {
	if (*q++ == '/') {
	    *path = p;		/* Point to last component in input arg */
	    slash = q - 1;	/* Point to rightmost '/' in buf */
	}
    }
    if (q >= end) {
	return STD_NOSPACE;
    }

    if (slash != NULL) {
	/* There was a "/" in the name; now look up the prefix. */
	if (slash == buf) {	/* Empty prefix, so look up "/" */
	    err = sp_lookup(dir, "/", last);
	} else {
	    *slash = '\0';	/* Change rightmost '/' in buf to a '\0' */
	    err = sp_lookup(dir, buf, last);
	}
    } else {
	/* Have simple name without any "/".  Use the given directory, or
	 * current working directory, if SP_DEFAULT specified:
	 */
	if (dir == SP_DEFAULT) {
	    err = sp_get_workdir(last);
	} else {
	    err = cs_copy(last, dir) ? STD_OK : STD_NOSPACE;
	}
    }

    return err;
}
