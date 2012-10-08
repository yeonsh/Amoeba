/*	@(#)mm.c	1.6	96/02/27 14:00:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "map.h"
#include "process/proc.h"
#include "psr.h"
#include "microsparc.h"
#include "sparc-refmmu.h"
#include "mmuconst.h"
#include "openprom.h"
#include "seg.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "sys/proto.h"
#include "mmuproto.h"

#define ROUNDUP(a, size)	(((a)+(size)-1)&(~((size)-1)))
#define ROUNDDOWN(a, size)	(((a)         )&(~((size)-1)))

#define	MAX(a, b)		((a) >= (b) ? (a) : (b))
#define	MIN(a, b)		((a) <= (b) ? (a) : (b))

/* The following is needed for cache flushing */
static int	Cache_line_size;
static int	Cache_size;
static void	(*Cache_flush_func) _ARGS((vir_bytes, vir_bytes));
static void	(*Cache_flushall_func) _ARGS((void));

/*
 * Reading and writing physical space
 * with some possible debugging printf's
 */

#undef PHYSDEBUG
#ifdef PHYSDEBUG
static uint32
deb_rd_phys_addr(x)
uint32 x;
{
	uint32 r;

	r = rd_phys_addr(x);
	DPRINTF(2, ("rd_phys_addr(%x) = %x\n", x, r));
	return r;
}

static
deb_wr_phys_addr(x, v)
uint32 x;
uint32 v;
{

	DPRINTF(2, ("wr_phys_addr(%x, %x)\n", x, v));
	wr_phys_addr(x, v);
}

#define RD_PHYS_ADDR deb_rd_phys_addr
#define WR_PHYS_ADDR deb_wr_phys_addr

#else

#define RD_PHYS_ADDR rd_phys_addr
#define WR_PHYS_ADDR wr_phys_addr

#endif /* PHYSDEBUG */

/*
 * Chunks of physical memory found by prom
 * We do not use small pieces because of paranoia
 * The fear is that the prom might start using them any minute
 *
 * If only we knew what we were doing.....
 */

#define PM_MAXCHUNKS	12
struct phys_mem physmem[ PM_MAXCHUNKS ]; /* Array of all physical memory */
int pm_maxchunk;                        /* Number of entries in above */

#define MIN_MEM_CHUNKSIZE 0x100000	/* Anything less than a meg .. */

vir_bytes mmu_maxaddr;			/* Communicates end of mem to cpu.c */

static vir_bytes mmu_ioaddr, mmu_maxioaddr;	/* For mapping of IO devices */
#define	NIOPAGES	288		/* 1.25 MB */

/*
 * Memory management info per process
 *
 * A pool of 256 byte buffers is held per process, to use for level 2 and 3
 * tables. There will be some reasonable amount of space per user process.
 */

struct mmap {
	struct mmap *mm_next;
	uint32	     mm_addr;
};

struct softmm {
	uint32	smm_level1;		/* Physical address of level 1 map */
	struct mmap * smm_list;
};

static struct softmm kernel_soft;	/* Kernel info */

struct mmap *mmap_freelist;		/* List of free mmap's */

#ifdef MM_DEBUG
/* Forward */
int dump_map_usage _ARGS(( char *, char * ));
#endif

static uint32
alloc_map(smap)
struct softmm *smap;
{
	struct mmap *mmel;
	uint32 addr;
	uint32 i;

	mmel = mmap_freelist;
	if (mmel == 0) {
		printf("Out of MMU map buffers\n");
#ifdef MM_DEBUG
		(void) dump_map_usage((char *) 0, (char *) 0);
#endif
		return 0;
	}
	mmap_freelist = mmel->mm_next;
	mmel->mm_next = smap->smm_list;
	smap->smm_list = mmel;
	addr = mmel->mm_addr;
	for (i = 0; i < 64*4; i += 4)
		wr_phys_addr(addr + i, (uint32) 0);
	return (addr >> 4) | PTE_ET_PTP;
}

static int
enter_mapping(virt, pte, smap)
uint32 virt;
uint32 pte;
struct softmm *smap;
{
	uint32	l1i, l2i, l3i;
	uint32	     l2a, l3a;

	assert((virt&(PAGESIZE-1)) == 0);

	l3i = virt >> PAGESHIFT;
	l2i = l3i >> 6;
	l3i &= 63;
	l1i = l2i >> 6;
	l2i &= 63;
	compare (l1i, <, 256);

	DPRINTF(2, ("enter_mapping(%x, %x, %x) ind=%x,%x,%x\n",
					virt, pte, smap, l1i, l2i, l3i ));

