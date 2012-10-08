/*	@(#)mmuproto.h	1.3	96/02/27 10:31:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MMPROTO_H__
#define	__MMPROTO_H__

/* From mmutil.S */
uint32		rd_phys_addr _ARGS((uint32));
void		wr_phys_addr _ARGS((uint32, uint32));
uint32		rd_mmu_reg _ARGS((uint32));
void		wr_mmu_reg _ARGS((uint32, uint32));
uint32		mm_probe _ARGS((vir_bytes));
void		initmemarea _ARGS((char *, phys_clicks));
void		mmu_accesstrap _ARGS((void));
void		mm_flush _ARGS((int));

/* Cache flush functions */
void		msI_data_cache_flash_clear _ARGS((vir_bytes, vir_bytes));
void		msI_instr_cache_flash_clear _ARGS((void));
void		msII_do_cache_flush_all _ARGS((int, int));

void		cache_flush _ARGS((vir_bytes, vir_bytes));
void		cache_flush_all _ARGS((void));

/* From mm.c */
#include "openprom.h"

uint32	p_addr _ARGS((uint32));
vir_bytes	mmu_mapdev _ARGS((dev_reg_t *));
vir_bytes	mmu_mapmem _ARGS((dev_reg_t *, int cachable));
vir_bytes	mmu_virtaddr _ARGS((char *));
void		mmu_dontcache _ARGS((vir_bytes, vir_bytes));

/* From iommu.c */
uint32		iommu_allocbuf _ARGS((phys_bytes, uint32));
uint32		iommu_circbuf _ARGS((phys_bytes, uint32));
uint32		iommu_scsimap _ARGS((phys_bytes, uint32));

#endif /* __MMPROTO_H__ */
