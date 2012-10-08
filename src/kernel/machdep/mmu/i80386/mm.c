/*	@(#)mm.c	1.13	96/02/27 13:59:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * mm.c
 *
 * i80386 memory management code. When the kernel starts running one of
 * the first routines it should call is initmm to setup the kernel page
 * tables and enable paging in general.
 * 
 * Author:
 *	Leendert van Doorn
 */
#include <bool.h>
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <assert.h>
INIT_ASSERT
#include <map.h>
#include <process/proc.h>
#include <i80386.h>
#include <type.h>
#include <seg.h>
#if defined(ISA) || defined(MCA)
#include <cmos.h>
#endif
#include <string.h>
#include <sys/proto.h>

void page_enable();
void page_disable();
void setCR3();
void flushTLB();
void seg_settype();

/*
 * On ISA/MCA machines the region reserved for memory mapped
 * I/O (between 640 and 1024Kb) is not mapped in.
 */
#if defined(ISA) || defined(MCA)
#define	PAGE_640KB	((640 * 1024) / PAGE_SIZE)
#define	PAGE_1000KB	((1000 * 1024) / PAGE_SIZE)
#endif /* defined(ISA) || defined(MCA) */

static int maxclicks;		/* max. physical memory clicks */
static long nkmaps;		/* number of page tables needed to cover
				   all of physical memory */
long *kpd;			/* kernel page table directory */

#ifndef NOPROC
/*
 * Every process has a memory map associated with it that
 * holds the the user page table directory.
 */
struct memmap {
   long	upd[PAGE_TABLE_ENTRIES];
};
    
extern uint16 nproc;
struct memmap **memmap;
#endif /* NOPROC */

phys_clicks
firstclick()
{
    return 0;
}

phys_clicks
lastclick()
{
    return maxclicks;
}

#ifndef NOPROC
/*ARGSUSED*/
hintmap(proc, size)
    uint16 proc;
    vir_bytes size;
{
    /* Vax-ism, noop on a 80386 */
}

usemap(proc)
    uint16 proc;
{
    register struct memmap *mm = memmap[proc];

    assert(mm);
    if (VTOP(mm->upd) != (phys_bytes)getCR3())
	setCR3(VTOP(mm->upd));
}

startuser(proc, pc, sp)
    uint16 proc;
    vir_bytes pc, sp;
{
    usemap(proc);
    start(pc, sp);
}

/*
 * Map pages into user page table. When there's no user page
 * table entry we allocate one and put it into the user's
 * page directory.
 */
static struct memmap *
mapclick(mm, v, p, type)
    register struct memmap *mm;
    register vir_clicks v;
    register phys_clicks p;
    register unsigned long type;
{
    register long *upt;
    register int index;

    index = v >> (PDINDXSHFT - CLICKSHIFT);
    assert(index > nkmaps && index < PAGE_TABLE_ENTRIES);
    upt = (long *)(mm->upd[index] & ~(CLICKSIZE-1));
    if (upt == (long *)0) {
	upt = (long *) getblk((vir_bytes) PAGE_TABLE_SIZE);
	if (upt == (long *)0) {
	    printf("mapclick failed: no more memory for user page table\n");
	    return (struct memmap *) 0;
	}
	assert(((int)upt & (CLICKSIZE-1)) == 0);
	mm->upd[index] = (long)upt | PTE_UW | PTE_P;
    }
    upt[v & (MAP_CLICKS-1)] = (p << CLICKSHIFT) | type;
    return mm;
}

/*
 * Set up the necessary mapping for a user segment
 */
int
setmap(proc, vir, phy, len, type)
    uint16 proc;
    register vir_clicks vir, len;
    register phys_clicks phy;
    long type;
{
    register struct memmap *mm = memmap[proc];
    register uint32 mmutype;

    assert(phy + len <= maxclicks);
    if ((type & MAP_PROTMASK) == MAP_READWRITE) 
	mmutype = PTE_UW | PTE_P;
    else if ((type & MAP_PROTMASK) == MAP_READONLY)
	mmutype = PTE_UR | PTE_P;
    else
	mmutype = 0;

    assert(mm);
    while (len != 0) {
	mm = mapclick(mm, vir, phy, mmutype);
	if (mm == (struct memmap *)0) return -1;
	len--; vir++; phy++;
    }
    return 0;
}