	l2a = rd_phys_addr(smap->smm_level1 + 4*l1i);
	if ((l2a&PTE_ET_MASK)!=PTE_ET_PTP) {
		/* Allocate map */
		l2a = alloc_map(smap);
		if (l2a == 0)
			return 0;
		WR_PHYS_ADDR(smap->smm_level1 + 4*l1i, l2a);
	}
	l2a = (l2a & ~PTE_ET_MASK) << 4;

	l3a = rd_phys_addr(l2a + 4*l2i);
	if ((l3a&PTE_ET_MASK)!=PTE_ET_PTP) {
		/* Allocate map */
		l3a = alloc_map(smap);
		if (l3a == 0)
			return 0;
		WR_PHYS_ADDR(l2a + 4*l2i, l3a);
	}
	l3a = (l3a & ~PTE_ET_MASK) << 4;

	WR_PHYS_ADDR(l3a + 4*l3i, pte);
	return 1;
}

initmmearly(){}

static uint32 *kernel_level2_table;
static int total_level2;

static uint32 *kernel_level3_table;
static int total_level3;

static uint32	prom_ctx_table;		/* physical address of CTX table */

uint32 p_addr(v)
uint32 v;
{
	long pagebits;
	long pte;

	compare (v, >=, KERNBASE);
	compare (v, <, mmu_maxaddr);
	v -= KERNBASE;
	pagebits = v&(PAGESIZE-1);
	v >>= PAGESHIFT;
	pte = kernel_level3_table[v];
	assert((pte&PTE_ET_MASK)==PTE_ET_PTE);
	pte &= PTE_PPN_MASK;
	pte <<= 4;
	return pte|pagebits;
}

extern int *sparc_prbeg, *sparc_prend;	/* Beginning and end of probe code */
extern int sparc_nofault;		/* Tell sparc_probe() that we faulted */

/*
 * mmu_pagefault() (INTERRUPT HANDLER) -- deal with a page fault
 */
/*ARGSUSED*/
mmu_pagefault( reason, framep, pc, psr, pid, addr )
int reason;				/* Trap type (from TBR) */
thread_ustate *framep;			/* Frame on kernel stack */
int pc;					/* Address of trapping instruction */
int psr;				/* Original PSR (right after trap) */
int pid;			       	/* Process ID number */
vir_bytes addr;				/* Address causing fault */
{
    int probing = (( sparc_prbeg <= (int *) pc ) && ( (int *) pc < sparc_prend ));

    DPRINTF( 1, ( "mmu_pagefault(%x): pid %x, sp %08x, pc %x, psr %x, address %x%s\n",
	   reason, pid, framep, pc, psr, addr, probing ? " probing" : "" ));
    if ( USERMODE( psr ) ) {
	framep->tu_global[0] = addr;	/* Use %g0 storage for fault address */
	puttrap ( reason );
	return;
    }
    printf( "mmu_pagefault(%x): pid %x, sp %08x, pc %x, psr %x, address %x%s\n",
	   reason, pid, framep, pc, psr, addr, probing ? " probing" : "" );
    panic("mmu_pagefault");
}

static void
mmu_flush_tlb()
{

    mm_flush(0x400);	/* Flush the TLB */
}


/*
 *  Cache handling routines for MicroSPARC I
 */

static void
msI_cache_flush_all()
{
    msI_data_cache_flash_clear(0, 0);
    msI_instr_cache_flash_clear();
}


/*
 *  Cache handling routines for MicroSPARC II
 */

static void
msII_cache_flush(addr, size)
vir_bytes	addr;
vir_bytes	size;
{
    vir_bytes	align;

    assert(size > 0);

    DPRINTF(-2, ("cache_flush(%x, %x)\n", addr, size));
    /*
     * Align the address with the start of a cache line and add enough
     * to the size to cover the difference.
     */
    align = addr & (Cache_line_size - 1);
    addr -= align;
    size += align;
    msII_do_cache_flush(addr, size, Cache_line_size);
}

static void
msII_cache_flush_all()
{
    msII_do_cache_flush_all(Cache_size, Cache_line_size);
}


/*
 * Generic cache handling: flush from addr to (addr + size - 1) from the
 * (data) cache.
 */
void
cache_flush(addr, size)
vir_bytes	addr;
vir_bytes	size;
{
    (*Cache_flush_func)(addr, size);
}

/*
 * Trash both caches :-)
 */
void
cache_flush_all()
{
    (*Cache_flushall_func)();
}

/*
 * Switch both instruction and data caches on
 */
static void
cache_enable()
{
    uint32	pcr;

    pcr = rd_mmu_reg(MMREG_PCR);
    pcr |= PCR_IE|PCR_DE;
    wr_mmu_reg(MMREG_PCR, pcr);
}

/*
 * Switch both instruction and data caches off
 */
