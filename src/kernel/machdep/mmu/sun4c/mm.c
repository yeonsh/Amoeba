/*	@(#)mm.c	1.7	96/02/27 13:59:41 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: mm.c -- handle the mmu's initialization and operation
 *
 * Author: Philip Shafer, Nov 1990
 *	   (who was on loan from the Univerity of Hawai'i, Manoa)
 * Modified: Hans van Staveren & Greg Sharp, Nov 1992
 *	     - fixed things that made it not portable to the SS2.
 */

#include "amoeba.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "machdep.h"
#include "promid_sun4.h"
#include "global.h"
#include "table.h"
#include "fault.h"
#include "mmuconst.h"
#include "openprom.h"
#include "psr.h"
#include "map.h"
#include "process/proc.h"
#include "seg.h"
#include "string.h"
#include "sys/proto.h"
#include "mmuproto.h"
#include "memaccess.h"

#define	PM_MAXCHUNKS	12

#define kernel_alloc( who, what, size )	{				\
	who = (what *) kernel_end;					\
	kernel_end += (size) * sizeof( what );				\
	kernel_end = (vir_bytes) RNDUP( (int) kernel_end, MAXALIGN );	\
	compare(kernel_end, <=, KERNBASE+5*(1<<MMU_SEGSHIFT));		\
	(void) memset( (_VOIDSTAR) who, 0, (size_t) ((size) * sizeof (what)) );\
}


#define MMU_SETPMEG(a, b, c) do { \
	DPRINTF(1, ("mmu_setpmeg(%x,%x,%x)\n", a, b, c)); \
	mmu_setpmeg(a,b,c); } while(0)

#define MMU_SETPTE_0(a, b, c) do { \
	DPRINTF(1, ("mmu_setpte(%x,%x,%x)\n", a, b, c)); \
	mmu_setpte(a,b,c); } while(0)

#define MMU_SETPTE(a, b, c) do { \
	DPRINTF(1, ("mmu_setpte(%x,%x,%x)\n", a, b, c)); \
	mmu_setpte(a,b,c); } while(0)

/* We set the next pointer to -1 since it can never take that value normally */
#define	LOCK_PMEG(i) \
	do {						\
	    pmegmap[i].pm_next = (struct pmegmap *) -1;	\
	    pmegmap[i].pm_pid = 0;			\
	} while (0)

#define	PMEG_LOCKED(i)	(pmegmap[i].pm_next == (struct pmegmap *) -1)

extern void initmemarea();
extern void start();
/*Forward*/
void mmu_pagefault _ARGS(( int reason, thread_ustate *framep,
				    int pc, int psr, int pid, vir_bytes addr ));

extern int *sparc_prbeg, *sparc_prend;	/* Beginning and end of probe code */
extern int sparc_nofault;		/* Tell sparc_probe() that we faulted */

int dummy_pidtoctxtopidmap;		/* Initial (all zero) map */

/*
 * These two arrays show the current mapping of process id to hardware
 * context. If a resuming process notices that the pidtoctx[ pid ] is
 * "-1", it must find a new context and fill it in before switching. The
 * other array ctxtopid[] is used to find which processes are living in
 * which contexts, so that some care can be taken when trying to re-claim
 * contexts.
 */
int *pidtoctx;				/* Indexed by pid */
int *ctxtopid;				/* Indexed by context */
int lructx;				/* recycle on LRU for now */

struct procmap *pmap;			/* Map of processes */
struct procmap *curpmap;		/* Current process page map */

int *segmap;				/* *all* user-mode ptes (in segments)*/
int smap_free;				/* First segment map free */
int *smap_flist;			/* Segment map free list */

char *kstk_base;			/* Kernel stack memory arena */
int kstk_shift;				/* log2( kstk_size ) */
int kstk_fhead;				/* First kernel stack free */
int *kstk_flist;			/* Kernel stack free list */

struct pmegmap pmegmap[ MMU_MAXPMEG ];	/* PMEG usage map */
/*
 * Least recently stolen PMEG list head and tail.  All pmegs that have not
 * been locked are on this list and are available for PMEG stealing.
 * Free pmegs are added to the front.  Newly allocated pmegs are added at
 * the tail.
 */
struct pmegmap *pmeg_head;	
struct pmegmap *pmeg_tail;	

int *sysmap, syssize;			/* Kernel PTE table */

struct phys_mem physmem[ PM_MAXCHUNKS ]; /* Array of all physical memory */
int pm_maxchunk;			/* Number of entries in above */
vir_bytes mmu_maxaddr;			/* Largest valid kernel address */
vir_bytes mmu_ioaddr;			/* Next address IO will be mapped */
vir_bytes mmu_maxioaddr;		/* End of IO map addresses */

/* Variables for storing the most recent memory error info for NMI traps */
uint32	synch_err;
uint32	synch_vaddr;
uint32	asynch_err;
uint32	asynch_vaddr;

/*
 * Usemap is here because the scheduler wants to call a function with
 * that name.  In our case it doesn't have to do anything.
 */
/*ARGSUSED*/
usemap(ctx)
uint16 ctx;
{
    /* for now we don't do a thing */
}


static volatile uint32 * memerr_reg_p;

static void
init_me_register()
{
    vir_bytes vaddr;

    vaddr = mmu_virtaddr("/memory-error");
    if (vaddr != 0)
	memerr_reg_p = (uint32 *) vaddr;
    else
	printf("init_me_register: couldn't find /memory-error\n");
}


static void
analyze_mem_err()
{
    uint32	merr;

    if (memerr_reg_p != 0 && (merr = *memerr_reg_p) != 0)
    {
	if (merr & MER_PARITY)
	{
	    if (merr & MER_MULTIPLE)
		printf("Got multiple parity errors, one of which is");
	    else
		printf("Got a parity error");
	    printf(" at address %x", asynch_vaddr);
	    if (merr & MER_BYTE3)
		printf(" in bits 31:24");
	    if (merr & MER_BYTE2)
		printf(" in bits 23:16");
	    if (merr & MER_BYTE3)
		printf(" in bits 15:8");
	    if (merr & MER_BYTE3)
		printf(" in bits 7:0");
	    printf("\n");
	}
    }
}


