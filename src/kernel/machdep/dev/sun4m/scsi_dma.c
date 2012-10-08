/*	@(#)scsi_dma.c	1.3	94/04/06 09:38:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *	L64854 DMA Chip Routines for SCSI interface
 *
 * Author:
 *	Gregory J. Sharp, Aug 1993
 */

#include "amoeba.h"
#include "machdep.h"
#include "memaccess.h"
#include "mmuproto.h"
#include "scsi_dma.h"
#include "scsi_dmaprot.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT


/*
 * scsi_dma_error
 *	Checks if a DMA error is pending and clears it.
 */
int
scsi_dma_error(lp)
dmactlr *	lp;
{
    uint32	stat_reg;

    stat_reg = lp->l_stat_ctl;
    if (stat_reg & L64854_ERR_PEND)
    {
	/* Clear the error */
	mem_put_long(&lp->l_stat_ctl, L64854_INVALIDATE);
	printf("WARNING: SCSI DMA error.  DMA status reg = %x\n", stat_reg);
	return 1;
    }
    return 0;
}


/*
 * scsi_dma_reset
 *	Reset the DMA channel - typically only done at initialisation.
 */

void
scsi_dma_reset(lp)
dmactlr *	lp;
{
    uint32	mask;

    DPRINTF(0, ("scsi_dma_reset: resetting DMA controller\n"));

    /* Reset the DMA chip */
    mem_put_long(&lp->l_stat_ctl, L64854_RESET);
    mem_AND_long(&lp->l_stat_ctl, ~L64854_RESET);

    mask = mem_get_long(&lp->l_stat_ctl) & L64854_DEV_ID_MASK;
    if (mask != L64854_DEV_ID1)
    {
	DPRINTF(0, ("scsi_dma: WARNING: unknown DMA device id (0x%x)."));
	DPRINTF(0, ("  We'll try it anyway.\n", mask));
    }

    /* Enable interrupts for the SCSI & DMA chips */
    mem_put_long(&lp->l_stat_ctl, L64854_INT_EN);
}


/*
 * scsi_dma_enable_intr
 *      Enable interrupts for the SCSI & DMA chips
 */

void
scsi_dma_enable_intr(lp)
dmactlr *       lp;
{
    mem_OR_long(&lp->l_stat_ctl, L64854_INT_EN);
}


void
scsi_dma_go(lp)
dmactlr *	lp;
{
    mem_OR_long(&lp->l_stat_ctl, L64854_EN_DMA);
}


void
scsi_dma_stop(lp)
dmactlr *	lp;
{
    /* Wait for DMA to drain */
    while (mem_get_long(&lp->l_stat_ctl) & L64854_DRAINING)
	/* do nothing */;

    mem_AND_long(&lp->l_stat_ctl, ~L64854_EN_DMA);
}


/*
 * scsi_dma_setup
 *	Prepare to do a SCSI operation via the D-channel.
 */

void
scsi_dma_setup(lp, from_mem, addr, size)
dmactlr *	lp;
int		from_mem;	/* direction of xfer: !=0 => from memory */
uint32		addr;		/* kernel virtual addr for I/O */
uint32		size;
{
    uint32	kaddr;
    uint32	ctl_bits;

    /*
     * Create iommu mapping for this kernel address.
     * Note: DMAs must cannot cross 16 Mbyte boundaries if iommu_scsimap
     * did the right thing!
     */
    kaddr = iommu_scsimap((phys_bytes) addr, size);
    assert(kaddr != 0);
    assert((kaddr & 0xFF000000) == ((kaddr + size - 1) & 0xFF000000));

    /* Wrap up the previous DMA */
    mem_OR_long(&lp->l_stat_ctl, L64854_INVALIDATE);

    ctl_bits =	L64854_TC_INT_DISABLE |
		L64854_TWO_CYCLE |
		L64854_BURST4 |
		L64854_INT_EN |
		L64854_EN_CNT;
    ctl_bits |= from_mem ? L64854_DMA_FROM_MEM : L64854_DMA_TO_MEM;

    /* Make sure DMA is disabled before altering the registers */
    scsi_dma_stop(lp);

    /* Set up new DMA */
    mem_put_long(&lp->l_addr, kaddr);
    mem_put_long(&lp->l_byte_cnt, size);
    mem_put_long(&lp->l_stat_ctl, ctl_bits);
}