static void
cache_disable()
{
    uint32	pcr;

    disable();
    cache_flush_all();
    pcr = rd_mmu_reg(MMREG_PCR);
    pcr &= ~(PCR_IE|PCR_DE);
    wr_mmu_reg(MMREG_PCR, pcr);
    enable();
}

#ifndef NOPROC

extern uint16 nproc;

static struct softmm *user_soft;

extern void fast_flush_windows();
extern void start();

#ifdef MM_DEBUG

static long	Totmaps;

/* Count the # mmaps in the list pointed to by l */
static long
smm_total(l)
struct mmap * l;
{
    long n;

    for (n = 0; l != (struct mmap *) 0; l = l->mm_next)
	n++;
    return n;
}

int
dump_map_usage(beg, end)
char *	beg;
char *	end;
{
    int i;
    long sum;
    long n;
    char * p;

    p = bprintf(beg, end, "MMAP debug dump:\n");
    for (sum = 0, i = 0; i < (int) nproc; i++)
    {
	n = smm_total(user_soft[i].smm_list);
	p = bprintf(p, end, "Proc %d has %d mmaps\n", i, n);
	sum += n;
    }
    p = bprintf(p, end, "Total maps in use  = %d\n", sum);
    p = bprintf(p, end, "Total maps free    = %d\n", smm_total(mmap_freelist));
    p = bprintf(p, end, "Total maps alloced = %d\n", Totmaps);
    if (p == 0)
	return end - beg; /* we overfilled the buffer */
    else
	return p - beg;
}
#endif /* MM_DEBUG */

/*
 * startuser() (ENTRY POINT) -- start the given user process going with the
 * pc and sp provided. This does get a little tricky: We make a struct
 * thread_ustate on the kernel stack that start() will make the current frame.
 * This means that we will never return to here or to start(). start() is
 * responsible for filling in the tu_fp and tu_retaddr of the thead_ustate
 * we hand to it.
 */
startuser( pid, pc, sp )
uint16 pid;
vir_bytes pc;
vir_bytes sp;
{
    thread_ustate tu;


    DPRINTF(1, ("startuser(%d, %x, %x)\n", pid, pc, sp));
    fast_flush_windows();

    (void) memset( (_VOIDSTAR) &tu, 0, sizeof( tu ));
    tu.tu_psr = USER_PSR;
    tu.tu_pc = pc;
    tu.tu_npc = MAKE_NPC( pc );
    tu.tu_pid = pid;
    tu.tu_fp = ( sp == 0 ) ? -1 : ROUNDDOWN( (int) sp - MINFRAME, MAXALIGN );
    tu.tu_sf.sf_in[ 0 ] = sp;
    tu.tu_retaddr = -1;

    start( &tu );
    panic( "startuser: start returned" );
}

/*ARGSUSED*/
hintmap(pid, nbytes)
uint16 pid;
vir_bytes nbytes;
{

	/*
	 * Utterly useless call. Goes down to the days of the VAX,
	 * probably combined with sloppy programming.
	 *
	 * We ignore it.
	 */
}

usemap( pid )
uint16 pid;
{
	uint32	oldctx;

	compare(pid, <, nproc);
	oldctx = rd_mmu_reg (MMREG_CR);
	if (oldctx != pid) {
		DPRINTF(1, ("Switch from context %d to %d\n", oldctx, pid));
		wr_mmu_reg (MMREG_CR, (uint32) pid);
	}
}

/*
 * setmap() (ENTRY POINT) -- add the given chunk of physical memory into the
 * memory map of the given process' address space at the virtual address
 * provided.
 */
setmap( pid, vir, physc, len, type )
uint16 pid;
vir_clicks vir;
phys_clicks physc;
vir_clicks len;
long type;
{
    uint32 pte_flags;
    struct softmm *smm;
    vir_bytes addr_bytes, len_bytes;
    phys_bytes phys;
    uint32 realphys;

    assert (pid);
    compare (pid, <, nproc);
    if (( type & MAP_PROTMASK ) == MAP_READWRITE )
#ifndef NO_ALLOW_DATA_EXEC
	pte_flags = PTE_ACC_RWXRWX | PTE_C | PTE_ET_PTE;
#else
	pte_flags = PTE_ACC_RW_RW_ | PTE_C | PTE_ET_PTE;
#endif
    else {
	if (( type & MAP_PROTMASK ) == MAP_READONLY )
	    pte_flags = PTE_ACC_R_XR_X | PTE_C | PTE_ET_PTE;
	else {
	    /* unmap it ! */
	    pte_flags = 0;
	}
    }
#ifdef DONT_CACHE_HW_SEG
    if (type & MAP_INPLACE) {
	/* don't cache device or DMA segment */
	pte_flags &= ~PTE_C;
    }
#endif
    DPRINTF(1, ("setmap(%d, %x, %x, %x, %x) flags = %x\n",
					pid, vir, physc, len, type, pte_flags));
    smm = user_soft + pid;
    addr_bytes = vir << CLICKSHIFT;
    len_bytes = len << CLICKSHIFT;
    phys = physc << CLICKSHIFT;
    while (len_bytes) {
	if (phys >= KERNBASE) {
		/* Memory */
		realphys = pte_flags ? p_addr(phys) : 0;
	} else {
		/* Hardware Seg mapping */
#ifdef notdef	/* not needed, devices disable own caching */
		if (len == 1) /* something small, device reg, don't cache */
			pte_flags &= ~PTE_C;
#endif
		realphys = pte_flags ? phys : 0;
	}
	if (!enter_mapping(addr_bytes, (realphys>>4) | pte_flags, smm))
	    return -1;
	addr_bytes += PAGESIZE;
	phys += PAGESIZE;
	len_bytes -= PAGESIZE;
    }

    mmu_flush_tlb();

    return 0;
}

