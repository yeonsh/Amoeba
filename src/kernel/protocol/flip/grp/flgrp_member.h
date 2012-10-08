/*	@(#)flgrp_member.h	1.4	94/04/06 08:39:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_MEMBER_H__
#define __FLGRP_MEMBER_H__

/* Prototypes for member functions */
#include <_ARGS.h>

#ifndef SMALL_KERNEL
void grmem_print _ARGS(( member_p m ));
void grmem_debug _ARGS(( group_p g ));
int grmem_dump _ARGS(( char *begin, char *end, group_p g ));
#endif /* SMALL_KERNEL */

int grmem_member _ARGS(( group_p g, adr_p addr, g_index_t *m ));
int grmem_alloc _ARGS(( group_p g, g_index_t *new ));
void grmem_del _ARGS(( group_p g, member_p m ));
void grmem_buffer _ARGS(( group_p g, member_p src, bchdr_p bc, char *data,
			 f_size_t n, int bigendian ));
void grmem_buffree _ARGS(( group_p g ));
void grmem_new _ARGS(( group_p g, g_index_t i, adr_p sa, adr_p rpcaddr,
					g_msgcnt_t messid, g_seqcnt_t seqno ));
void grmem_init _ARGS(( group_p g, member_p m ));

#endif /* __FLGRP_MEMBER_H__ */