/*
 * NMI trap handler:  in mmutil.S the following 4 variables are set by
 * mmu_nmi_ack which saves the address error registers and then clears
 * the interrupt so that the trap handler can then call this routine.
 */

void
mmu_nmitrap( reason, framep, pc, psr, pid )
int reason;				/* Trap type (from TBR) */
thread_ustate *framep;			/* Frame on kernel stack */
int pc;					/* Address of trapping instruction */
int psr;				/* Original PSR (right after trap) */
int pid;				/* Process ID number */
{
    printf("NMI: framep %x, pc %x, psr %x, pid %x\n", framep, pc, psr, pid);

    if ((asynch_err &= 0xff) != 0)
    {
	printf("NMI: asynchronous memory error at address %x (ASER=%x) ",
					    asynch_vaddr, asynch_err);
	if (asynch_err == ASER_WBACKERR)
	{
	    /* Take a punt and guess that it is really just a pagefault */
	    mmu_pagefault( reason, framep, pc, psr, pid, asynch_vaddr );
	    return;
	}
	if (asynch_err & ASER_WBACKERR)
	    printf("due to invalid PTE");
	if (asynch_err & ASER_TIMEOUT)
	    printf("non-existent device address");
	if (asynch_err & ASER_DVMAERR)
	    printf("due to DVMA error");
	printf("\n");
    }
    else
    {
	printf("NMI: synchronous memory error at address %x (SER=%x)\n",
						synch_vaddr, synch_err & 0xff);
	if (synch_err & SER_MEM_ERR)
	    analyze_mem_err();
    }

    progerror();
}

/*
 * Map the device at physical address specified by 'drp' into some virtual
 * address of our choice.
 * Note that we have only reserved 64 pages of 4K each for mapping.
 */
vir_bytes
mmu_mapdev(drp)
dev_reg_t *drp;
{
    uint_t paddr, pte_flags;
    vir_bytes vaddr;
    int i;

    DPRINTF(1, ("mmu_mapdev: %x, %x\n", drp->reg_addr, drp->reg_size));
    DPRINTF(1, ("mmu_mapdev: ioadr=%x maxioaddr=%x\n", mmu_ioaddr, mmu_maxioaddr));

    vaddr = (drp->reg_addr&(PAGESIZE-1)) | mmu_ioaddr;
    paddr = (drp->reg_addr & 0xfffffff) >>PAGESHIFT;
    pte_flags = PTE_KERNWRITE | PTE_NOCACHE | PTE_SPACE(drp->reg_bustype);
    for (i = (drp->reg_size+PAGESIZE-1)>>PAGESHIFT; i; i--) {
	compare(mmu_ioaddr, <, mmu_maxioaddr);
	MMU_SETPTE( KERN_CTX, mmu_ioaddr, (int) (paddr | pte_flags));
	paddr++;
	mmu_ioaddr += PAGESIZE;
	DPRINTF(1, ("mmu_mapdev: mmu_ioaddr=%x\n", mmu_ioaddr));
    }
    return vaddr;
}

/*
 * Get virtual address of device.
 * First try the prom, if not map yourself(later)
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
	uint_t pte_flags;

	mapped_by_the_prom = devnodes[0].di_vaddr;
	/* We have to pass 4-byte aligned addresses to mmu_getpte */
	pte_flags = mmu_getpte(KERN_CTX, mapped_by_the_prom & ~3);
	if ((pte_flags & PTE_KERNWRITE) == PTE_KERNWRITE)
		return mapped_by_the_prom;
    }

    /*
     * Else we have to map the physical address in ourselves - the PROM
     * isn't saying where it mapped the device in.
     */
    if (devnodes[0].di_paddr.reg_addr != 0 ||
		devnodes[0].di_paddr.reg_size != 0 ||
		devnodes[0].di_paddr.reg_bustype != 0) {
	DPRINTF(0, ("mmu_virtaddr found device '%s' at phys addr (%x,%x,%x)\n",
			device,
			devnodes[0].di_paddr.reg_addr,
			devnodes[0].di_paddr.reg_size,
			devnodes[0].di_paddr.reg_bustype));
	return mmu_mapdev(&devnodes[0].di_paddr);

    }
    panic("mmu_virtaddr: Device %s found but can't get an address", device);
    /*NOTREACHED*/
}


/*
 * mmu_allocpmeg() --  PMEGs are a scarce resource.  Therefore we steal them
 * from someone else if we can't find a free one.
 * It currently uses a "least recently stolen" strategy.  It is implemented
 * with a linked list of all stealable PMEGs.  Locked PMEGs are not in the
 * list and there next pointer has the value (-1).  See locking macros above.
 * This strategy has the danger that it steals a PMEG from a process which is
 * about to run, or worse yet, from the process currently running.  Alas
 * PMEGs don't have an easy way to mark them as "recently used" so we are
 * stuck with "recently stolen".
 */