clrmap(pid)
uint16 pid;
{
    struct softmm *ummp;
    int l1;
    struct mmap *smmp, *fmmp;

    assert(pid);
    compare(pid, <, nproc);

    DPRINTF(1, ("clrmap(%d)\n", pid));

    ummp = user_soft + pid;
    smmp = ummp->smm_list;
    if (smmp != 0) {
	/* Move the whole list to the free list */
	for (fmmp = smmp; fmmp->mm_next; fmmp = fmmp->mm_next)
	    ;
	fmmp->mm_next = mmap_freelist;
	mmap_freelist = smmp;
	ummp->smm_list = 0;
    }
    /*
     * Clear all access to lower half of address space
     */
    for (l1 = 0; l1 < 512; l1 += 4)
	wr_phys_addr(ummp->smm_level1 + l1, (uint32) 0);
}

static uint32
init_user_mm(endp, nclicks)
uint32 endp;
phys_clicks nclicks;
{
    uint32 ptabspace;
    int procno;
    int l1;
    struct softmm *ummp;
    int i, nmm;
    struct mmap *mmp;

    endp = ROUNDUP(endp, sizeof(uint32));

    user_soft = (struct softmm *) endp;
    endp += nproc * sizeof(struct softmm);

    nmm = nclicks >> (5 - (CLICKSHIFT - PAGESHIFT));
#ifdef MM_DEBUG
    Totmaps = nmm;
#endif
    mmp = (struct mmap *) endp;
    endp += nmm * sizeof(struct mmap);
    DPRINTF(0, ("Allocated %d entries for mmap table\n", nmm));

    endp = ROUNDUP(endp, 256);

    ptabspace = endp - KERNBASE;
    endp += nmm * 256;
    for (i=0; i<nmm; i++) {
	mmp[i].mm_addr = ptabspace +i*256;
	mmp[i].mm_next = mmap_freelist;
	mmap_freelist = &mmp[i];
    }
    DPRINTF(0, ("Allocated 0x%x bytes for mmap blocks\n", nmm * 256));

    endp = ROUNDUP(endp, 1024);
    ptabspace = endp - KERNBASE;
    endp += (nproc-1) * 1024;

    for (procno=1, ummp = user_soft+1; procno < (int)nproc; procno++, ummp++) {
	ummp->smm_level1 = ptabspace;
	/*
	 * Bottom half of address space unaccessable for now
	 */
	for (l1 = 0; l1 < 512; l1 += 4)
	    wr_phys_addr(ptabspace + l1, (uint32) 0);
	/*
	 * Top half copied from kernel
	 */
	for ( ; l1 < 1024 ; l1 += 4)
	    wr_phys_addr(ptabspace + l1,
				rd_phys_addr(kernel_soft.smm_level1 + l1));
	ummp->smm_list = 0;
	ptabspace += 1024;
	WR_PHYS_ADDR(prom_ctx_table + 4*procno,
				(ummp->smm_level1>>4) | PTE_ET_PTP);
	DPRINTF(2, ("proc %2d: level1 = %6x\n", procno, ummp->smm_level1));
    }

    return endp;
}
#endif /* NOPROC */

