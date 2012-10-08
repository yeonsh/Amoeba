/*	@(#)chmod.c	1.5	94/04/07 10:24:20 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Posix chmod() function */

#include "ajax.h"
#include "unistd.h" /* For R_OK etc. */
#include "cmdreg.h"
#include "server/soap/soap.h"

#define HAS_MODE(mode, col, mask) (((mode)>>(6-3*(col)) & (mask)) == (mask))

#define NCOLS 3

#define SOAP_W_MODE		((long) 0xf0)
#define SOAP_R_MODE(col)	(1L << (col))

#define BULLET_W_MODE		((long)(BS_RGT_ALL & ~BS_RGT_READ))
#define BULLET_R_MODE		((long)BS_RGT_READ)

#ifdef __STDC__
int chmod(char *path, mode_t mode)
#else
int chmod(path, mode) char *path; int mode;
#endif
{
	capset csroot, csdir, csobject;
	const char *name;
	errstat err;
	capability object;
	b_fsize size;
	long cols[NCOLS];
	int i;
	
	for (i = 0; i < NCOLS; i++) {
		cols[i] = 0;
	}

	/* Determine root or start directory */
	if ((err = _ajax_csorigin(NILCAP, path, &csroot)) != STD_OK)
		ERR(_ajax_error(err), "chmod: csorigin failed");
	
	/* Look up object's parent directory */
	name = path;
	err = sp_traverse(&csroot, (const char **) &name, &csdir);
	cs_free(&csroot);
	if (err != STD_OK)
		ERR(_ajax_error(err), "chmod: sp_traverse failed");
	
	/* Look up the object */
	if ((err = sp_lookup(&csdir, name, &csobject)) != STD_OK) {
		cs_free(&csdir);
		ERR(_ajax_error(err), "chmod: sp_lookup failed");
	}
	
	/* Turn object capset into object capability */
	(void)cs_to_cap(&csobject, &object);
	cs_free(&csobject);
	
	/* Change the masks, depending on the object type */
	if ((err = b_size(&object, &size)) == STD_OK) {
		/* It's a bullet file */
		for (i = 0; i < NCOLS; i++) {
			if (HAS_MODE(mode, i, W_OK))
				cols[i] |= BULLET_W_MODE;
			if (HAS_MODE(mode, i, R_OK))
				cols[i] |= BULLET_R_MODE;
		}
	}
	else if (_is_am_dir(&object)) {
		/* It's a directory */
		for (i = 0; i < NCOLS; i++) {
			if (HAS_MODE(mode, i, W_OK))
				cols[i] |= SOAP_W_MODE;
			/* XXX Make column readable only if r *and* x perm */
			if (HAS_MODE(mode, i, R_OK|X_OK))
				cols[i] |= SOAP_R_MODE(i);
		}
		/* If we have a right on column 0, add the rights
		 * for all columns (not just the first 3)
		 * This will turn it into an owner cap, when possible.
		 */
		if (cols[0] & SOAP_R_MODE(0)) {
			cols[0] |= SP_COLMASK;
		}
	}
	else {
		/* It's neither -- user should use std_restrict instead */
		cs_free(&csdir);
		ERR(EPERM, "chmod: neither bullet file nor directory");
		/* XXX questionable errno value */
	}

	/* The rights in column j > i should be included in the
	 * rights for users having access to column i:
	 */
	for (i = NCOLS - 1; i > 0; i--) {
		cols[i - 1] |= cols[i];
	}
	
	/* Set new masks */
	err = sp_chmod(&csdir, name, NCOLS, cols);
	cs_free(&csdir);
	if (err != STD_OK)
		ERR(_ajax_error(err), "chmod: sp_chmod failed");
	
	/* Success */
	return 0;
}
