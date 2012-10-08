/*	@(#)mmudep.h	1.3	94/04/06 16:48:30 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef	__MMUDEP_H__
#define	__MMUDEP_H__

/*
 * The kernel stack allocator is used to allow the kernel to "lock" the
 * pages of a kernel stack into memory so that trap handling will not
 * kill us.
 */
#define KSTK_ALLOC	kstk_alloc
#define	KSTK_FREE	kstk_free

vir_bytes kstk_alloc _ARGS(( vir_bytes ));
void kstk_free _ARGS(( vir_bytes ));

#endif	/* __MMUDEP_H__ */