static int
mmu_allocpmeg(pid, addr, lock)
int pid;			/* Process ID who wants the PMEG */
vir_bytes addr;			/* Address we will need it at */
int lock;			/* Lock the pmeg & take it out of the list */
{
    struct pmegmap * pmp;
    int pmeg;

    /* Take the head of the pmeg LRS list */
    pmp = pmeg_head;
    assert(pmp != 0);
    pmeg_head = pmp->pm_next;

    pmeg = pmp - pmegmap;
    compare(pmeg, >=, 0);
    compare(pmeg, <, machine.mi_pmegs);

    if (pmp->pm_pid != -1) {
	/* PMEG is in use.  Mark it FOD so we can steal it. */
	DPRINTF(-1,
	    ("allocpmeg: stealing pmeg %x from pid %d, ctx %d, addr %x\n",
		pmeg, pmp->pm_pid, pidtoctx[pmp->pm_pid], pmp->pm_address));
	if (pmp->pm_pid == 0) {
	    /* The Kernel: do it in every context. */
	    int ctx;

	    ctx = machine.mi_contexts;
	    while (--ctx >= 0) {
		MMU_SETPMEG( ctx, pmp->pm_address, MMU_FODPMEG );
	    }
	} else {
	    /* User PMEG */
	    if (pidtoctx[pmp->pm_pid] != -1) {
		MMU_SETPMEG( pidtoctx[pmp->pm_pid], pmp->pm_address,
								MMU_FODPMEG );
	    }
	    /* Tell the soft pmeg copy we nicked it */
	    pmap[pmp->pm_pid].pm_seg[SEGNUM(pmp->pm_address)].ps_pmeg =
								MMU_FODPMEG;
	}
	/*
	 * If the cache might contain valid entries we have to get rid of
	 * them.  A cache hit while the PMEG has been stolen is fatal for
	 * us.  It yields an NMI.
	 */
	if (pmp->pm_pid == 0 || pidtoctx[pmp->pm_pid] != -1) {
	    int ctx;

	    if ((ctx = pidtoctx[pmp->pm_pid]) == -1)
		ctx = 0;
	    mmu_flushseg(ctx, pmp->pm_address);
	}
    }
    pmp->pm_address = RNDDN(addr, MMU_SEGSIZE);

    if (lock) {
	compare(pid, ==, 0);
	LOCK_PMEG(pmeg);
    } else {
	/* Add it to the tail of the LRS list */
	pmp->pm_pid = pid;
	pmp->pm_next = 0;
	if (pmeg_head == 0) {
	    pmeg_head = pmp;
	} else {
	    pmeg_tail->pm_next = pmp;
	}
	pmeg_tail = pmp;
    }

    DPRINTF(-1, ("allocpmeg: giving pmeg %x to pid %d, ctx %d, addr %x (%d)\n",
					pmeg, pid, pidtoctx[pid], addr, lock));
    return pmeg;
}


static void
mmu_freepmeg(pmeg)
int pmeg;
{
    struct pmegmap * pmp;
    struct pmegmap * prev;

    assert(!PMEG_LOCKED(pmeg));

    pmp = &pmegmap[pmeg];

    /* The following should only be true when clrmap calls us */
    if (pidtoctx[pmp->pm_pid] != -1)
	MMU_SETPMEG( pidtoctx[pmp->pm_pid], pmp->pm_address, MMU_INVPMEG );

    pmp->pm_pid = -1;
    pmp->pm_address = 0;
    if (pmp != pmeg_head) {
	/* Find the pmeg in the LRS list */
	for (prev = pmeg_head; prev->pm_next != pmp; prev = prev->pm_next) {
	    if (prev == pmeg_tail)
		panic("mmu_freepmeg: corrupt pmeg LRS list\n");
	}
	assert(prev->pm_next == pmp);
	/* Delete it from the list */
	prev->pm_next = pmp->pm_next;
	if (prev->pm_next == 0)
	    pmeg_tail = prev;
	/* Add it at the head of the LRS list */
	pmp->pm_next = pmeg_head;
	pmeg_head = pmp;
    }
}


/*
 * mmu_populate() -- fill the given address range with locked PMEGs.
 */
void
mmu_populate( addr, size )
vir_bytes addr;
int size;
{
    int pmeg, ctx;

    DPRINTF( 0, ( "mmu_populate( %x, %x )\n", addr, size));
    for (size = ( size + MMU_SEGSIZE - 1 ) >> MMU_SEGSHIFT; size > 0; size-- ) {
	pmeg = mmu_allocpmeg( 0, addr, 1 );
	DPRINTF( 0, ( "mmu_populate: %x gets %x\n", addr, pmeg ));
	for ( ctx = machine.mi_contexts - 1 ; ctx >= 0; ctx-- ) {
	    MMU_SETPMEG( ctx, addr, pmeg );
	}
	addr += MMU_SEGSIZE;
    }
}


/*
 * mmu_allocctx() -- recycle hardware contexts, using a simple LRU. This
 * should eventually be replaced by something better.
 */
mmu_allocctx( newpid )
int newpid;
{
    int ctx, pid;

    ctx = lructx;
    if ( ctxtopid[ ctx ] != -1 ) {
	mmu_freectx( ctx );
	ctx = lructx;			/* Re-fetch lructx */
    }
    if ( --lructx < 0 ) lructx = machine.mi_contexts - 1;

    pid = ctxtopid[ ctx ];
    if (pid > 0)
	pidtoctx[ pid ] = -1;

    assert(newpid != 0);
    pidtoctx[ newpid ] = ctx;
    ctxtopid[ ctx ] = newpid;

    DPRINTF( 1, ( "mmu_allocctx: returning %x (ptep=%x)\n",
	   ctx, pmap[ newpid ].pm_seg ));

    mmu_flushctx( ctx, &pmap[ newpid ].pm_seg[ SEGNUM( USERLOW )]);
    return( ctx );
}

/*
 * mmu_freectx() -- release a hardware context
 */
mmu_freectx( ctx )
int ctx;
{
    int pid;

    lructx = ctx;

    DPRINTF( 1, ( "mmu_freectx: free %x\n", ctx ));
    pid = ctxtopid[ ctx ];
    if ( pid != -1 ) {
	if (pid != 0) /* pidtoctx[0] must always be 0 */
	    pidtoctx[ pid ] = -1;
	ctxtopid[ ctx ] = -1;
    }
}

/*
 * mmu_pagefault() (INTERRUPT HANDLER) -- deal with a page fault, which in
 * most cases is a PMEG fault.
 */