/*
 * Clrmap is called when a user process exits,
 * and releases all of its user page tables.
 */
clrmap(proc)
    uint16 proc;
{
    register long *upt;
    register int i;

    flushTLB();
    assert(memmap[proc]);
    for (i = nkmaps+1; i < PAGE_TABLE_ENTRIES; i++) {
	upt = (long *)(memmap[proc]->upd[i] & ~(CLICKSIZE-1));
	if (upt != (long *)0)
	    relblk((vir_bytes) upt);
	memmap[proc]->upd[i] = 0;
    }
}
#endif /* NOPROC */

/*
 * Copy 'cnt' bytes from 'src' to 'dst'
 */
phys_copy(src, dst, cnt)
    phys_bytes src, dst, cnt;
{
    (void) memmove((_VOIDSTAR) PTOV(dst), (_VOIDSTAR) PTOV(src), (size_t) cnt);
}

/*
 * Zero 'cnt' bytes at 'dst'
 */
phys_zero(dst, cnt)
    phys_bytes dst, cnt;
{ 
    (void) memset((_VOIDSTAR) PTOV(dst), 0, (size_t) cnt);
}

/*
 * Find out whether physical memory exists
 */
int
phys_probe(addr)
    phys_bytes addr;
{
    return PTOV(addr) > 0 && (PTOV(addr) >> CLICKSHIFT) < maxclicks;
}

/*
 * Find out whether virtual memory exists
 */
/*ARGSUSED*/
int
probe(addr, width)
    vir_bytes addr;
    int width;
{
    register int i;

    if (maxclicks > 0) {
	for (i = 0; i <= width; i += CLICKSIZE)
	    if (!probe_page(addr + i))
		return FALSE;
    }
    return TRUE;
}

/*
 * Probe whether a page exists
 */
int
probe_page(addr)
    vir_bytes addr;
{
    register long *ktp;

    ktp = (long *)(kpd[addr >> PDINDXSHFT] & ~(CLICKSIZE - 1));
    if (ktp && ktp[(addr & ((1 << PDINDXSHFT) - 1)) >> CLICKSHIFT] & PG_P)
	return TRUE;
    return FALSE;
}

#if defined(ISA) || defined(MCA)
/*
 * Find out the amount of memory this machine has, where the kernel is
 * located and set up the segment tables accordingly. This routine is
 * very ISA/MCA specific.
 */
void
mm_initseg()
{
    phys_clicks begin_click, end_click;
    phys_clicks lo_clicks, hi_clicks;
    uint32 lo_mem, hi_mem;
    extern char *kernelstart, end;

    /*
     * The memory sizes are stored in CMOS static RAM, look at
     * cmos.h and cmos.c in the dev/ibm_at directory for more
     * details.
     */
    lo_mem = cmos_read(CMOS_BMEM) + (cmos_read(CMOS_BMEM+1) << 8);
    hi_mem = cmos_read(CMOS_EMEM) + (cmos_read(CMOS_EMEM+1) << 8);

    /* have to convert lo_mem and hi_mem from K to clicks */
    lo_clicks = BATOC(lo_mem * 1024);
    hi_clicks = BATOC(hi_mem * 1024);
    maxclicks = BATOC(HI_MEM_START) + hi_clicks;

#ifdef MM_DEBUG
    /* you'll need to enable CRT_TRACE to see this */
    printf("mm_initseg: lo_clicks=%ld, hi_clicks=%ld\n", lo_clicks, hi_clicks);
#endif

    seg_reg_memchunk((phys_clicks) 0, lo_clicks);
    seg_reg_memchunk(BATOC(HI_MEM_START), hi_clicks);
    begin_click = BATOC((long) &kernelstart);
    end_click = BATOC((long) &end) + 1;
    seg_mark_used(begin_click, end_click - begin_click, (long) ST_KERNEL);
    seg_mark_used((phys_clicks) 0, BSTOC(0x2000), (long) ST_KERNEL);
}
#endif /* defined(ISA) || defined(MCA) */

