/*	@(#)ajlookup.c	1.6	96/02/27 11:05:05 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Look up a name and return its capability, (mode) and mtime */

#include "ajax.h"
#include "module/path.h"

errstat
_ajax_lookup(dir, path, pcap, pmode, pmtime)
	capability *dir;
	const char *path;
	capability *pcap;
	int *pmode;
	long *pmtime;
{
	capset    csdir;
	capset   *res_capset;
	long      mtime;
	sp_entry  in;
	sp_result out;
	errstat   err;

	char namebuf[PATH_MAX+1];
	const char *q = path + strlen(path); 	/* Point at end of path */

        while (q > path && *--q == '/') ;  /* Ignore trailing /'s */
        if (*q == '.' && (q == path || *--q == '/' ||
                          *q == '.' && (q == path || *--q == '/'))) {
                /* Path ends in "." or ".."; normalize it: */
		if (path_capnorm(&dir, path, namebuf, sizeof namebuf) >= 0)
			path = namebuf;
	}

	if ((err = _ajax_csorigin(dir, path, &csdir)) != STD_OK)
		return err;

	/* If the caller is not interested in the modification time of the
	 * entry, a simple lookup is sufficient.
	 * Otherwise we have to do it in two steps.
	 */
	res_capset = NULL;
	if (pmtime == NULL) {
		err = sp_lookup(&csdir, path, &in.se_capset);
		cs_free(&csdir);
		if (err != STD_OK)
			return err;

		res_capset = &in.se_capset;
	} else {
		in.se_name = (char *) path;
		err = sp_traverse(&csdir, (const char **) &in.se_name,
				  &in.se_capset);
		cs_free(&csdir);
		if (err != STD_OK)
			return err;

		if (*in.se_name == '\0') {
			/* Looking up "/" or "" -- no mtime */
			res_capset = &in.se_capset;
			mtime = 0; /* Unknown */
		} else {
			if ((err = sp_setlookup(1, &in, &out)) == STD_OK &&
			    (err = ERR_CONVERT(out.sr_status)) == STD_OK)
			{
				res_capset = &out.sr_capset;
				mtime = out.sr_time;
			}
			cs_free(&in.se_capset);
		}
	}

	/* now store the results */
	if (err == STD_OK) {
		if (pcap != NULL && res_capset != NULL) {
			(void) cs_to_cap(res_capset, pcap);
		}
		if (res_capset != NULL)
			cs_free(res_capset);
		if (pmode != NULL)
			*pmode = 0777; /* Fake something */
		if (pmtime != NULL)
			*pmtime = mtime;
	}
	return err;
}