/*ARGSUSED*/
void
mmu_pagefault( reason, framep, pc, psr, pid, addr )
int reason;				/* Trap type (from TBR) */
thread_ustate *framep;			/* Frame on kernel stack */
int pc;					/* Address of trapping instruction */
int psr;				/* Original PSR (right after trap) */
int pid;			       	/* Process ID number */
vir_bytes addr;				/* Address causing fault */
{
    int offset, segment, *ptep, pmeg, i, ctx, usermem;
    struct procseg *psp;
    int probing = (( sparc_prbeg <= (int *) pc ) && ( (int *) pc < sparc_prend ));

    DPRINTF( 1, ( "mmu_pagefault(%x): pid %x, sp %08x, pc %x, psr %x, address %x%s\n",
	   reason, pid, framep, pc, psr, addr, probing ? " probing" : "" ));

    pmeg = mmu_getpmeg( pidtoctx[ pid ], addr );
    if ( pmeg != MMU_FODPMEG ) {
	DPRINTF( 1, ( "pagefault: not FODPMEG: %x\n", pmeg ));
	if ( USERMODE( psr )) {
	    /* Use %g0 storage for fault address */
	    framep->tu_global[0] = addr;
	    puttrap( reason );
	    return;
	}
	if ( probing ) {
	    sparc_nofault = 0;
	    return;
	}
	ctx = pidtoctx[ pid ];
	printf("mmu_pagefault: not FODPMEG: pmeg = 0x%x\n", pmeg );
	printf("pid = %d, ctx = %d\n", pid, ctx);
	printf("psr = 0x%x\n", psr);
	printf("pc  = 0x%x\n", pc);
	printf("adr = 0x%x\n", addr);
	printf("reason = %d\n", reason);
	mmu_sync_err(); /* Set synch_err */
	if (synch_err & SER_MEM_ERR)
	    analyze_mem_err();
	if (synch_err & SER_PROT_ERR)
	    printf("Memory protection violation\n");
	if (synch_err & SER_TIMEOUT)
	    printf("Non-existent device/memory access\n");
	if (synch_err & SER_SBUS_ERR)
	    printf("SBUS bus error\n");
	panic( "pagefault" );
    }

    if (USERMODE(psr) && (framep->tu_psr & PSR_EF) && framep->tu_qsize != 0) {
	/* What now */
	printf("FP queue in mmu_pagefault, psr=%x, qsize=%d\n", framep->tu_psr,framep->tu_qsize);
	puttrap( reason );
	return;
    }
    usermem = ( USERMODE( psr ) || addr < KERNBASE );

    if ( usermem ) {
	if ( addr < USERLOW || USERHIGH <= addr ) {
	    puttrap( reason );
	    return;
	}

	assert(pid != 0);
	assert((unsigned) pid < nproc);
	ctx = pidtoctx[ pid ];
	segment = SEGNUM( addr );
	offset = SEGOFF( addr );
	psp = &pmap[ pid ].pm_seg[ segment ];

	if ( psp->ps_pte == 0 ) {
	    DPRINTF( 1, ( "pagefault: invalid memory refernce\n" ));
	    if ( probing ) {
		sparc_nofault = 0;
		return;
	    }
	    puttrap( reason );
	    return;
	}

	ptep = psp->ps_pte;
	if ( ctx == -1 )
	    panic( "pagefault: usermem without context" );
	assert(psp);
	/* Allocate and fill in a PMEG */
	assert(psp->ps_pmeg == MMU_FODPMEG);
	pmeg = psp->ps_pmeg = mmu_allocpmeg( pid, addr, 0 );
	MMU_SETPMEG( ctx, addr, pmeg );
    } else {
	if ( sysmap == 0 || syssize == 0 )
	    panic( "early pagefault in kernel" );
	pid = 0;
	ctx = KERN_CTX;
	offset = BTOPG( RNDDN( addr - KERNBASE, MMU_SEGSIZE ));
	if ( offset >= syssize ) {
	    DPRINTF( 1, ( "pagefault: kernel reference off sysmap\n" ));
	    if ( probing ) {
		sparc_nofault = 0;
		return;
	    }
	    printf("Offset=%x, syssize=%x, addr=%x\n", offset, syssize, addr);
	    panic( "pagefault" );
	}
	ptep = &sysmap[ offset ];
	psp = 0;
	/* Allocate and fill in a PMEG */
	pmeg = mmu_allocpmeg( pid, addr, 0 );
	for ( ctx = machine.mi_contexts - 1 ; ctx >= 0; ctx-- )
	    MMU_SETPMEG( ctx, addr, pmeg );
	ctx = 0;
	DPRINTF( 1,
	  ( "pagefault: kernel:: addr=%x offset %x, ptep %x, *ptep=%x\n",
				addr, offset, ptep, *ptep ));
    }

    /* Round 'addr' to lower segment boundary */
    addr = RNDDN( addr, MMU_SEGSIZE );

    /* Fill in the PTEs from the software copy of the pagemap */
    for ( i = BTOPG( MMU_SEGSIZE ); i > 0; i-- ) {
	MMU_SETPTE( ctx, addr, *ptep );
	ptep += 1;
	addr += PAGESIZE;
    }
}


/*
 * The interface is like it is for compatibility with the sun4m cache
 * flush routines
 */
/*ARGSUSED*/
void
cache_flush(addr, size)
vir_bytes	addr;
vir_bytes	size;
{
    mmu_flushcache();
}


/*
 * hintmap() (ENTRY POINT) -- the upper layers are trying to give us a hint
 * about the total resources that a process will eat (as far as the upper
 * layer knows).  This is a stupid call that has no value, as the nbytes
 * parameter really tells us *nothing*.
 */
/*ARGSUSED*/
hintmap( pid, nbytes )
uint16 pid;
vir_bytes nbytes;
{
}

/*
 * setmap() (ENTRY POINT) -- add the given chunk of physical memory into the
 * memory map of the given process' address space at the virtual address
 * provided.
 * This routine is *only* for mapping in user process' memory.
 */
