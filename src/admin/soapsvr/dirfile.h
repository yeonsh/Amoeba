/*	@(#)dirfile.h	1.3	94/04/06 11:57:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef DIRFILE_H
#define DIRFILE_H

#include "_ARGS.h"

errstat dirf_modify _ARGS((objnum obj, int ncaps,
			   capability oldcaps[], capability newcaps[],
			   capability fsvr_caps[]));
errstat dirf_write  _ARGS((objnum obj, int ncaps, capability newcaps[],
			   capability fsvr_caps[]));
errstat dirf_read   _ARGS((objnum obj, capability filecaps[], int nfilecaps,
			   struct sp_dir **dir_ret));
errstat dirf_touch  _ARGS((objnum obj, capability caps[], int good_file[]));
errstat dirf_check  _ARGS((objnum obj, capability filecaps[]));

int dirf_safe_rows  _ARGS((objnum obj));

/* Max size of bullet directory file: */
#define MAX_DIR_BYTES	120000

/* The actual buffer used is somewhat bigger: */
#define IOBUF_EXTRA	1000
#define IOBUF_SIZE	(MAX_DIR_BYTES + IOBUF_EXTRA)

char *  sp_get_iobuf _ARGS((void));
void    sp_put_iobuf _ARGS((char *buf));

#endif /* DIRFILE_H */
