/*	@(#)iommu.c	1.3	94/04/06 09:45:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "map.h"
#include "sparc-refmmu.h"
#include "mmuconst.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "mmuproto.h"

extern vir_bytes mmu_virtaddr _ARGS(( char * ));

/*
 * Reading and writing physical space
 * with some possible debugging printf's
 */

#undef PHYSDEBUG
#ifdef PHYSDEBUG
static
deb_rd_phys_addr(x) {
	long r;

	r = rd_phys_addr(x);
	DPRINTF(2, ("rd_phys_addr(%x) = %x\n", x, r));
	return r;
}

static
deb_wr_phys_addr(x, v) {

	DPRINTF(2, ("wr_phys_addr(%x, %x)\n", x, v));
	wr_phys_addr(x, v);
}

#define RD_PHYS_ADDR deb_rd_phys_addr
#define WR_PHYS_ADDR deb_wr_phys_addr

#else

#define RD_PHYS_ADDR rd_phys_addr
#define WR_PHYS_ADDR wr_phys_addr

#endif /* PHYSDEBUG */

static
struct iommuregs {
	uint32	iom_control;	/* control register */
	uint32	iom_base;	/* base of PTE's */
	uint32	iom_hole1[3];
	uint32	iom_flushall;	/* flush all TLB entries */
	uint32	iom_flush;	/* Address flush register */
} *iommuregs;

#define CTL_IMPL_MASK	0xF0000000
#define CTL_VER_MASK	0x0F000000
#define CTL_RANGE_MASK	0x0000001C
#define CTL_RANGE_SHIFT	2
#define	CTL_MMU_ENABLE	0x00000001

#define IO_SCSI_START		0xFE000		/* Allow 8 Mbytes allocation */
#define IO_SCSI_END		0xFE800
#define IO_ALLOC_START		0xFF000		/* Allow 8 Mbytes allocation */
#define IO_ALLOC_END		0xFF800
#define IO_CIRCBUF_START	0xFFC00		/* and 4 Mbytes circbuf */
#define IO_CIRCBUF_END		0x100000

static uint32 iobottom, iorange;	/* Page numbers */
static uint32 iopte;			/* Array of IO pte's */

struct iommu_alloc {
	uint32	ioa_start;
	uint32	ioa_end;
	uint32	ioa_now;
};

static uint32
iommu_alloc_and_map(a, s, ioa)
phys_bytes a;
uint32 s;
struct iommu_alloc *ioa;
{
	uint32 page, startpage, endpage, pagebits;
	uint32 now;
	uint32 result;
	uint32 physpage;

	DPRINTF(2, ("iommu_alloc_and_map(%x, %d, (%x,%x,%x)) = ",
			a, s, ioa->ioa_start, ioa->ioa_end, ioa->ioa_now));
	startpage = a>>PAGESHIFT;
	endpage = (a+s-1)>>PAGESHIFT;
	pagebits = ((uint32) a)&(PAGESIZE-1);
	now = ioa->ioa_now;
	if (now + endpage - startpage >= ioa->ioa_end) {
		now = ioa->ioa_start;
		assert( now );
	}
	compare(now, >=, iobottom);
	result = (now<<PAGESHIFT) + pagebits;
	for (page=startpage; page <= endpage; page++) {
		physpage = p_addr(page<<PAGESHIFT)>>(PAGESHIFT-PTE_PPN_SHIFT);
		WR_PHYS_ADDR(iopte + 4*(now-iobottom),
				    physpage|PTE_ACC_RW_RW_|PTE_ET_PTE);
		iommuregs->iom_flush = now<<PAGESHIFT;
		now++;
	}
	ioa->ioa_now = now;
	DPRINTF(2, ("%x\n", result));
	return result;
}

/*
 * Circular IO mmu stuff used by SCSI
 */

static struct iommu_alloc scsi_alloc;

static void
initscsimap()
{

	scsi_alloc.ioa_start = IO_SCSI_START;
	scsi_alloc.ioa_end = IO_SCSI_END;
	scsi_alloc.ioa_now = IO_SCSI_START;
}

uint32
iommu_scsimap(a, s)
phys_bytes a;
uint32 s;
{

	return iommu_alloc_and_map(a, s, &scsi_alloc);
}

/*
 * Circular IO mmu stuff used by the Ethernet code
 */

static struct iommu_alloc circbuf_alloc;

static void
initcircbuf()
{

	circbuf_alloc.ioa_start = IO_CIRCBUF_START;
	circbuf_alloc.ioa_end = IO_CIRCBUF_END;
	circbuf_alloc.ioa_now = IO_CIRCBUF_START;
}

uint32
iommu_circbuf(a, s)
phys_bytes a;
uint32 s;
{
	return iommu_alloc_and_map(a, s, &circbuf_alloc);
}

/*
 * Allocating IO mmu stuff used by the Ethernet code
 */

static struct iommu_alloc allocbuf_alloc;

static void
initallocbuf()
{

	allocbuf_alloc.ioa_start = 0;		/* No restart */
	allocbuf_alloc.ioa_end = IO_ALLOC_END;
	allocbuf_alloc.ioa_now = IO_ALLOC_START;
}

uint32
iommu_allocbuf(a, s)
phys_bytes a;
uint32 s;
{

	return iommu_alloc_and_map(a,s, &allocbuf_alloc);
}

/*
 * Init routine
 */

void
initiommu()
{

	iommuregs = (struct iommuregs *) mmu_virtaddr("/iommu");
	DPRINTF(2, ("iommu: control = %x, base = %x\n",
			iommuregs->iom_control, iommuregs->iom_base));

	iorange = 1<<12;	/* 16 MB */
	iorange <<= ((iommuregs->iom_control&CTL_RANGE_MASK)>>CTL_RANGE_SHIFT);
	DPRINTF(2, ("iorange is %x\n", iorange));

	iobottom = 0x100000-iorange;
	iopte = iommuregs->iom_base<<4;

	initcircbuf();
	initallocbuf();
	initscsimap();
}