setmap( pid, vir, phys, len, type )
uint16 pid;
vir_clicks vir;
phys_clicks phys;
vir_clicks len;
long type;
{
    int ctx;
    int off;
    struct procseg *cur;
    struct procmap *pmp;
    vir_bytes bvir;	/* byte address */
    vir_bytes bphys;	/* byte address */
    vir_bytes topbits;

    assert(pid != 0);
    assert(pid < nproc);

    mmu_flushcache();

    pmp = &pmap[ pid ];
    ctx = pidtoctx[ pid ];
    bvir = vir << CLICKSHIFT;
    cur = &pmp->pm_seg[ SEGNUM( bvir ) ];
    off = (bvir & ( MMU_SEGSIZE - 1 )) >> PAGESHIFT;

    if (( type & MAP_PROTMASK ) == MAP_READWRITE )
	type = PTE_USRWRITE;
    else {
	if (( type & MAP_PROTMASK ) == MAP_READONLY )
	    type = PTE_USRREAD;
	else {
	    /* unmap it! */
	    type = 0;
	    phys = 0;
	}
    }

    bphys = phys << CLICKSHIFT;
    topbits = phys >> 12;
#ifndef NDEBUG
    if (type != 0)
    {
	/* We are mapping something in */
	if (topbits != 0xE) /* ie. kernel virtual address */
	{
	    DPRINTF( 2, ( "Got physical address 0x%x\n", bphys ));
	    assert(topbits == 1);
	    /* If not kernel virtual it must be I/O space */
	}
    }
#endif /* NDEBUG */

    DPRINTF(2,
     ("setmap: pid %x, vir %x(%08x), phys %x(%08x), len %x, pr %x, off = %x\n",
	   pid, vir, bvir, phys, bphys, len, type, off));

    /* Len becomes the total # pages still to be mapped */
    for ( len <<= (CLICKSHIFT-PAGESHIFT); len > 0; len-- ) {

	/* 1. Make sure we have a software copy segment map entry */
	if ( cur->ps_pte == 0 ) {
	    /*
	     * Allocate an smap for this segment.  We don't have to free
	     * the ones already allocated if we fail.  They'll go when the
	     * process exits.
	     */
	    DPRINTF( 2, ("setmap: allocing new smap\n" ));
	    if ( smap_free == -1 ) {
		DPRINTF( 0, ( "setmap: out of smaps\n" ));
		return -1;
	    }

	    DPRINTF( 0, ( "setmap: consuming smap %x\n", smap_free ));
	    cur->ps_pte = &segmap[ smap_free * NPTEperSEG ];
	    (void) memset((_VOIDSTAR) cur->ps_pte, 0,
					(size_t) NPTEperSEG * sizeof (int));
	    smap_free = smap_flist[ smap_free ];
	    cur->ps_pmeg = MMU_FODPMEG;
	}

	/* 2. Set the software copy of the segment map */
	if (topbits == 0xE) {
	    cur->ps_pte[ off ] = type | ( MMU_MAKEPTE( bphys ) & PTE_PFNUM );
	} else {
	    /* Hack: we have to map something really physical
	     * Note that when getting the page number we only take
	     * the bottom 16 bits instead of 20 since the top 4 bits are
	     * bogus in this case - namely the I/O space number.
	     */
	    cur->ps_pte[ off ] = type | PTE_SPACE(topbits) | PTE_NOCACHE |
					(BTOPG(bphys) & 0xFFFF);
	}

	/*
	 * 3. If there is no context but there is a pmeg we free it.
	 *    Else if the is a context but not pmeg we set the softcopy
	 *    and the pmeg to fill on demand.  If there is a context and
	 *    a pmeg we fill in the PTE.
	 *    NB.  The test PMEG_LOCKED is cheaper than testing if it is
	 *    the invalid or fill-on-demand pmeg.  No user process can
	 *    ever get some other LOCKED pmeg.
	 */
	if ( ctx == -1 ) {
	    DPRINTF( 2, ( "setmap: no ctx @ vaddr %x\n", bvir ));
	    if ( !PMEG_LOCKED( cur->ps_pmeg )) {
		DPRINTF( 2, ( "setmap: freeing pmeg %x\n", cur->ps_pmeg ));
		mmu_freepmeg( cur->ps_pmeg );
	    }
	    cur->ps_pmeg = MMU_FODPMEG;
	} else {
	    DPRINTF( 2, ( "setmap: ctx & pmeg %x @ vaddr %x\n",
							cur->ps_pmeg, bvir ));
	    if ( PMEG_LOCKED( cur->ps_pmeg )) {
		/* Just in case it is the invalid pmeg */
		MMU_SETPMEG( ctx, bvir, MMU_FODPMEG );
		cur->ps_pmeg = MMU_FODPMEG;
	    } else {
		MMU_SETPTE( ctx, bvir, cur->ps_pte[off] );
	    }
	}

	/* 4. The virtual and physical addresses have to be incremented */
	bvir += PAGESIZE;
	bphys += PAGESIZE;

	/*
	 * 5. If we are at the start of a new segment we have to wrap
	 *    the offset and increment cur.
	 */
	if ( ++off == NPTEperSEG ) {
	    cur++;
	    off = 0;
	}
    }

    return 0;
}


/*
 * clrmap() (ENTRY POINT) -- clear and free the resources allocated to the
 * given processID.
 */
clrmap( pid )
uint16 pid;
{
    int i, seg;
    struct procseg *psp;

    DPRINTF( 1, ( "clrmap: %x\n", pid ));

    if ( pid > nproc ) panic( "clrmap: bad pid" );

    for ( i = NUM_USERSEGS, psp = pmap[ pid ].pm_seg; i > 0; i--, psp++ ) {
	if ( psp->ps_pte == 0 ) continue;

	seg = ( psp->ps_pte - segmap ) / NPTEperSEG;
	DPRINTF( 1, ( "clrmap: freeing smap %x\n", seg ));
	smap_flist[ seg ] = smap_free;
	smap_free = seg;
	psp->ps_pte = 0;

	if ( ! PMEG_LOCKED(psp->ps_pmeg) ) {
	    DPRINTF( 1, ( "clrmap: freeing pmeg %x\n", psp->ps_pmeg ));
	    mmu_freepmeg( psp->ps_pmeg );
	}
	psp->ps_pmeg = MMU_INVPMEG;
    }

    if ( pidtoctx[ pid ] != -1 )
	mmu_freectx( pidtoctx[ pid ]);
}

vir_bytes kstk_init( kernel_end )
vir_bytes kernel_end;
{
    int kstk_size;				/* Kernel stack size */
    int i, stksize;

    kstk_size = KTHREAD_STKSIZ;

    /* Find kstk_shift */
    for ( i = 1, stksize = 2; i != 0; i += 1, stksize <<= 1 )
	if ( stksize >= kstk_size ) break;

    DPRINTF( 0, ( "minimum kernel stack is %d(0x%x)\n", stksize, 1 << i ));

    kstk_shift = i;

    kernel_alloc( kstk_flist, int, totthread );

    /*
     * We cannot just kernel_alloc() kstk_base, for it is too much memory
     * for kernel_alloc to deal with. We must update kernel_end, but we
     * just back this memory with real memory ourselves.
     */
    i = totthread << kstk_shift;
    kstk_base = (char *) kernel_end;
    kernel_end += i;

    DPRINTF( 2, ( "kstk_base %x, kernel_end %x\n", kstk_base, kernel_end ));

    for ( i = totthread - 2; i >= 0; i-- )
	kstk_flist[ i ] = i + 1;
    kstk_flist[ totthread - 1 ] = -1;
    kstk_fhead = 0;

    return( kernel_end );
}

