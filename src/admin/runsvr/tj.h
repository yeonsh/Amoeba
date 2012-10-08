/*	@(#)tj.h	1.2	94/04/06 11:52:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _TJ_H_
#define _TJ_H_

#include "_ARGS.h"

struct tjobset;

struct tjobset *tj_create	_ARGS((long stacksize, int nworkers));
errstat		tj_timeleft	_ARGS((struct tjobset *tj, void (*func)(),
				       char *data, long *left));
errstat		tj_addjob	_ARGS((struct tjobset *tj, void (*func)(),
				       char *data, long delay));
int		tj_cancel	_ARGS((struct tjobset *tj, void (*func)(),
				       char *data));
void		tj_wait		_ARGS((struct tjobset *tj));
void		tj_destroy	_ARGS((struct tjobset *tj));
int		tj_moreworkers	_ARGS((struct tjobset *tj, int n));

#endif