static void
get_cpu_attrs(p)
nodelist *	p;
{
    int	proplen;
    int	dcache_lsz;
    int	icache_lsz;
    int	dcache_lines;
    int	icache_lines;
    int cpu_type;

    /*
     * Get the cache-line sizes
     */
    proplen = obp_getattr(p->l_curnode, "dcache-line-size",
			  (void *) &dcache_lsz, sizeof dcache_lsz);
    if (proplen != sizeof dcache_lsz)
	dcache_lsz = 0;
    proplen = obp_getattr(p->l_curnode, "icache-line-size",
			  (void *) &icache_lsz, sizeof icache_lsz);
    if (proplen != sizeof icache_lsz)
	icache_lsz = 0;

    if (dcache_lsz == 0 && icache_lsz == 0)
    {
	proplen = obp_getattr(p->l_curnode, "icache-line-size",
			      (void *) &icache_lsz, sizeof icache_lsz);
	if (proplen != sizeof icache_lsz)
	{
	    panic("can't find cache-line-size info");
	    /*NOTREACHED*/
	}
    }
    if (dcache_lsz == 0)
    {
	Cache_line_size = icache_lsz;
    }
    else
    {
	if (icache_lsz == 0)
	{
	    Cache_line_size = dcache_lsz;
	}
	else
	{
	    Cache_line_size = MIN(dcache_lsz, icache_lsz);
	}
    }
    DPRINTF(-2, ("get_cpu_attrs: Cache_line_size = %d\n", Cache_line_size));

    proplen = obp_getattr(p->l_curnode, "dcache-nlines",
			  (void *) &dcache_lines, sizeof dcache_lines);
    assert(proplen == sizeof dcache_lines);
    proplen = obp_getattr(p->l_curnode, "icache-nlines",
			  (void *) &icache_lines, sizeof icache_lines);
    assert(proplen == sizeof icache_lines);
    Cache_size = MAX(dcache_lines * dcache_lsz, icache_lines * icache_lsz);

    /* Work out the cpu_type */
    cpu_type = rd_mmu_reg(MMREG_PCR) >> 24;
    switch (cpu_type)
    {
    case MICROSPARC_I:
	Cache_flush_func = msI_data_cache_flash_clear;
	Cache_flushall_func = msI_cache_flush_all;
	break;

    case MICROSPARC_II:
	Cache_flush_func = msII_cache_flush;
	Cache_flushall_func = msII_cache_flush_all;
	break;

    default:
	panic("Unknown system id: %d\n", cpu_type);
	break;
    }
}

void
initmm() {
    phys_clicks total;
    phys_clicks nclicks;
    int i,p;
    uint32 *l3p;
    extern char end;
    uint32 kernel_end;
	
    /*
     * Get CPU attributes that might be interesting
     */
    obp_regnode("device_type", "cpu", get_cpu_attrs);
    
    mmu_flush_tlb();
    kernel_end = ROUNDUP((uint32) &end, PAGESIZE);

    /*
     * Find all available memory, skipping fiddling small change
     */
    pm_maxchunk = obp_availmem(PM_MAXCHUNKS, physmem);
    total = 0;
    for ( i = 0; i < pm_maxchunk; i++ ) {
        DPRINTF(1, ("Found mem-chunk %d at %x, len %x\n",
                        i, physmem[i].phm_addr, physmem[i].phm_len));
	if ( physmem[i].phm_len >= MIN_MEM_CHUNKSIZE ) {
		total += BTOPG( physmem[i].phm_len );
	}
    }

    nclicks = total>>(CLICKSHIFT-PAGESHIFT);
    mmu_maxaddr = KERNBASE + (total<<PAGESHIFT);
    DPRINTF(2, ("mmu_maxaddr = %x\n", mmu_maxaddr));

    mmu_ioaddr = mmu_maxaddr;
    total += NIOPAGES;
    mmu_maxioaddr = KERNBASE + (total<<PAGESHIFT);
    DPRINTF(2, ("mmu_ioaddr = %x, mmu_maxioaddr = %x\n", mmu_ioaddr, mmu_maxioaddr));

    total_level2 = ROUNDUP(total, 1<<MMU_LEVEL1_SHIFT) >> MMU_LEVEL1_SHIFT;
    kernel_level2_table = (uint32 *) kernel_end;
    kernel_end += 256 * total_level2;
    memset((_VOIDSTAR) kernel_level2_table, 0, (size_t) (256 * total_level2));

    total_level3 = ROUNDUP(total, 1<<MMU_LEVEL2_SHIFT) >> MMU_LEVEL2_SHIFT;
    kernel_level3_table = (uint32 *) kernel_end;
    kernel_end += 256 * total_level3;
    memset((_VOIDSTAR) kernel_level3_table, 0, (size_t) (256 * total_level3));

    /*
     * Now fill in level 3 table with page entries
     */

    l3p = kernel_level3_table;
    for ( i = 0; i < pm_maxchunk; i++ ) {
	if ( physmem[i].phm_len >= MIN_MEM_CHUNKSIZE ) {
	    int npages, pagebase;

	    npages = physmem[i].phm_len>>PAGESHIFT;
	    pagebase = physmem[i].phm_addr>>PAGESHIFT;
	    for (p=0; p<npages; p++) {
		*l3p++ = ((pagebase+p)<<PTE_PPN_SHIFT)|PTE_C|PTE_ACC____RWX|PTE_ET_PTE;
	    }
	}
    }

    /*
     * Fill in level 2 table with level 3 pointer entries
     */
    
    for (i=0; i<total_level3; i++) {
	kernel_level2_table[i] = (p_addr((uint32) &kernel_level3_table[64*i]) >> 4)|
					PTE_ET_PTP;
	/* printf("kernel_level2_table[%d] = %x\n", i, kernel_level2_table[i]); */
    }

    /*
     * Stash level 2 pointer entries into level 1 context 0 table
     */

    prom_ctx_table = rd_mmu_reg(MMREG_CTPR)<<4;
    kernel_soft.smm_level1 = (RD_PHYS_ADDR(prom_ctx_table) & ~PTE_ET_MASK)<<4;
    DPRINTF(2, ("prom_ctx_table = %x, kernel_soft.smm_level1=%x\n", prom_ctx_table, kernel_soft.smm_level1));
    /*
     * Write entries backward now. The lowest numbered one is the one
     * we are currently running from.
     */
    for (i=total_level2-1; i>=0; i--) {
	WR_PHYS_ADDR(kernel_soft.smm_level1 +
		(((unsigned)KERNBASE)>>(MMU_LEVEL1_SHIFT+PAGESHIFT-2)) + (i<<2),
			(p_addr((uint32) &kernel_level2_table[64*i])>>4)|
				PTE_ET_PTP);
    }

    /*
     * Clear lower half of address space. It should be available
     * for user processes now.
     */

    for (i=0; i<128; i++)
	WR_PHYS_ADDR(kernel_soft.smm_level1 + 4*i, (uint32) 0);

    mmu_flush_tlb();

    /* Insert trap catching hooks here */
    setvec( TRAPV_INSN_ACCESS, mmu_accesstrap );
    setvec( TRAPV_DATA_ACCESS, mmu_accesstrap );

#ifndef NOPROC
    kernel_end = init_user_mm(kernel_end, nclicks);
#endif

    kernel_end = ROUNDUP(kernel_end, PAGESIZE);
    seg_reg_memchunk( BATOC(VTOP(KERNELSTART)), nclicks );
    seg_mark_used( BATOC(VTOP(KERNELSTART)),
		    BSTOC(VTOP(kernel_end - KERNELSTART)), (long) ST_KERNEL );

#ifndef NO_CACHE
    DPRINTF(2, ("about to enable caches\n"));
    cache_enable();
    DPRINTF(2, ("cache now enabled\n"));
#endif
}