/*ARGSUSED*/
vir_bytes kstk_alloc( size )
vir_bytes size;
{
    char *ksp;

    assert(size <= KTHREAD_STKSIZ);
    if ( kstk_fhead == -1 ) return( 0 );

    ksp = kstk_base + ( kstk_fhead << kstk_shift );
    DPRINTF( 1, ( "kstk_alloc: stack number %x(%x) size %d\n",
		kstk_fhead, ksp, size ));
    kstk_fhead = kstk_flist[ kstk_fhead ];

    return( (vir_bytes) ksp );
}

void kstk_free( addr )
vir_bytes addr;
{
    int ksn;

    ksn = ( (char *) addr - kstk_base ) >> kstk_shift;
    kstk_flist[ ksn ] = kstk_fhead;
    kstk_fhead = ksn;
    DPRINTF( 0, ( "kstk_free: stack number %x(%x)\n", ksn, addr ));
}

unsigned char *interrupt_reg_p;

init_ie_register() {
    vir_bytes vaddr;

    vaddr = mmu_virtaddr("/interrupt-enable");
    interrupt_reg_p = (unsigned char *) vaddr;
    *interrupt_reg_p = IER_ENALL;
}

/*
 * initmmearly() -- The trap handling code requires some setup work to be
 * done by the memory management code. Any such work is done here.
 */
initmmearly() {
    pidtoctx = &dummy_pidtoctxtopidmap;
    ctxtopid = &dummy_pidtoctxtopidmap;
}
	
/*
 * initmm() (ENTRY POINT) -- perform all manner of mmu initializations. This
 * includes setting up handlers for MMU traps, setting hooks that will allow
 * all physical memory to be viewed as one contiguous piece, init the PMEG
 * and hardware context recyclers, handing memory chunks to init{,more}mem(),
 * and tons of other stuff.
 */
