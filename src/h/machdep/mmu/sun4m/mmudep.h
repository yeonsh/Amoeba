/*	@(#)mmudep.h	1.3	94/04/06 16:49:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MMUDEP_H__
#define __MMUDEP_H__

#include "stdlib.h"

#define	KSTK_ALLOC(a)   (vir_bytes) malloc((size_t) (a))
#define	KSTK_FREE(a)	free((_VOIDSTAR) (a))

#endif /* __MMUDEP_H__ */
