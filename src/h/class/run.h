/*	@(#)run.h	1.3	96/02/27 10:28:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __RUN_H__
#define __RUN_H__

/* Constants: */
#define PD_BUFSIZE	6000
#define PD_HOSTSIZE	16

/* Operators: */
#include "_ARGS.h"

#define	run_get_exec_cap	_run_get_exec_cap
#define	run_multi_findhost	_run_multi_findhost
#define	run_create		_run_create

errstat
run_get_exec_cap   _ARGS((capability *runobj,
			  capability *procsvr,
			  capability *execcap,
			  char * arch));

errstat
run_multi_findhost _ARGS((capability *runobj,
			  int	      pd_num,
			  char	     *pd_buf,
			  int         pd_len,
			  int        *pd_chosen,
			  char        hostname[PD_HOSTSIZE],
			  capability *procsvr));

errstat run_create _ARGS((capability *runsuper,
			  capability *dircap,
			  capability *runobj));

#endif /* __RUN_H__ */
