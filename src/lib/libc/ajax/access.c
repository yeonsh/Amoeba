/*	@(#)access.c	1.8	96/02/27 11:04:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* access(2) system call emulation
 *
 * Valid modes are
 *   F_OK:	just check if the object exists.  It need not be a file or
 *		directory so long as it is there.
 * or a combination of
 *   R_OK:	check that it has right RGT_READ
 *   X_OK:	same as R_OK (it could be a shell script, so there's
 *		no easy way to find if it really is executable).
 *   W_OK:	check if directory (!) can be modified (see below).
 */

#include "ajax.h"
#include "stdrights.h"
#include <unistd.h>
#define _POSIX_SOURCE
#include <limits.h>
#include <module/path.h>

int
access(file, needmode)
	char *file;
	int needmode;
{
    char	infobuf[30]; /* to receive result of stdinfo() in */
    capability	cap;
    int		len;
    errstat	err;
	
    /* Check the arguments - first the mode */
    if ((needmode != F_OK) && !(needmode & (R_OK|X_OK|W_OK))) {
	ERR(EINVAL, "access: invalid mode argument");
    }

    /*
     * The `mode' as set by _ajax_lookup doesn't give any info
     * (it is always the same constant value).
     * First try to lookup the full pathname.
     * If this fails, none of the access modes will be available,
     * so an error may be returned.
     */

    if (_ajax_lookup(NILCAP, file, &cap, (int *)NULL, (long *)NULL) != STD_OK) {
	ERR(ENOENT, "access: file not found");
    }

    /* Check existence of the object */
    infobuf[0] = '\0';
    err = std_info(&cap, infobuf, (int) sizeof(infobuf), &len);
    if (err != STD_OK && err != STD_OVERFLOW) {
	ERR(EACCES, "access: file not available");
    }

    if (needmode & (R_OK|X_OK)) {
	/* directories require special treatment */
	if (_is_am_dir(&cap)) {
	    if ((cap.cap_priv.prv_rights & SP_COLMASK) == 0) {
	        ERR(EACCES, "access: cannot open directory");
	    }
	} else {
	    /* Now we are assuming that the object is a file! */
	    if ((cap.cap_priv.prv_rights & RGT_READ) == 0) {
	        ERR(EACCES, "access: cannot read file");
	    }
	    if ((needmode & X_OK) && _aj_xbit(&cap) == 0) {
		ERR(EACCES, "access: file not executable");
	    }
	}
    }

    if (needmode & W_OK) {
	capability parent;
	capability *dir;
	register long rights;
	
	/* access() is mostly used to see if an file operation would work
	 * if it were tried. Therefore there isn't much sense in just checking
	 * RGT_MODIFY, because files created with the current implementation
	 * of stdio just create and install a new version of the (bullet)file.
	 * Therefore we do the following:
	 *	- if the path is itself a directory: check that it
	 *	  can be modified
	 *	- otherwise check that the directory containing it
	 *	  can be modified.
	 */
	    
	if (infobuf[0] == '/') { /* assume path leads to a soap object */
	    dir = &cap;
	} else {
	    capability *orig;
	    char namebuf[PATH_MAX];

	    /* Find the object's parent. We have to normalise path before
	     * traversing until the one but last entry, because it might
	     * contain "."'s and ".."'s.
	     * We're not interested in the last component (returned by
	     * _ajax_breakpath), just as long as it's not 0.
	     */
	    if ((orig = _ajax_origin(NILCAP, file)) != 0 &&
		(path_capnorm(&orig, file, namebuf) >= 0) &&
		_ajax_breakpath(orig, namebuf, &parent) != 0) {
		dir = &parent;
	    } else {
		ERR(EACCES, "access: cannot find parent directory");
	    }
	}
	
	rights = dir->cap_priv.prv_rights & PRV_ALL_RIGHTS;

	/* We must have access to at least one of the columns
	 * and we need SP_MODRGT on the directory.
	 */
	if ((rights & SP_COLMASK) == 0 || (rights & SP_MODRGT) != SP_MODRGT) {
	    ERR(EACCES, "access: cannot modify directory");
	}
    }

    /* passed all checks */
    return 0;
}
