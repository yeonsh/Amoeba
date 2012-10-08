/*	@(#)proc.h	1.4	96/02/27 10:33:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_PROC_H__
#define __MODULE_PROC_H__

#include "_ARGS.h"
#include "caplist.h"
#include "machdep.h"
#include "server/process/proc.h"

#define	pro_exec		_pro_exec
#define	pro_getcomment		_pro_getcomment
#define	pro_setcomment		_pro_setcomment
#define	pro_getdef		_pro_getdef
#define	pro_getload		_pro_getload
#define	pro_getowner		_pro_getowner
#define	pro_setowner		_pro_setowner
#define	pro_sgetload		_pro_sgetload
#define	pro_stun		_pro_stun
#define	pro_swapproc		_pro_swapproc

#define	ps_segcreate		_ps_segcreate
#define	ps_segwrite		_ps_segwrite

#define	pd_arch			_pd_arch
#define	pd_read			_pd_read
#define	pd_read_multiple	_pd_read_multiple
#define	pd_preserve		_pd_preserve

#define	exec_file		_exec_file
#define	exec_pd			_exec_pd
#define	exec_set_runsvr		_exec_set_runsvr
#define	exec_set_pooldir	_exec_set_pooldir
#define	exec_get_runsvr		_exec_get_runsvr
#define	exec_get_pooldir	_exec_get_pooldir
#define	exec_multi_findhost	_exec_multi_findhost
#define	exec_findhost		_exec_findhost
#define	buildstack		_buildstack

#define	exitprocess		_exitprocess
#define	getinfo			_getinfo
#define	seg_map			_seg_map
#define	seg_unmap		_seg_unmap
#define	seg_grow		_seg_grow
#define	findhole		_findhole


errstat	pro_exec _ARGS((capability *, process_d *, capability *));
errstat pro_getcomment _ARGS((capability *, char *, int));
errstat pro_setcomment _ARGS((capability *, char *));
errstat	pro_getdef _ARGS((capability *, int *, int *, int *));
errstat	pro_getload _ARGS((capability *, long *, long *, long *));
errstat	pro_sgetload _ARGS((capability *, long *, long *, long *,
								capability *));
errstat	pro_getowner _ARGS((capability *, capability *));
errstat	pro_setowner _ARGS((capability *, capability *));
errstat	pro_stun _ARGS((capability *, long));
errstat	pro_swapproc _ARGS((capability *, capability *, capability *));
errstat ps_segcreate _ARGS((capability *, long, capability *, capability *));
errstat ps_segwrite _ARGS((capability *, long, char *, long));

char *	pd_arch _ARGS((process_d *));
process_d * pd_read _ARGS((capability *));
errstat	pd_read_multiple  _ARGS((capability *, process_d ***, int *));
int	pd_preserve _ARGS((capability *, process_d *, int, capability *));

errstat exec_file _ARGS((capability *, capability *, capability *, int,
			char **, char **, struct caplist *, capability *));
errstat	exec_pd _ARGS((process_d *, int, capability *, capability *,
		    int, char **, char **, struct caplist *, capability *));
void	exec_set_runsvr _ARGS((capability * runcap));
void	exec_set_pooldir _ARGS((capability *pooldir));
errstat exec_get_runsvr _ARGS((capability *runcap));
errstat exec_get_pooldir _ARGS((capability *pooldir));
errstat	exec_multi_findhost  _ARGS((capability *, capability *, process_d **,
					    int, int *, capability *, char *));
errstat	exec_findhost _ARGS((process_d *, capability *));
unsigned long buildstack _ARGS(( char *, long, unsigned long, char **,
						char **, struct caplist * ));

void	exitprocess _ARGS(( int ));
segid	seg_map _ARGS(( capability *, vir_bytes, vir_bytes, long ));
errstat	seg_unmap _ARGS(( segid, capability * ));
errstat	seg_grow _ARGS(( segid, vir_bytes ));
char *	findhole _ARGS(( long ));
int	getinfo _ARGS(( capability *, process_d *, int ));

#endif /* __MODULE_PROC_H__ */