vir_bytes
mmu_mapmem(devreg, cachable)
dev_reg_t *devreg;
int cachable; /* 1 => map cachable, 0 => map uncachable */
{
	uint32 pagebits, lowaddr, highaddr, phys;
	uint32 flags;
	int r;
	vir_bytes result;

	compare(devreg->reg_bustype, ==, 0);	/* We don't handle 36 bit */

	lowaddr = ROUNDDOWN(devreg->reg_addr, PAGESIZE);
	pagebits = devreg->reg_addr - lowaddr;
	highaddr = ROUNDUP(devreg->reg_addr+devreg->reg_size, PAGESIZE);

	compare(mmu_ioaddr+highaddr-lowaddr, <=, mmu_maxioaddr);

	/*
	 * When we map in large devices, like NVRAM, then we want to cache
	 * them.  It's only I/O device we don't want to cache.
	 */
	flags = PTE_ACC____RWX | PTE_ET_PTE;
	if (cachable)
	    flags |= PTE_C;

	result = mmu_ioaddr + pagebits;
	for ( phys = lowaddr; phys < highaddr; phys += PAGESIZE ) {
		r = enter_mapping(mmu_ioaddr, (phys>>4)|flags, &kernel_soft);
		assert(r);
		mmu_ioaddr += PAGESIZE;
	}
	return result;
}

vir_bytes
mmu_mapdev(devreg)
dev_reg_t *devreg;
{
    return mmu_mapmem(devreg, 0);
}

/*
 * Get virtual address of device.
 * First try the prom, if not map yourself
 */

vir_bytes
mmu_virtaddr(device)
char *device;
{
    device_info devnodes[1];
    int ndevs;

    ndevs = obp_findnode(device, devnodes, 1);
    compare (ndevs, ==, 1);
    if (devnodes[0].di_vaddr != 0) {
	vir_bytes mapped_by_the_prom;
	uint32 pte_flags;

	mapped_by_the_prom = devnodes[0].di_vaddr;
	pte_flags = mm_probe(mapped_by_the_prom);
	if ((pte_flags & PTE_ACC_RW_RW_) == PTE_ACC_RW_RW_)
		return mapped_by_the_prom;
    }

    /*
     * Else we have to map the physical address in ourselves - the PROM
     * isn't saying where it mapped the device in, or mapped it read-only
     */
    DPRINTF(1, ("mmu_virtaddr found device '%s' at phys addr %x\n",
					    device, devnodes[0].di_paddr.reg_addr));
    assert (devnodes[0].di_paddr.reg_addr != 0 ||
		devnodes[0].di_paddr.reg_size != 0 ||
		devnodes[0].di_paddr.reg_bustype != 0);
    return mmu_mapdev(&devnodes[0].di_paddr);
}

