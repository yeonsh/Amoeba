/*	@(#)lintlib.c	1.3	94/04/06 10:10:12 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * lintlibrary for assembler routines
 */

#include "amoeba.h"
#include "machdep.h"
#include "mmuproto.h"

/*ARGSUSED*/
void wr_phys_addr(a, v)
uint32 a, v;
{
}

uint32 rd_phys_addr(a)
uint32 a;
{
    return a;
}

/*ARGSUSED*/
void wr_mmu_reg(a, v)
int a;
uint32 v;
{
}

/*ARGSUSED*/
uint32 rd_mmu_reg(a)
int a;
{
    return 0;
}

/*ARGSUSED*/
uint32 mm_probe(a)
vir_bytes a;
{
    return 0;
}

void data_cache_flash_clear() {}
void mmu_flush_data_cache() {}

/*ARGSUSED*/
void mm_flush(a)
int a;
{
}

void mmu_accesstrap() {}
