/*	@(#)group.h	1.4	96/02/27 10:39:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _SYS_FLIP_GROUP_H_
#define _SYS_FLIP_GROUP_H_

#include <_ARGS.h>

#ifdef NO_MPX_REGISTER
void grp_stopgrp _ARGS(( struct thread *t ));
int grp_sendsig _ARGS(( struct thread *t ));
void grp_destroy _ARGS(( struct thread *t ));
#endif
int grp_memaddress _ARGS((port *p, g_id_t gd, g_indx_t memid, adr_p memaddr));

#endif /* _SYS_FLIP_GROUP_H_ */