void
mmu_dontcache(addr, size)
vir_bytes addr, size;
{
	uint32 low_addr, high_addr;
	uint32 pte;
	int r;

	low_addr = ROUNDDOWN((uint32) addr, PAGESIZE);
	high_addr = ROUNDUP((uint32) (addr+size), PAGESIZE);
	DPRINTF(1, ("mmu_dontcache(%x, %x), low=%x, high=%x\n", addr, size, low_addr, high_addr));
	while (low_addr < high_addr) {
		pte = mm_probe(low_addr);
		assert(pte);
		r = enter_mapping(low_addr, pte & ~PTE_C, &kernel_soft);
		assert(r);
		low_addr += PAGESIZE;
	}
	mmu_flush_tlb();
	cache_flush(addr, size);
}

mmu_nmi_ack()
{ 

	/*
	 * Must be a leaf routine, no subroutine calls allowed
	 */
}

#ifndef SMALL_KERNEL
static char *string_access[] = {
	"R__R__",
	"RW_RW_",
	"R_XR_X",
	"RWXRWX",
	"__X__X",
	"R__RW_",
	"___R_X",
	"___RWX"
};

static char *string_entry_type[] = {
	"INV",
	"PTP",
	"PTE",
	"???"
};

static int chunkshift[] = {
	PAGESHIFT+MMU_LEVEL1_SHIFT,
	PAGESHIFT+MMU_LEVEL2_SHIFT,
	PAGESHIFT+MMU_LEVEL3_SHIFT
};

static char *
space(p, end, nspaces)
char *p, *end;
{

	while (nspaces>=8) {
		p = bprintf(p, end, "\t");
		nspaces -= 8;
	}
	while (nspaces>0) {
		p = bprintf(p, end, " ");
		nspaces--;
	}
	return p;
}

static char *
indent(p, end, base, level, underway)
char *p, *end;
uint32 base;
{
	if (underway)
		p = bprintf(p, end, "\n");
	p = bprintf(p, end, "%8x", base);
	p = space(p, end, 3+2*level);
	return p;
}

static char *
dump_mm_tree(p, end, treep, base, level, highbit)
char *p, *end;
uint32 treep, base;
uint32 highbit;
{
	uint32 nextlevel;
	int i,lowi,highi;
	static lastpaddr;
	static underway;
	int paddr;
	int tree;

	tree = rd_phys_addr(treep);
	if (level == 3 && (tree&PTE_ET_MASK) == PTE_ET_PTE) {
		paddr = (tree&PTE_PPN_MASK)<<(12-PTE_PPN_SHIFT);
		if (paddr != lastpaddr + (1<<12)) {
			p = indent(p, end, base, level, underway);
			p = bprintf(p, end, "%8x ", paddr);
		}
		if (!(tree&PTE_C)) p = bprintf(p, end, "!C");
		if (tree&PTE_M) p = bprintf(p, end, "M");
		if (tree&PTE_R) p = bprintf(p, end, "R");
		p = bprintf(p, end, ",");
		lastpaddr = paddr;
		underway = 1;
		return p;
	}
	lastpaddr = -1;
	switch(tree&PTE_ET_MASK) {
	case PTE_ET_INVALID:
		return p;
	case PTE_ET_PTP:
		if (level == 0) {
			if (highbit) {
				lowi = 128; highi = 256;
			} else {
				lowi = 0; highi = 128;
			}
		} else {
			lowi = 0; highi = 64;
		}
		nextlevel = (tree & (~PTE_ET_MASK))<<4;
#ifdef notdef
		p = indent(p, end, base, level, underway);
		p = bprintf(p, end, "ptp%d @ %x\n", level, nextlevel);
		underway = 0;
#endif
		for (i=lowi; i<highi; i++)
			p = dump_mm_tree(p, end, nextlevel + 4*i,
				base+(i<<chunkshift[level]), level+1, highbit);
		return p;
	case PTE_ET_PTE:
		p = indent(p, end, base, level, underway);
		p = bprintf(p, end, "%8x %c %c %c %s\n",
			(tree&PTE_PPN_MASK)<<(12-PTE_PPN_SHIFT),
			tree&PTE_C ? 'C' : '-',
			tree&PTE_M ? 'M' : '-',
			tree&PTE_R ? 'R' : '-',
			string_access[(tree&PTE_ACC_MASK)>>PTE_ACC_SHIFT]);
		underway = 0;
		return p;
	default:
		p = indent(p, end, base, level, underway);
		p = bprintf(p, end, "impossible\n");
		underway = 0;
		return p;
	}
	/*NOTREACHED*/
}

