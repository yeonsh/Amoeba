/*	@(#)lintlib.c	1.2	94/04/06 09:44:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Lint library for assembler routines for the sun4c mmu
 *
 * Author: Greg Sharp, 2 Sept, 1991
 */

#include "amoeba.h"
#include "machdep.h"
#include "mmuconst.h"
#include "mmuproto.h"

/* From khead.S */
/****************/
void abort() {}

/* From mmutil.S */
/*****************/
void mmu_initcache() {}

void mmu_offcache() {}

/*ARGSUSED*/
int
mmu_probe(addr, width, rw)
vir_bytes addr;
int width; /* #bytes wide to probe (1,2,4,8) */
int rw; /* 0 = readable, non-zero = writeable */
{
    return 0;
}

void mmu_accesstrap() {}

/*ARGSUSED*/
void mmu_set_sysenb_reg(flags)
int flags;
{
}

/*ARGSUSED*/
int
mmu_getpte(ctx, vaddr)
int ctx;
vir_bytes vaddr;
{
    return 0;
}

/*ARGSUSED*/
void
mmu_setpte(ctx, vaddr, pte)
int ctx;
vir_bytes vaddr;
int pte;
{
}

/*ARGSUSED*/
int
mmu_getpmeg(ctx, vaddr)
int ctx;
vir_bytes vaddr;
{
    return 0;
}

/*ARGSUSED*/
void
mmu_setpmeg(ctx, vaddr, pmeg)
int ctx;
vir_bytes vaddr;
int pmeg;
{
}

/*ARGSUSED*/
void
mmu_flushctx(ctx, psp)
int ctx;
struct procseg * psp;
{
}

int
mmu_getctx()
{
    return 0;
}

/*ARGSUSED*/
void
mmu_setctx(ctx)
int ctx;
{
}

void
mmu_flushcache() {}

void
mmu_flush_data_cache() {}

/* Variables defined in assembly code */
int		mmu_nofault;

/*
 * The next few things are actually assembler labels but so we can use them
 * from C we disguise them as characters and take their address where we need
 * it.
 */
char	kst_beg;	/* beginning of kernel stack */
char	kst_end;	/* end of kernel stack */
char	end;		/* end of bss */
int	begin;		/* start of bss */
int *	mmu_prend;	/* end of probe code */
int *	mmu_prbeg;	/* beginning of probe code */
