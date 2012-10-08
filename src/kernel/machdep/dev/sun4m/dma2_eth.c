/*	@(#)dma2_eth.c	1.1	94/04/06 09:36:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "openprom.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "mmuproto.h"

/*
 * Ethernet part of DMA2
 */
struct ledma {
	uint32	e_csr;
	uint32	e_tst_csr;
	uint32	e_vld;
	uint8	e_base_addr;
};

/*
 * e_csr contains zillions of bits, here are some of them
 */
#define E_INT_EN	0x00000010	/* Interrupt enable */
#define	E_INVALIDATE	0x00000020	/* Clear everything */
#define E_TP_AUI_	0x00400000	/* TP iso AUI */

static struct ledma *lep;

void
initdma2ether()
{

	lep = (struct ledma *) mmu_virtaddr("/iommu/sbus/ledma");
	compare (lep, !=, 0);
	DPRINTF(2, ("ledma: %x, %x, %x, %x\n", lep->e_csr, lep->e_tst_csr,
						lep->e_vld, lep->e_base_addr));
	lep->e_csr |= E_INT_EN;		/* We want an interrupt someday */
}

void
stopdma2ether()
{
	if (lep != 0) {
		lep->e_csr &= ~E_INT_EN;	/* Stop interrupts */
		lep->e_csr |= E_INVALIDATE;	/* Clear pending interrupts */
	}
}
