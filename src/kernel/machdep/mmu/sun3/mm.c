/*	@(#)mm.c	1.9	96/02/27 13:59:28 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#include "process/proc.h"
#include "boot.h"
#include "assert.h"
INIT_ASSERT
#include "cputype.h"
#include "sys/seg.h"
#include "sys/proto.h"

#define MEMCTL		ABSPTR(char *, IOBASE + 0x8000)
#define MEMADR		ABSPTR(long *, IOBASE + 0x8004)
#define INTREG		ABSPTR(char *, IOBASE + 0xA000)

#define NCONTEXT	8

#define PAGEBITS	4
#define SEGBITS		11

#define NPAGE		((vir_clicks) 1 << PAGEBITS)
#define NSEGMENT	(1 << SEGBITS)

#define PAGESIZE	(1 << CLICKSHIFT)
#define OFFSET		((phys_bytes) PAGESIZE - 1)

#define SEGSHIFT	(CLICKSHIFT + PAGEBITS)
#define SEGSIZE		((long) 1 << SEGSHIFT)

/*
** the following gives the maximum legal value for a phys_click plus
** a mask for dealing with the peculiar round off errors due to the
** bitmap being mapped in lower than KERNELSTART
*/

#define	MAX_PHYS_CLICK	((sizeof(phys_clicks) * 8) - CLICKSHIFT)
#define	PHYS_CLK_MASK	((1 << MAX_PHYS_CLICK) -1)

#define MM(base, seg, page, offset)	(base \
					| ((long) (seg) << SEGSHIFT	\
					| (long) (page) << CLICKSHIFT	\
					| (offset) & ~OFFSET))

#define PAGEMAP	((long) 0x10000000)
#define SEGMAP	((long) 0x20000000)
#define CONTEXT	((long) 0x30000000)

#define SM(seg)		MM(SEGMAP, seg, 0, 0)
#define PM(seg, page)	MM(PAGEMAP, seg, page, 0)

#define		bit(b)		(1 << (b))

#define VB	bit(31)		/* valid bit, implies read access */
#define WB	bit(30)		/* write access bit */
#define SB	bit(29)		/* system access bit */
#define XB	bit(28)		/* don't cache bit */
#define AB	bit(25)		/* accessed bit */
#define MB	bit(24)		/* modified bit */

#define MEM	0x0000000	/* main memory */
#define IO	0x4000000	/* SUN-3 I/O space */
#define V16	0x8000000	/* VMEbus 16-bit data */
#define V32	0xC000000	/* VMEbus 32-bit data */

/* pmegs 127, and 239-255, are used by the monitor, and should be left
 * untouched.
 */
#define IOPMEG		125	/* pmeg of io pages */
#define BMPMEG		126	/* Bitmap pages */
#define INVALID		127	/* invalid pages (courtesy of monitor) */
#define FIRSTUPMEG	128	/* first free user pmeg */
#define NPMEG		239	/* #pmegs */

#define ACCESS		0xE0000000
#define ______		XB
#define R_X___		(XB | VB | SB)
#define RW____		(XB | VB | WB | SB)
#define RWX___		(XB | VB | WB | SB)
#define RW_RW_		(XB | VB | WB)
#define RW_R_X		(XB | VB)
#define RW_RWX		(XB | VB | WB)

#define OB_LAST		0x1000000
#define OB_INCR		SEGSIZE

#define FIRSTUSEG	(VIR_LOW >> PAGEBITS)
#define LASTUSEG	(VIR_HIGH >> PAGEBITS)

#define IOSEG		(IOBASE >> SEGSHIFT)
#define BMSEG		(BMBASE >> SEGSHIFT)

#define IOSPACE		(VB|WB|SB|XB|IO)

static phys_clicks obclicks;

int cputype;

/*ARGSUSED*/
void
bootkernel(entry, size, commandline, flags)
phys_bytes entry;
long size;
char *commandline;
int flags;
{
#if 0
	/* What needs to happen is that we change the mapping so that
	 * kernel virtual is equivalent to real physical addresses
	 * and then jump into the code. The trick is to make all this
	 * position independent.
	 */
	printf("Starting kernel 0x%x...", entry);
	(*(int (*)()) entry)();
	panic("bootkernel: new kernel returned!");
#else
	panic("bootkernel: not yet implemented");
#endif
}

#ifndef NOPROC

#define NILALLOC	0
#define ALLOC		((uint16) -1)

struct pmeg {
	short p_alloc;		/* bitmap of allocated pages */
	short p_seg;		/* segment number in context */
	struct pmeg *p_next;	/* for linked lists */
	struct pmeg **p_prev;	/* for doubly linked lists */
} pmeg[NPMEG], *pfree, **procmap;

