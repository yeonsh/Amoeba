/*	@(#)flgrp_alloc.h	1.5	96/02/27 14:01:23 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FLGRP_ALLOC_H__
#define __FLGRP_ALLOC_H__

#include "flgrp_malloc.h"

void grp_initbuf();

/* New buffer allocation implementation: use malloc() variant to avoid
 * allocating way too much memory for small messages (which are typically
 * used most frequently).
 */

#define GRP_FREEBUF(g, b) \
    if ((b) != 0) { 							\
	GDEBUG(2, printf("free buffer at 0x%lx\n", b));			\
	mal_free(&(g)->g_mem_descr, (b));				\
	(g)->g_bufnbuf++;						\
	(b) = 0;							\
    }

#define GRP_GETBUF(g, b, size) \
    if ((g)->g_bufnbuf > 0) {						\
	GDEBUG(2, printf("alloc buffer of %d bytes\n", size));		\
	(b) = mal_malloc(&(g)->g_mem_descr, size);			\
	if ((b) != 0) {							\
	    (g)->g_bufnbuf--;						\
	}								\
	GDEBUG(2, printf("addr 0x%lx, avail %d\n", b, (g)->g_bufnbuf));	\
    } else {								\
	/* free some first */						\
	GDEBUG(2, printf("no buffers available\n"));			\
	(b) = 0;							\
    }

#endif /* __FLGRP_ALLOC_H__ */
