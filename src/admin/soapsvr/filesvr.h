/*	@(#)filesvr.h	1.2	94/04/06 11:57:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef FILESVR_H
#define FILESVR_H

#include "_ARGS.h"

int  fsvr_add_svr	_ARGS((capability *cap, int pref));
void fsvr_replace_svr	_ARGS((int index, capability *svrcap));
void fsvr_get_svrs	_ARGS((capability caplist[], int ncaps));

void fsvr_set_unavail	_ARGS((port *svrport));
int  fsvr_avail		_ARGS((port *svrport));
void fsvr_check_svrs	_ARGS((void));

int  fsvr_nr_avail	_ARGS((void));
int  fsvr_preferred	_ARGS((void));
void fsvr_order_caps	_ARGS((capability caps[], int ncaps, int order[]));

#endif /* FILESVR_H */
