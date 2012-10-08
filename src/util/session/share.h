/*	@(#)share.h	1.3	94/04/07 16:04:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <_ARGS.h>

struct object {
	struct object *ob_next;
	struct link *ob_link;
	void (*ob_close)();
	char *ob_data;
};

struct object *ob_new _ARGS((int pid, void (*close)(), char *data));

int ob_share _ARGS((struct object *p, int pid));
void ob_unshare _ARGS((struct object *p, int pid));

void ob_allshare _ARGS((int pid, int newpid));
void ob_allunshare _ARGS((int pid));