static char *
dump_mm_list(p, end, l)
char *p, *end;
struct mmap *l;
{

	p = bprintf(p, end, "list of mmap's:");
	while (l) {
		p = bprintf(p, end, " %x", l->mm_addr);
		l = l->mm_next;
	}
	return bprintf(p, end, "\n");
}

mmu_dump(begin, end)
char *begin, *end;
{
	char *p;
	uint32 ctpr;
	long cr;
	long i;
	extern struct openboot *prom_devp;

	ctpr = rd_mmu_reg(MMREG_CTPR);
	ctpr <<= 4;
	p = bprintf(begin, end, "Context table at physical %x\n", ctpr);
	p = bprintf(p, end, "Kernel part of address space\n");
	if (kernel_soft.smm_list != 0)
		p = dump_mm_list(p, end, kernel_soft.smm_list);
	p = dump_mm_tree(p, end, ctpr, (uint32) 0, 0, (uint32) 1);
	for (i = 0; i < (long) nproc; i++) {
		if (user_soft[i].smm_list == 0)
			continue;
		p = bprintf(p, end, "\nUser part of address space of context %d\n", i);
		p = dump_mm_list(p, end, user_soft[i].smm_list);
		p = dump_mm_tree(p, end, ctpr + 4*i, (uint32) 0, 0, (uint32) 0);
	}
	p = bprintf(p, end, "\n\nFree");
	p = dump_mm_list(p, end, mmap_freelist);
	cr = rd_mmu_reg(MMREG_CR);
	p = bprintf(p, end, "\ncr = %x\n", cr);
#ifdef notdef
	for(i=0; i<32; i++) {
		long pte, tag;

		pte = rd_tlb_entry(i*8);
		tag = rd_tlb_entry(i*8+4);
		p = bprintf(p, end, "%3d: ", i);
		p = bprintf(p, end, "%9x %2d %c%d %c %s %c",
			tag&TAG_VA_MASK,
			(tag&TAG_CTX_MASK)>>TAG_CTX_SHIFT,
			tag&TAG_V ? 'V' : 'I',
			(tag&TAG_LEVEL_MASK)>>TAG_LEVEL_SHIFT,
			tag&TAG_S ? 'S' : '-',
			tag&TAG_IO ? "IO" : " -",
			tag&TAG_PTP ? 'P' : '-');
		p = bprintf(p, end, "%12x %c %c %c %s %s\n",
			(pte&PTE_PPN_MASK)<<(12-PTE_PPN_SHIFT),
			pte&PTE_C ? 'C' : '-',
			pte&PTE_M ? 'M' : '-',
			pte&PTE_R ? 'R' : '-',
			string_access[(pte&PTE_ACC_MASK)>>PTE_ACC_SHIFT],
			string_entry_type[pte&PTE_ET_MASK]);
	}
#endif
	if (p == 0)
	    return end - begin; /* we overfilled the buffer */
	else
	    return p - begin;
}
#endif /* SMALL_KERNEL */

/*
 * reboot() (ENTRY POINT) -- Reboot the machine, using the monitor
 */
reboot( begin, end )
char *begin;
char *end;
{
	char bootargs[256];

	bprintf( begin, end, "rebooting...\n" );
#ifdef NO_KERNEL_SECURITY
	(void) strcpy( bootargs, *prom_devp->op_bootpath );
	(void) strcat( bootargs, *prom_devp->op_bootargs );
#else
	bootargs[0] = '\0';
#endif
	(*prom_devp->op_boot)( bootargs );
	/*NOTREACHED*/
}

void
abort()
{

	reboot( (char *) 0, (char *) 0);
}

/*
 * A kernel has been loaded somewhere into memory.  We want to start it
 * running to replace us.  We add 0x20 to the entry point to skip over
 * the a.out header.
 */

/*ARGSUSED*/
void
bootkernel(entry, size, commandline, flags)
phys_bytes entry;
long size;
char *commandline;
int flags;
{
    static int (*start)(); /* static to avoid GAS bug */

    start = (int (*)()) (entry+32);
    printf("Going to start new kernel at 0x%x (really 0x%x) size 0x%x\n",
						   entry, (int) start, size);

    disable();		/* No more interrupts now */
    cache_disable();
    fast_flush_windows();
    mmu_flush_tlb();
    stop_kernel();
    usemap(1);		/* switch to context 1 */
    restore_prom_stack();

    /* And now for the new kernel */
    (*start)(prom_devp);
    panic("bootkernel: new kernel returned");
}