void
initmm() {

    extern int etext;
    extern char end;

    struct phys_mem *physp;
    int i, j, total;
    vir_bytes firstwritepage, lastwritepage, va;
    int nph, type, pa, len, size, pmeg;
    int *smap, *smap_beg;
    vir_bytes kernel_end;
    struct procmap *prp;
    struct procseg *psp;
    struct pmegmap *freepmp;

    mmu_setctx( PROM_CTX );
    DPRINTF( 0, ( "initing memory management\n" ));

    /* Insert trap catching hooks here */
    setvec( TRAPV_INSN_ACCESS, mmu_accesstrap );
    setvec( TRAPV_DATA_ACCESS, mmu_accesstrap );
    setvec( INTERRUPT(15), mmu_nmitrap );

    /* Mark all pmegs as unused. */
    for ( i = machine.mi_pmegs - 1; i >= 0; i-- )
	pmegmap[ i ].pm_pid = -1;

    /*
     * We need to mark all the kernel PMEGs as LOCKED, so that the
     * recycling code does not reclaim them, as this could cause the PMEG
     * reclaiming code to lose it's PMEG.  LOCKED pmegs all belong to pid 0.
     */
    for ( i = MMU_KERNPMEG; i != MMU_KERNPMEG_LAST; i++ )
	LOCK_PMEG( i );
    LOCK_PMEG( MMU_PMEGMAP1 );
    LOCK_PMEG( MMU_PMEGMAP2 );
    LOCK_PMEG( MMU_FODPMEG );
    LOCK_PMEG( MMU_PROMPMEG );
    LOCK_PMEG( MMU_IOPMEG );

    /* Now we mark all the PMEGs the PROM uses as KERNEL owned */
    for ( i = PROMLOW; i != PROMHIGH; i += MMU_SEGSIZE )
	LOCK_PMEG( mmu_getpmeg( PROM_CTX, (vir_bytes) i ) );
    LOCK_PMEG( 0 );	/* Prom uses these two too */
    LOCK_PMEG( MMU_INVPMEG );

    freepmp = 0;
    /* Set up the LRS list of as yet unused PMEGS */
    for ( i = 0; i < machine.mi_pmegs; i++ ) {
	if ( ! PMEG_LOCKED( i ) ) {
	    if ( freepmp == 0 ) {
		pmeg_head = freepmp = &pmegmap[ i ];
	    } else {
		freepmp->pm_next = &pmegmap[ i ];
		freepmp = freepmp->pm_next;
	    }
	}
    }
    assert(freepmp != 0);
    freepmp->pm_next = 0;
    pmeg_tail = freepmp;

    pm_maxchunk = obp_availmem(PM_MAXCHUNKS, physmem);
    total = 0;
    for ( i = 0; i < pm_maxchunk; i++ ) {
        DPRINTF(1, ("Found mem-chunk %d at %x, len %x\n",
			i, physmem[i].phm_addr, physmem[i].phm_len));
	total += BTOPG( physmem[i].phm_len );
    }

    /* Now we can perform any kernel allocations my need */
    kernel_end = (vir_bytes) &end;

    syssize = RNDUP( total, BTOPG( MMU_SEGSIZE ));
    kernel_alloc( smap, int, syssize );
    smap_beg = smap;

    kernel_alloc( pmap, struct procmap, nproc );
    kernel_alloc( pidtoctx, int, nproc );
    kernel_alloc( ctxtopid, int, machine.mi_contexts );
    kernel_alloc( segmap, int, NPTEperSEG * NUMSEGS );
    kernel_alloc( smap_flist, int, NUMSEGS );

    /* Call kstk_init after all the dust has settled */
    kernel_end = kstk_init( kernel_end );

    DPRINTF( 2, ( "initmemory: sysmap %08x, syssize %08x, end %08x\n",
	   smap, syssize, kernel_end ));

    /*
     * Use the prom to set segments in other contexts.  You need this if you
     * want to do early printfs.  We have to do it again later to get the
     * subsequent changes we make.
     */
    size = MMU_SEGSIZE;
    for ( va = KERNBASE; va != 0; va += size ) {
	pmeg = mmu_getpmeg( PROM_CTX, va );
	for ( i = machine.mi_contexts - 1 ; i >= 0; i-- )
	    (*prom_devp->v2_setseg)( i, va, pmeg );
    }

    firstwritepage = RNDUP( (int) &etext, PAGESIZE );
    lastwritepage = KERNBASE +
		(( MMU_KERNPMEG_LAST - MMU_KERNPMEG ) << MMU_SEGSHIFT );
    if ( lastwritepage < kernel_end ) {
	mmu_populate( lastwritepage, (int) (kernel_end - lastwritepage) );
	lastwritepage = (vir_bytes) RNDUP( (int) kernel_end, MMU_SEGSIZE );
	DPRINTF( 2, ( "initmm: locked kernel gets expanded to %x\n",
		 lastwritepage ));
    }

    /* Now we must make PTEs for kernel memory, mapping the first few pmegs */
    va = KERNBASE;
    size = PAGESIZE;
    for ( physp = physmem, nph = pm_maxchunk; nph > 0; nph--, physp++ ) {

	for ( pa = BTOPG( physp->phm_addr ), len = BTOPG( physp->phm_len );
	     len > 0; len-- ) {

	    type = ( KERNLOAD <= va && va < firstwritepage )
		? PTE_KERNREAD : PTE_KERNWRITE;

	    *smap = pa | type;
	    if ( va < lastwritepage )
		MMU_SETPTE( PROM_CTX, va, pa | type );
	    else {
		if ( ( va & ( MMU_SEGSIZE - 1 )) == 0 )
		    for ( i = machine.mi_contexts - 1 ; i >= 0; i-- )
			MMU_SETPMEG( i, va, MMU_FODPMEG );
	    }

	    smap += 1;
	    pa += 1;
	    va += size;
	}
    }

    /* Set the PMEG to map IO registers */
    mmu_ioaddr = PROMHIGH;
    mmu_maxioaddr = PROMHIGH + MMU_SEGSIZE;
    MMU_SETPMEG( PROM_CTX, mmu_ioaddr, MMU_IOPMEG);

    /* Use the prom to set segments in other contexts */
    size = MMU_SEGSIZE;
    for ( va = KERNBASE; va != 0; va += size ) {
	pmeg = mmu_getpmeg( PROM_CTX, va );
	for ( i = machine.mi_contexts - 1 ; i >= 0; i-- )
	    (*prom_devp->v2_setseg)( i, va, pmeg );
    }

    /* Now zero out all non-kernel memory */
    for ( va = USERLOW/*0*/; va < USERHIGH; va += size ) {
	for ( i = machine.mi_contexts - 1 ; i >= 0; i-- )
	    mmu_setpmeg( i, va, MMU_INVPMEG );
    }

#ifndef NDEBUG
    /* Test all that out */
    for ( va = KERNBASE; va != 0; va += size )
	for ( i = machine.mi_contexts - 1; i >= 0; i-- )
	    if ( mmu_getpmeg( PROM_CTX, va ) != mmu_getpmeg( i, va )) {
		DPRINTF( 1, ( "initmm: kmegs differ at va %08x (%x != %x)\n",
		       va, mmu_getpmeg( PROM_CTX, va ), mmu_getpmeg( i, va )));
		panic( "initmm" );
	    }

    for ( va = USERLOW/*0*/; va < USERHIGH; va += size )
	for ( i = machine.mi_contexts - 1; i >= 0; i-- )
	    if ( mmu_getpmeg( PROM_CTX, va ) != mmu_getpmeg( i, va )) {
		DPRINTF( 0, ( "initmm: kmegs differ at va %08x (%x != %x)\n",
		       va, mmu_getpmeg( PROM_CTX, va ), mmu_getpmeg( i, va )));
		panic( "initmm" );
	    }
#endif	/* !NDEBUG */

    (void) memset((_VOIDSTAR) kstk_base, 0, (size_t) (totthread << kstk_shift));

    /* Now zero out the invalid PMEG (make !PTE_V pages) */
    MMU_SETPMEG( PROM_CTX, (vir_bytes) USERLOW, MMU_INVPMEG );
    for ( va = 0; va < size; va += PAGESIZE )
	MMU_SETPTE( PROM_CTX, va + USERLOW, 0 );

    /* Now zero out the fill-on-demand PMEG (make !PTE_V pages) */
    MMU_SETPMEG( PROM_CTX, (vir_bytes) USERLOW, MMU_FODPMEG );
    for ( va = 0; va < size; va += PAGESIZE )
	MMU_SETPTE( PROM_CTX, va + USERLOW, 0 );

    /* Set it back to INValid */
    MMU_SETPMEG( PROM_CTX, (vir_bytes) USERLOW, MMU_INVPMEG );

    /* Set up segmap free list */
    for ( i = NUMSEGS - 2; i >= 0; i-- )
	smap_flist[ i ] = i + 1;
    smap_flist[ NUMSEGS - 1 ] = -1;
    smap_free = 0;

    /* Set the procmaps to be invalid pmegs */
    for ( i = nproc, prp = pmap; i > 0; i--, prp++ )
	for ( j = NUM_USERSEGS, psp = prp->pm_seg; j > 0; j--, psp++ )
	    psp->ps_pmeg = MMU_INVPMEG;

    /*
     * NB. pidtoctx[0] == 0 and mustn't be changed.
     */
    pidtoctx[ 0 ] = 0;
    for ( i = 1; i < (int) nproc; i++ )
	pidtoctx[ i ] = -1;
    for ( i = 0; i < machine.mi_contexts; i++ )
	ctxtopid[ i ] = -1;

    init_ie_register();
    init_me_register();

    /* Now that things are all initialized, we can set (and test) "sysmap" */
    sysmap = smap_beg;
#ifndef NDEBUG
    if ( sparc_teststack( 0, 1, 2, 3, 4, 5 )) panic( "teststack3" );
#endif	/* !NDEBUG */

    /* Now we can initialize the cache */
    mmu_initcache();

    lructx = machine.mi_contexts - 1;
    mmu_setctx( KERN_CTX );

    /* Now we go tell the upper layers about our memory */
    mmu_maxaddr = KERNBASE + PGTOB( total );
    seg_reg_memchunk(BATOC(VTOP(KERNELSTART)),
				(phys_clicks) total>>(CLICKSHIFT-PAGESHIFT) );
    seg_mark_used( BATOC(VTOP(KERNELSTART)),
		BSTOC(VTOP(kernel_end - KERNELSTART)), (long) ST_KERNEL );
    mmu_set_sysenb_reg(ENA_SDVMA | ENA_NOTBOOT);
}