extern uint16 nproc;


usemap(ctx)
uint16 ctx;
{

  compare(ctx, >, 0);
  tctlb(CONTEXT, ctx-1);
  tcacr(9);
}

startuser(ctx, pc, sp)
uint16 ctx;
vir_bytes pc;
vir_bytes sp;
{

  usemap(ctx);
  start(pc, sp);
}


/*ARGSUSED*/
hintmap(ctx, nbytes)
uint16		ctx;
vir_bytes	nbytes;
{
    extern uint16 nproc;

    /* check configuration. Done here because by now we are allowed
     * to print panic messages.
     */
    compare(nproc-1, <=, NCONTEXT);
}

/*
 * Set up the necessary mapping for a user segment.
 */
int
setmap(ctx, vir, phys, len, type)
uint16 ctx;
vir_clicks vir;
register phys_clicks phys;
vir_clicks len;
long type;
{
  int i, page, seg, pm;
  struct pmeg *p;
  int oldcontext;

  compare(ctx, >, 0);
  oldcontext = fctlb(CONTEXT);
  tctlb(CONTEXT, ctx-1);
  compare(len, !=, 0);
  phys -= KERNELSTART >> CLICKSHIFT;
  /* we mask off the bits that can't be a valid click address */
  phys &= PHYS_CLK_MASK;
  type &= MAP_PROTMASK;
  for (i = 0; i < len; i++, vir++, phys++) {
	/* See whether segment including this page has already a pmeg
	 * allocated.  INVALID is a pmeg with all pages invalid and is
	 * used to denote that no pmeg has been allocated.  If no pmeg
	 * has been allocated, allocate one (unless the map is invalidated).
	 */
	seg = vir >> PAGEBITS;
	if ((pm = fctlb(SM(seg))) == INVALID) {
		if (type == 0)
			continue;
		if ((p = pfree) == 0)
			panic("out of pmegs");
		pfree = p->p_next;
		tctlb(SM(seg), (unsigned char) (p - pmeg));
		p->p_prev = &procmap[ctx];
		if ((p->p_next = *p->p_prev) != 0)
			p->p_next->p_prev = &p->p_next;
		*p->p_prev = p;
		p->p_seg = seg;
		assert(p->p_alloc == 0);
	}
	else {
		p = &pmeg[pm];
		assert(p->p_seg == seg);
		assert(p->p_alloc != 0);
	}

	/* Depending on the type a page has to be allocated or freed.
	 */
	page = vir & (NPAGE - 1);
	switch (type) {
        case MAP_READWRITE:
		tctll(PM(seg, page), phys | RW_RWX);
		p->p_alloc |= 1 << page;
		break;
        case MAP_READONLY:
		tctll(PM(seg, page), phys | RW_R_X);
		p->p_alloc |= 1 << page;
		break;
	default:	/* INVALID */
		tctll(PM(seg, page), (long) ______);
		p->p_alloc &= ~(1 << page);
		/* If all pages have been freed in this pmeg, the pmeg can
		 * be freed.
		 */
		if (p->p_alloc == 0) {
			if ((*p->p_prev = p->p_next) != 0)
				p->p_next->p_prev = p->p_prev;
			p->p_next = pfree;
			pfree = p;
			tctlb(SM(seg), INVALID);
		}
        }
  }
  tctlb(CONTEXT, oldcontext);
  return 0;
}

clrmap(ctx)
register uint16 ctx;
{
  struct pmeg *p, *next;
  int page;
  int oldcontext;

  compare(ctx, >, 0);
  oldcontext = fctlb(CONTEXT);
  tctlb(CONTEXT, ctx-1);
  for (p = procmap[ctx]; p != 0; p = next) {
	assert(p->p_alloc != 0);
	for (page = 0; page < NPAGE; page++)
		if (p->p_alloc & (1 << page))
			tctlb(PM(p->p_seg, page), ______);
	p->p_alloc = 0;
	tctlb(SM(p->p_seg), INVALID);
	next = p->p_next;
	p->p_next = pfree;
	pfree = p;
  }
  procmap[ctx] = 0;
  tctlb(CONTEXT, oldcontext);
}

/*ARGSUSED*/
mmrerun(addr, fc)
char *addr;
{

	/*
	 * We never restart instructions
	 */
	return 0;
}

#endif NOPROC


/*
 * Copy 'cnt' bytes from 'src' to 'dst.'
 */
phys_copy(src, dst, cnt)
phys_bytes src, dst, cnt;
{
  (void) memmove((_VOIDSTAR) PTOV(dst), (_VOIDSTAR) PTOV(src), (size_t) cnt);
}

/*
 * Zero 'cnt' bytes at 'dst.'
 */
