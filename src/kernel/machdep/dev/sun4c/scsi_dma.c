/*	@(#)scsi_dma.c	1.5	96/02/27 13:53:50 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *      L64853 DMA Chip Routines for SCSI interface
 *	Also the mmu support needed to avoid 16MB boundaries.
 *
 * Author:
 *      Gregory J. Sharp, Aug 1993
 */

#include "amoeba.h"
#include "map.h"
#include "machdep.h"
#include "memaccess.h"
#include "mmuconst.h"
#include "mmuproto.h"
#include "scsi_dma.h"
#include "scsi_dmaprot.h"
#include "debug.h"
#include "assert.h"
INIT_ASSERT


/********************************************************/
/* MMU support to map the kernel virtual address to a	*/
/* virtual address in the top 16 MB of virtual address	*/
/* space.  This avoids possible 16MB boundaries which	*/
/* the DMA chip can't handle.  Also ensures pmegs are	*/
/* present so no memory errors during dma		*/
/********************************************************/

#define	NSEGS_FOR_SCSI		4
#define	MAP_SIZE		(NSEGS_FOR_SCSI * MMU_SEGSIZE)
#define	SCSI_LAST		(SCSIMEM + MAP_SIZE)
#define	MIN_ALLOC		MAX_DMA_BYTES

uint32		scsi_next;	/* pointer to next free chunk of memory */

void
scsi_dma_init()
{
    /* Reserve pmegs for SCSI dma to use */
    mmu_populate((vir_bytes) SCSIMEM, MAP_SIZE);
    scsi_next = (vir_bytes) SCSIMEM;
}


/*
 * This uses optimistic address space allocation.  Pray that we stop using a
 * chunk of address space before it is reused.  Only when multiple devices are
 * doing 64K transfers will there be trouble.  We can handle at least 8
 * simultaneous mappings of that size with 4 segments.  Should be enough?
 * You can only have one per controller active anyway.
 */

static uint32
scsi_dma_map(fromaddr, size)
uint32	fromaddr;	/* A kernel virtual address to be mapped in higher up */
uint32	size;
{
    uint32	addr;
    uint32	retval;
    uint32	end;

    assert(fromaddr < SCSIMEM); /* Should *never* be dma to here! */

    /*
     * Might cross a "page" boundary so make sure 2 pages are available
     * just in case
     */
    addr = scsi_next;
    if (addr + 2* MIN_ALLOC >= SCSI_LAST)
	addr = SCSIMEM;

    /* Always allocate in chunks of MIN_ALLOC */
    assert((addr & (MIN_ALLOC - 1)) == 0);
    retval = addr | (fromaddr & (PAGESIZE - 1));
    end = (retval + size + MIN_ALLOC - 1) & ~ (MIN_ALLOC - 1);

    /* All DMA takes place in context 0.  We need a PTE per page so ... */
    while (addr < end)
    {
	DPRINTF(2, ("mmu_setpte(%x,%x,%x)\n",
				KERN_CTX, addr, MMU_MAKEPTE(fromaddr)));
	mmu_setpte(KERN_CTX, addr, MMU_MAKEPTE(fromaddr));
	addr += PAGESIZE;
	fromaddr += PAGESIZE;
    }

    scsi_next = addr;
    assert(retval >= SCSIMEM);
    assert(retval < SCSI_LAST);
    return retval;
}


/****************************************/
/*	L64853 DMA Chip Routines	*/
/****************************************/


int
scsi_dma_error(lp)
dmactlr *	lp;
{
    if (mem_get_long(&lp->l_stat_ctl) & L64853_ERR_PEND)
    {
	/* Clear the error */
	mem_put_long(&lp->l_stat_ctl, L64853_FLUSH);
	printf("WARNING: SCSI DMA error\n");
	return 1;
    }
    return 0;
}


/*
 * scsi_dma_reset
 *      Reset the DMA channel - typically only done at initialisation.
 */

void
scsi_dma_reset(lp)
dmactlr *       lp;
{
    uint32	mask;

    DPRINTF(0, ("scsi_dma_reset: resetting DMA controller\n"));

    if (mem_get_long(&lp->l_stat_ctl) & L64853_REQ_PEND)
    {
	DPRINTF(0, ("scsi_dma_reset: ignoring dangerous DMA reset\n"));
	return;
    }
    mem_put_long(&lp->l_stat_ctl, L64853_RESET);
    mem_AND_long(&lp->l_stat_ctl, ~L64853_RESET);

    /* We don't have an ILACC chip */
    mem_AND_long(&lp->l_stat_ctl, ~L64853_ILACC);

    /* Just in case, clear everything down */
    mem_put_long(&lp->l_stat_ctl, L64853_FLUSH);

    /* Give warning if we don't recognise the chip version! */
    mask = mem_get_long(&lp->l_stat_ctl) & L64853_DEV_ID_MASK;
    if (mask != L64853_DEV_ID1 && mask != L64853_DEV_ID2)
    {
	DPRINTF(0, ("scsi_dma: WARNING: unknown DMA device id (0x%x)."));
	DPRINTF(0, ("  We'll try it anyway.\n", mask));
    }

    /* Enable interrupts for the SCSI & DMA chips */
    mem_OR_long(&lp->l_stat_ctl, L64853_INT_EN);
}


void
scsi_dma_go(lp)
dmactlr *	lp;
{
    mem_OR_long(&lp->l_stat_ctl, L64853_INT_EN | L64853_EN_DMA);
}


void
scsi_dma_stop(lp)
dmactlr *	lp;
{
    if (mem_get_long(&lp->l_stat_ctl) & L64853_PACK_CNT)
    {
	/* Wait for DMA to drain */
	mem_OR_long(&lp->l_stat_ctl, L64853_DRAIN);
	while (mem_get_long(&lp->l_stat_ctl) & L64853_PACK_CNT)
	    /* do nothing */;
    }

    mem_AND_long(&lp->l_stat_ctl, ~L64853_EN_DMA);
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
    uint32	direction;
    uint32	stat;
    uint32	kaddr;

    /*
     * - Note: DMAs must not cross 16 Mbyte boundaries!
     *   That should have been handled at a higher level.
     */
    kaddr = scsi_dma_map(addr, size);
    assert((kaddr & 0xFF000000) == ((kaddr + size - 1) & 0xFF000000));

    /* Do a drain if required */
    stat = mem_get_long(&lp->l_stat_ctl);
    if (stat & L64853_PACK_CNT)
    {
	mem_put_long(&lp->l_stat_ctl, L64853_DRAIN);
	while (mem_get_long(&lp->l_stat_ctl) & L64853_PACK_CNT)
	    /* do nothing */;
	stat = mem_get_long(&lp->l_stat_ctl);
    }

    /* If it is still busy then we're in trouble */
    if (stat & L64853_REQ_PEND)
	panic("scsi_dma_setup: previous DMA request still pending!\n");

    mem_OR_long(&lp->l_stat_ctl, L64853_FLUSH);

    /* Make sure DMA is disabled before altering the registers */
    scsi_dma_stop(lp);

    /* Set up new DMA */
    mem_put_long(&lp->l_addr, kaddr);
    direction = from_mem ? L64853_DMA_FROM_MEM : L64853_DMA_TO_MEM;
    mem_put_long(&lp->l_stat_ctl, L64853_INT_EN | direction);
}


/*
 * scsi_dma_enable_intr
 *	Enable interrupts for the SCSI & DMA chips
 */

void
scsi_dma_enable_intr(lp)
dmactlr *	lp;
{
    mem_OR_long(&lp->l_stat_ctl, L64853_INT_EN);
}
