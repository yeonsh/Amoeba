/*	@(#)caching.h	1.4	96/02/27 10:21:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef CACHING_H
#define CACHING_H

#include "_ARGS.h"

int	 out_of_cache      _ARGS((objnum obj));
int	 in_cache	   _ARGS((objnum obj));

void	 cache_new_dir	   _ARGS((objnum obj));
void	 cache_delete	   _ARGS((objnum obj));
void	 cache_destroy	   _ARGS((objnum obj));
void	 cache_init	   _ARGS((int rowmem));

int      cached_rows       _ARGS((void));
void     cache_set_maxrows _ARGS((int maxrows));
void	 cache_set_size	   _ARGS((int rowmem));

void     make_room	   _ARGS((int nrows));
errstat  sp_refresh        _ARGS((objnum obj, command cmd));

void     init_sp_table     _ARGS((int do_fsck));
void     refresh_all	   _ARGS((void));
int      get_time_to_live  _ARGS((objnum obj));
void     set_time_to_live  _ARGS((objnum obj, int time));

#endif /* CACHING_H */