/*
 * Initialize memory management. Basically setting up the
 * kernel page tables, and user page table directories.
 */
void
initmm()
{
    register int i, j;
    long *ktp, entry;

    /*
     * The 80386 uses a two level page table. The first level consists
     * of a page directory with entries pointing to the second level
     * page tables which refer to the actual pages. For historical
     * reasons the 80386 Amoeba kernel is loaded at 0x2000, leaving the
     * lowest 8 Kb available for boot info/parameters.
     */
    nkmaps = maxclicks / MAP_CLICKS;
    kpd = (long *) aalloc((vir_bytes) PAGE_TABLE_SIZE, CLICKSIZE);
    for (i = 0; i <= nkmaps; i++) {
	kpd[i] = (long) aalloc((vir_bytes) PAGE_TABLE_SIZE, CLICKSIZE);
	ktp = (long *) kpd[i];
	kpd[i] |= PG_P; /* page-table page is present */
	entry = (PG_P | PTE_RW) | (CTOB(MAP_CLICKS) * i);
	for (j = 0; j < PAGE_TABLE_ENTRIES; j++) {
	    ktp[j] = entry;
	    entry += PAGE_SIZE;
	}
#ifndef PAGE0_MAPPEDIN
	if (i == 0) ktp[0] &= ~PG_P;
#endif
    }

#if defined(ISA) || defined(MCA)
    /*
     * On ISA bus architecture the memory space between 640Kb and 1Mb
     * is reserved for memory mapped I/O devices and system ROMs. To
     * prevent accidental overwrites, we unmap the pages, the individual
     * drivers should map the pages back in when they need it.
     */
    ktp = (long *) (kpd[0] & ~(CLICKSIZE - 1));
    for (i = PAGE_640KB; i < PAGE_1000KB; i++)
	ktp[i] &= ~PG_P;
#endif /* defined(ISA) || defined(MCA) */

    /*
     * Enable paging
     */
    page_enable(kpd);

#ifndef NOPROC
    /*
     * For every (potential) process in the system we allocate
     * a user page table directory and map in the kernel pages.
     */
    memmap = (struct memmap **) aalloc((vir_bytes) (nproc * sizeof(*memmap)), 0);
    (void) memset((_VOIDSTAR) memmap, 0, nproc * sizeof(*memmap));
    for (i = 0; i < (int)nproc; i++) {
	/* allocate map for process i */
	memmap[i] = (struct memmap *) getblk((vir_bytes) sizeof(struct memmap));
	(void) memset((_VOIDSTAR) memmap[i], 0, sizeof(struct memmap));
	/* map kernel space into user space */
	for (j = 0; j <= nkmaps; j++)
	    memmap[i]->upd[j] = kpd[j];
    }
#endif /* NOPROC */
}

/*
 * Stop memory management
 */
void
stopmm()
{
    page_disable();
}

#if defined(ISA) || defined(MCA)
/*
 * Map device pages at address `addr' with total size `size'
 */
void
page_mapin(addr, size)
    phys_bytes addr, size;
{
    register int i;
    int low, high;
    long *ktp;

    low = addr >> CLICKSHIFT;
    high = (addr + size + CLICKSIZE - 1) >> CLICKSHIFT;
    if (low < PAGE_640KB || low >= PAGE_1000KB)
	panic("page_mapin: invalid lower bound (%x)\n", low);
    if (high < PAGE_640KB || high >= PAGE_1000KB)
	panic("page_mapin: invalid upper bound (%x)\n", high);

    ktp = (long *) (kpd[0] & ~(CLICKSIZE - 1));
    for (i = low; i < high; i++)
	ktp[i] |= PG_P;
    flushTLB();
}
#endif /* defined(ISA) || defined(MCA) */