#ifndef SMALL_KERNEL
/*
 * mmu_dump() (CONFIG ENTRY POINT) -- called from the dumptab to dump any 
 * MMU information that we want for someone else to see.
 */
mmu_dump( begin, end )
char *begin;
char *end;
{
    int i, j;
    register char *p;
    struct pmegmap *pmp;

    p = bprintf( begin, end, "\nctxtopid::" );
    for ( i = 0, j = machine.mi_contexts; j > 0; j--, i++ )
	p = bprintf( p, end, "\n[%x] %x  ", i, ctxtopid[ i ]);

    p = bprintf( p, end, "\npidtoctx::" );
    for ( i = 0, j = nproc; j > 0; j--, i++ )
	p = bprintf( p, end, "\n[%x] %x  ", i, pidtoctx[ i ]);

    p = bprintf( p, end, "\nlructx is %x", lructx );

    p = bprintf( p, end, "\npmegmap:: (h=%x, t=%x)", pmeg_head, pmeg_tail );
    p = bprintf( p, end, "\n  [pmeg/ptr]   pid  address     next");
    for ( i = 0, j = machine.mi_pmegs, pmp = pmegmap; j > 0; j--, i++, pmp++ )
	p = bprintf( p, end, "\n[%2x/%8x] %4d %8x %8x",
			i, pmp, pmp->pm_pid, pmp->pm_address, pmp->pm_next );

    p = bprintf( p, end, "\nphysmem::" );
    for ( i = 0, j = pm_maxchunk; j > 0; j--, i++ )
	p = bprintf( p, end, "[%x] addr %x, len %x  ",
			i, physmem[ i ].phm_addr, physmem[ i ].phm_len );

    p = bprintf( p, end, "\n" );

    return( p - begin );
}
#endif /* SMALL_KERNEL */

/*
 * startkernel() (ENTRY POINT) -- start running the ``new'' kernel already
 * loaded into the virtual address given. We just make a virtual address out
 * of it, cast it into a void function, and call it.
 */
startkernel( addr )
phys_bytes addr;
{
    void (*function)();

    function = (void (*) ()) PTOV( addr );
    mmu_offcache();
    disable();
    (*function)( prom_devp );
}

#ifndef NOPROC

extern void fast_flush_windows();

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

    fast_flush_windows();

    (void) memset( (_VOIDSTAR) &tu, 0, sizeof( tu ));
    tu.tu_psr = USER_PSR;
    tu.tu_pc = pc;
    tu.tu_npc = MAKE_NPC( pc );
    tu.tu_pid = pid;
    tu.tu_fp = ( sp == 0 ) ? -1 : RNDDN( (int) sp - MINFRAME, MAXALIGN );
    tu.tu_sf.sf_in[ 0 ] = sp;
    tu.tu_retaddr = -1;

    start( &tu );
    panic( "startuser: start returned" );
}
#endif

/*
 * reboot() (ENTRY POINT) -- Reboot the machine, using the monitor
 */
/*ARGSUSED*/
reboot( begin, end )
char *begin;
char *end;
{
    char bootargs[256];

    bprintf( begin, end, "rebooting....\n" );
#ifdef NO_KERNEL_SECURITY
    if (prom_devp->op_romvec_version <= 1) {
	(*prom_devp->op_boot)( *prom_devp->v1_bootcmd );
    } else {
	(void) strcpy( bootargs, *prom_devp->op_bootpath );
	(void) strcat( bootargs, *prom_devp->op_bootargs );
	(*prom_devp->op_boot)( bootargs );
    }
#else
    (*prom_devp->op_boot)( "" );
#endif /* NO_KERNEL_SECURITY */
    /*NOTREACHED*/
}

/*
 * The following routines are not machine dependant, but we must still
 * provide them, just in case we wanted to do something special. These
 * are provided as macros in map.h, for callers which want to save the
 * overhead of a procedure call.
 */

#undef phys_copy
#undef phys_probe
#undef phys_zero

/*
 * phys_copy() (ENTRY POINT) -- copy the given area of physical memory
 */
phys_copy( src, dst, cnt )
phys_bytes src, dst, cnt;
{
    (void) memmove( (_VOIDSTAR) PTOV( dst ), (_VOIDSTAR) PTOV( src ),
			(size_t) cnt );
}

/*
 * phys_probe() (ENTRY POINT) -- probe the given byte of physical memory
 */
phys_probe( p )
phys_bytes p;
{
    return probe( PTOV( p ), 1 );
}

/*
 * phys_zero() (ENTRY POINT) -- clear the given chunk of physical memory
 */
phys_zero( dst, cnt )
phys_bytes dst, cnt;
{
    (void) memset( (_VOIDSTAR) PTOV( dst ), 0, (size_t) cnt );
}


/*
 * A kernel has been loaded somewhere into memory.  We want to start it
 * running to replace us.
 * XXX: This routine may have to first disable the MMU before it all works
 */

#include "memaccess.h"

/*ARGSUSED*/
void
bootkernel(entry, size, commandline, flags)
phys_bytes entry;
long size;
char *commandline;
int flags;
{
    static int (*start)(); /* static to avoid GAS bug */
    char *	addr;

    start = (int (*)()) (entry+32);
    printf("Starting new kernel at 0x%x (really 0x%x) size 0x%x\n",
	   entry, (int) start, size);
    mmu_offcache();

    /* Touch all the segments of the new kernel to make sure they have pmegs */
    addr = (char *) entry;
    (void) mem_get_byte(addr + size); /* Touch the last segment */
    size = ( size + MMU_SEGSIZE - 1 ) >> MMU_SEGSHIFT;
    while (--size >= 0) {
	(void) mem_get_byte(addr);
	addr += MMU_SEGSIZE;
    }

    /* Dive into the new kernel & pray */
    (*start)(prom_devp);
    panic("bootkernel: new kernel returned");
}
