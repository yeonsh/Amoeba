/*	@(#)whoami.c	1.4	96/03/26 11:29:39 */
/*
 *	Who am I?
 *	Compares the current ROOT capability (a ka "/")
 *	with the capabilities in DEF_USERDIR
 *	All capabilities are searched.
 */

#include "amoeba.h"
#include "stdcom.h"
#include "cmdreg.h"
#include "stderr.h"
#include "ampolicy.h"
#include "module/direct.h"
#include "module/name.h"
#include "stdio.h"


/*
 *	Compare two capabilities, see if they point to the same object.
 *	This assumes that the rights and random part of the capability
 *	should not be compared; which is true for SOAP directories.
 *	In general, systems programmers who violate this convention
 *	should be shot, errr, fired :-)
 */
int
capequ(c1, c2)
    capability *c1, *c2;
{
    return PORTCMP(c1, c2) &&
	memcmp( c1->cap_priv.prv_object,
		c2->cap_priv.prv_object,
		sizeof(c1->cap_priv.prv_object)) == 0;
} /* capequ */

char *whoami()
{
    capability *slash, udir, scratch;
    char *p; int n_found = 0;
    errstat err;
    struct dir_open *dp;
    static struct passwd *pw;
    

    slash = getcap("ROOT");
    if (slash == NULL) {
	fprintf(stderr, "whoami: cannot find ROOT in capenv\n");
	exit(1);
    }

    if ((err = name_lookup(DEF_USERDIR, &udir)) != STD_OK) {
	fprintf(stderr, "whoami: cannot lookup %s (%s)\n",
	    DEF_USERDIR, err_why(err));
	exit(1);
    }

    if ((dp = dir_open(&udir)) == NULL) {
	fprintf(stderr, "whoami: dir_open %s failed\n",
	    DEF_USERDIR);
	exit(1);
    }
    while (!n_found && (p = dir_next(dp)) != NULL) {
	if ((err = dir_lookup(&udir, p, &scratch)) != STD_OK) {
	    fprintf(stderr, "whoami: cannot lookup %s in %s (%s)\n",
		p, DEF_USERDIR, err_why(err));
	    exit(1);
	}
	if (capequ(slash, &scratch)) {
	    ++n_found;
	}
    }
    (void)dir_close(dp);
    if (n_found) {
	return(p);
    } else {
	fprintf(stderr, "whoami: you are not a user\n") ;
	exit(1);
	/*NOTREACHED*/
    }
}