phys_zero(dst, cnt)
phys_bytes dst, cnt;
{
  (void) memset((_VOIDSTAR) PTOV(dst), '\0', (size_t) cnt);
}

/* see if physical address ``addr'' exists */
phys_probe(addr)
phys_bytes addr;
{
  return probe(PTOV(addr), 4);
}

/* count onboard memory */
static countob(){
  register phys_bytes addr;

  for (addr = IVECBASE; addr < IVECBASE+OB_LAST; addr += OB_INCR)
	if (!phys_probe(addr))	/* test for bus error */
		break;
  /* subtract one segment and leave it for the monitor */
  obclicks = (addr - OB_INCR) >> CLICKSHIFT;
}

#ifdef notdef
/*ARGSUSED*/
mapkernel(mm)
struct memmap *mm;
{
  panic("mapkernel called");
}
#endif

void
nmi_trap(){
  printf("NMI trap: memory control register = %x\n", *MEMCTL & 0xFF);
  printf("   virtual address = %lx\n", *MEMADR & 0xFFFFFFF);
  printf("   context         = %ld\n", (*MEMADR >> 28) & 7);
  printf("   DVMA cycle err  = %ld\n", (*MEMADR >> 31) & 1);
  *MEMADR = 0;		/* clear pending interrupt */
}

void
initerror(){
  (void) setvec(31, nmi_trap);
  *MEMCTL |= 0x41;	/* enable error control */
  *MEMADR = 0;		/* clear pending interrupt */
  *INTREG |= 1;		/* enable all interrupts */
}

/*
** The physical address of the bitmap is needed for mapping it in as a
** hardware segment by iop drivers & other utilities that want the screen
*/
phys_bytes bmaddr;

void
mm_initseg()
{
  extern char end;
  phys_clicks begin_click, end_click;
  register uint16 ctx, seg, page, pm;

  cputype = fctlb((long) 1);
  for (ctx = 0; ctx < NCONTEXT; ctx++) {		/* user contexts */
	/* in each context, set up the segment registers for the
	 * user segments, the IO segment, and the bitmap segment.
	 * The page registers are shared by all contexts, and are
	 * filled in later.
	 */
	tctlb(CONTEXT, ctx);
	tctlb(SM(IOSEG), IOPMEG);
	tctlb(SM(BMSEG), BMPMEG);
	for (seg = FIRSTUSEG; seg < LASTUSEG; seg++)
		tctlb(SM(seg), INVALID);
  }
  for (page = 0; page < NPAGE; page++)		/* invalid pages */
	tctll(PM(INVALID, page), (long) ______);
  for (page = 0; page < NPAGE; page++)		/* IO pages */
	tctll(PM(IOSEG, page), (long) page * 0x10 + IOSPACE);
  switch(cputype) {
  case SUN3_50:
  default:
	bmaddr = 0x100000;
	break;
  case SUN3_60:
	bmaddr = 0xFF000000;
	break;
  }
  for (page = 0; page < NPAGE; page++)		/* Bitmap pages */
	tctll(PM(BMSEG, page), (long) ((((bmaddr>>CLICKSHIFT)&0x3FFFF) + page)|RWX___));
  /* Initialize all pages in all user pmegs to be non-accessible.  setmap
   * relies on this.  FIRSTUSEG is used as a scratch pmeg, but has to be
   * set back to INVALID afterwards.
   */
  for (pm = FIRSTUPMEG; pm < NPMEG; pm++) {
	tctlb(SM(FIRSTUSEG), pm);
	for (page = 0; page < NPAGE; page++)
		tctll(PM(FIRSTUSEG, page), (long) ______);
  }
  tctlb(SM(FIRSTUSEG), INVALID);
  countob();

  begin_click= BATOC(KERNELSTART);
  end_click= BSTOC((long)&end);

  seg_reg_memchunk(begin_click, obclicks - begin_click);	/* all memory */
  seg_mark_used(begin_click, end_click - begin_click, (long) ST_KERNEL);
  /*
   * We have to mark the framebuffer of the 3/50 because it is in the
   * middle of the kernel address space and we don't want it being allocated
   * as ordinary memory
   */
  if (cputype == SUN3_50)
	seg_mark_used(BATOC(KERNELSTART + 0x100000), BSTOC(0x20000),
				(long) ST_HARDWARE);
}

void
initmm()
{
#ifndef NOPROC
  struct pmeg *p;

  for (p = &pmeg[FIRSTUPMEG]; p < &pmeg[NPMEG]; p++)
	if (p != &pmeg[INVALID]) {
		p->p_next = pfree;
		pfree = p;
	}
  procmap = (struct pmeg **) aalloc((vir_bytes) nproc * sizeof(*procmap), 0);
#endif
}
