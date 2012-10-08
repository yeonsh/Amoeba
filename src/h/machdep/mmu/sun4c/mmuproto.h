/*	@(#)mmuproto.h	1.3	96/02/27 10:31:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MMPROTO_H__
#define	__MMPROTO_H__


/* From mm.c */
#include "openprom.h"
#include "mmuconst.h"

vir_bytes	mmu_mapdev _ARGS((dev_reg_t *));
vir_bytes	mmu_virtaddr _ARGS((char *));

void		mmu_initcache _ARGS((void));
void		mmu_flushcache _ARGS((void));
void		mmu_offcache _ARGS((void));
void		cache_flush _ARGS((vir_bytes, vir_bytes));
void		mmu_flushctx _ARGS((int, struct procseg *));

void		mmu_set_sysenb_reg _ARGS((int));
int		mmu_probe _ARGS((vir_bytes, int, int));
void		mmu_accesstrap _ARGS((void));

int		mmu_getpte _ARGS((int, vir_bytes));
void		mmu_setpte _ARGS((int, vir_bytes, int));

int		mmu_getpmeg _ARGS((int, vir_bytes));
void		mmu_setpmeg _ARGS((int, vir_bytes, int));

int		mmu_getctx _ARGS((void));
void		mmu_setctx _ARGS((int));

void		mmu_populate _ARGS((vir_bytes, int));

#endif /* __MMPROTO_H__ */
