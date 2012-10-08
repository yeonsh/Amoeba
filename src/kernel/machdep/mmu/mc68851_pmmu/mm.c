/*	@(#)mm.c	1.9	96/02/27 13:59:08 */
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
#include "assert.h"
INIT_ASSERT
#include "process/proc.h"
#include "sys/seg.h"
#include "sys/proto.h"

extern int is_mc68030;

#ifndef NOPROC
extern uint16 nproc;
#else
#define nproc 0			/* only used to size tables */
#endif /* NOPROC */

/*
 * convert local address to address as seen from the A24/D16 VMEbus
 * TODO: should definitely be removed from here.
 */
phys_bytes vme24addr(p)
phys_bytes p;
{
	/*
	 * What about mapped memory?
	 * Probably better to look in pagetable.
	 */

	if (p>=OBMEMBASE)
		return p-OBMEMBASE;
	if (p>=VME24BASE && p<VME24BASE+VME24SIZE)
		return p-VME24BASE;
	return 0x1000000;
	/*NOTREACHED*/
}

phys_copy(src, dst, cnt)
phys_bytes src, dst, cnt;
{
	(void) memmove((_VOIDSTAR) dst, (_VOIDSTAR) src, (size_t) cnt);
}

phys_probe(p)
phys_bytes p;
{
	return probe(p, 1);
}

phys_zero(dst, cnt)
phys_bytes dst, cnt;
{
	(void) memset((_VOIDSTAR) dst, 0, (size_t) cnt);
}

static phys_clicks
countmem(base, size)
phys_bytes base,size;
{
	long zerosave;
	phys_bytes addr;
	phys_clicks result;

	if (probe(base, 1) == 0)
		return 0;
	zerosave = *ABSPTR(long *, base);
	result = 1;
	for (addr=base+CLICKSIZE,size-=CLICKSIZE; size>0 && probe(addr, 1);
				addr += CLICKSIZE, size -= CLICKSIZE) {
		if (*ABSPTR(long *, addr) == *ABSPTR(long *, base)) {
			/*
			 * Memory happens to contain the same value as base
			 * Given the usual savings made in address decoding
			 * logic we are suspicious.
			 * So we increment base and if they then still
			 * match we have a wrap around and stop
			 */
			*ABSPTR(long *, base) += 1;
			if (*ABSPTR(long *, addr) == *ABSPTR(long *, base))
				break;
		}
		result++;
	}
	*ABSPTR(long *, base) = zerosave;
	return result;
}

#include "mc68851.h"

#define IS	0

struct user_mem_chunks {
	phys_bytes	umc_base;
	phys_bytes	umc_size;
	int		umc_must_be_there;
};

struct kernel_mem_chunks {
	unsigned long 	kmc_base;
	unsigned long	kmc_size;
	long		kmc_flags;
};

#include "addresses.c"

#define TID	0	/* Not used */

#define LOWBITS(n,b)	((n)&((1<<(b))-1))

#define N_A_DESCR	(1<<TIA)
#define N_B_DESCR	(1<<TIB)
#define N_C_DESCR	(1<<TIC)
#define N_BC_DESCR	(1<<(TIB+TIC))

#define ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

static phys_clicks user_mem_clicks[ARRAY_SIZE(user_mem_chunks)];

static struct ctxinfo {
	struct pm_rpr	ctx_rpr;
	char		ctx_inuse;
	char		ctx_fill[7];		/* alignment */
	struct pm_lftd	ctx_lA[N_A_DESCR];
} *rtctx
#ifndef NOPROC
	,*ctxinfo
#endif
		;
/*
 * Second level page table, allocated as necessary
 */
typedef union level_b_pagetable lbp_t, *lbp_p;
union level_b_pagetable {
	lbp_p		lbp_next;	/* pointer when in free list */
	struct pm_lftd	lbp_lB[N_B_DESCR];	/* level C page table */
};

static lbp_p	lbp_freelist;		/* Free list of these tables */
#define N_LBP	(nproc*3/2)+8		/* They usually only need one */

static int lbp_free,lbp_minfree;

/*
 * Lowest level page table, allocated as necessary.
 */
typedef union level_c_pagetable lcp_t, *lcp_p;
union level_c_pagetable {
	lcp_p	lcp_next;		/* pointer when in free list */
	long	lcp_page[N_C_DESCR];	/* Actual page descriptors when not */
};

static lcp_p	lcp_freelist;		/* Free list of these tables */
#define N_LCP	(nproc*5/2)+32		/* They usually only need two */

static int lcp_free,lcp_minfree;

static long *pagemap;
static phys_clicks npages;

static
hunt_for_mem()
{
	register i;
	phys_clicks chunk_size;

	/* next assertion should be lifted */
	compare(user_mem_chunks[0].umc_base, ==, OBMEMBASE);
	for(i=0;i<ARRAY_SIZE(user_mem_chunks);i++) {
		chunk_size = countmem(user_mem_chunks[i].umc_base,
					user_mem_chunks[i].umc_size);
		user_mem_clicks[i] = chunk_size;
		npages += chunk_size;
		if (user_mem_chunks[i].umc_must_be_there && 
			(chunk_size<<CLICKSHIFT) != user_mem_chunks[i].umc_size)
			break;
	}
}

static
init_page_map()
{
	register i,j;
	register long *p;
	register phys_bytes addr;

	p = pagemap;
	for(i=0;i<ARRAY_SIZE(user_mem_chunks);i++) {
		addr = user_mem_chunks[i].umc_base;
		for(j=0;j<user_mem_clicks[i];j++) {
			*p++ = addr|FL_DT_PAGE;
			addr += CLICKSIZE;
		}
	}
}
		
static
fill_map(c, vir, phy, size, flags)
register struct ctxinfo *c;
vir_clicks vir;
phys_clicks phy, size;
long flags;
{
    int aindex,bindex,cindex;
    register struct pm_lftd *a_tabent,*b_tabent;
    register lbp_p	b_table;
    register lcp_p	c_table;
    register i;
    int	     usepagemap;
    phys_bytes addr;

    if ((phy>=(OBMEMBASE>>CLICKSHIFT)) &&
				(phy < (OBMEMBASE>>CLICKSHIFT)+npages)) {
	usepagemap = 1;
	flags &= ~FL_DT_MASK;
    } else
	usepagemap = 0;
    while(size>0) {
	/*
	 * Compute all the indices
	 */
	cindex = LOWBITS(vir, TIC);	/* Index in level C table */
	bindex = vir>>TIC;
	aindex = bindex>>TIB;		/* Index in level A table */
	bindex = LOWBITS(bindex, TIB);	/* Index in level B table */

	compare(aindex, < , N_A_DESCR);
	compare(bindex, < , N_B_DESCR);
	compare(cindex, < , N_C_DESCR);

/*
printf("A=%x,B=%x,C=%x,%sP=%x,S=%x\n", aindex, bindex, cindex, usepagemap ? "U," : "", phy, size);
*/
	/*
	 * Walk down the tree.
	 * The regularity of this code immediately suggests a variable
	 * number of levels. Some day.
	 */
	a_tabent = &c->ctx_lA[aindex];
	if ((a_tabent->lftd_flags&FL_DT_MASK)==FL_DT_INVALID) {
		if (!usepagemap && bindex==0 && cindex==0 && size >= N_BC_DESCR) {
			/* Quick workaround is possible */
			a_tabent->lftd_flags = flags;
			a_tabent->lftd_limit = LIM_UNLIM;
			a_tabent->lftd_taddr = phy<<CLICKSHIFT;
			size -= N_BC_DESCR;
			vir += N_BC_DESCR;
			phy += N_BC_DESCR;
			continue;
		}
		/* Need a new level B page table */
		b_table = lbp_freelist;
		if (b_table == 0)
			return -1;	/* out of tables, shouldn't happen */
		lbp_freelist = b_table->lbp_next;
		lbp_free--;
		if (lbp_free<lbp_minfree)
			lbp_minfree = lbp_free;
		for (i=0; i<N_B_DESCR; i++)
			b_table->lbp_lB[i].lftd_flags = FL_DT_INVALID;
		a_tabent->lftd_flags = FL_DT_V8BYTE;
		a_tabent->lftd_limit = LIM_UNLIM;
		a_tabent->lftd_taddr = (long) b_table;
		assert((long) b_table == (long) b_table->lbp_lB);
	} else {
		compare(a_tabent->lftd_flags&FL_DT_MASK, ==, FL_DT_V8BYTE);
		b_table = (lbp_p) a_tabent->lftd_taddr;
	}

	b_tabent = &b_table->lbp_lB[bindex];
	if ((b_tabent->lftd_flags&FL_DT_MASK)==FL_DT_INVALID) {
		if (!usepagemap && cindex==0 && size >= N_C_DESCR) {
			/* Quick workaround is possible */
			b_tabent->lftd_flags = flags;
			b_tabent->lftd_limit = LIM_UNLIM;
			b_tabent->lftd_taddr = phy<<CLICKSHIFT;
			size -= N_C_DESCR;
			vir += N_C_DESCR;
			phy += N_C_DESCR;
			continue;
		}
		/* Need a new level C page table */
		c_table = lcp_freelist;
		if (c_table == 0)
			return -1;	/* out of tables, shouldn't happen */
		lcp_freelist = c_table->lcp_next;
		lcp_free--;
		if (lcp_free<lcp_minfree)
			lcp_minfree = lcp_free;
		for (i=0; i<N_C_DESCR; i++)
			c_table->lcp_page[i] = FL_DT_INVALID;
		b_tabent->lftd_flags = FL_DT_V4BYTE;
		b_tabent->lftd_limit = LIM_UNLIM;
		b_tabent->lftd_taddr = (long) c_table;
		assert((long) c_table == (long) c_table->lcp_page);
	} else {
		compare(b_tabent->lftd_flags&FL_DT_MASK, ==, FL_DT_V4BYTE);
		c_table = (lcp_p) b_tabent->lftd_taddr;
	}

	if (usepagemap)
		addr = pagemap[phy-(OBMEMBASE>>CLICKSHIFT)];
	else
		addr = (phy<<CLICKSHIFT);
	c_table->lcp_page[cindex] = addr|flags;
	size--;
	vir++;
	phy++;
    }
    return 0;
}

#ifndef NOPROC

long	cache_reset;	/* value to clear caches at context switch */

#define	CACR_WA		0x00002000
#define	CACR_DBE	0x00001000
#define	CACR_CD		0x00000800
#define	CACR_CED	0x00000400
#define	CACR_FD		0x00000200
#define	CACR_ED		0x00000100
#define	CACR_IBE	0x00000010
#define	CACR_CI		0x00000008
#define	CACR_CEI	0x00000004
#define	CACR_FI		0x00000002
#define	CACR_EI		0x00000001

static
initcache() {

  cache_reset = CACR_EI|CACR_CI;
  if (is_mc68030) {
	/*
	 * The data caching on the MC68030 has the option to
	 * allocate cache lines on write. We enable that option
	 * so we don't have to flush the cache when switching
	 * between user and kernel mode. This works because
	 * the aliased addresses in user/kernel mode are the
	 * same modulo the cache size(256 bytes).
	 *
	 * Finding this out took some man-days work and almost
	 * our sanity.
	 */
	cache_reset |= CACR_ED|CACR_CD|CACR_WA;
#ifdef BURST_ALLOWED
	cache_reset |= CACR_IBE|CACR_DBE;
#endif
  }
  tcacr(cache_reset);
}

usemap(ctx)
uint16 ctx;
{

  PsetCrp(&ctxinfo[ctx].ctx_rpr);
  tcacr(cache_reset);			/* clear caches */
}

startuser(ctx, pc, sp)
uint16 ctx;
vir_bytes pc, sp;
{

  usemap(ctx);
  start(pc, sp);
}

/*ARGSUSED*/
hintmap(ctx, nbytes)
uint16 ctx;
vir_bytes nbytes;
{

	/* not needed, we can handle anything without advance preparations */
}

setmap(ctx, vir, phys, len, type)
uint16 ctx;
vir_clicks vir;
phys_clicks phys;
vir_clicks len;
long type;
{
	register struct ctxinfo *c;
	long pageflags;

	if( (type&MAP_PROTMASK) == MAP_READWRITE )
		pageflags = FL_DT_PAGE;
	else if( (type&MAP_PROTMASK) == MAP_READONLY  )
		pageflags = FL_DT_PAGE|FL_WP;
	else {
		pageflags = FL_DT_INVALID;
		phys = OBMEMBASE>>CLICKSHIFT; /* actually unused */
	}

	c = &ctxinfo[ctx];
	c->ctx_inuse = 1;
	if (!is_mc68030)	/* Silly processor doesn't have it */
		Pflushr(&c->ctx_rpr);
	return fill_map(c, vir, phys, len, pageflags);
}

static clrBmap(b_table)
lbp_p b_table;
{
	register i;
	register lcp_p c_table;
	register struct pm_lftd *b_tabent;

	for (i=0; i<N_B_DESCR; i++) {
		b_tabent = &b_table->lbp_lB[i];
		if ((b_tabent->lftd_flags&FL_DT_MASK)==FL_DT_V4BYTE) {
			c_table = (lcp_p) b_tabent->lftd_taddr;
			c_table->lcp_next = lcp_freelist;
			lcp_freelist = c_table;
			lcp_free++;
		}
		b_tabent->lftd_flags = FL_DT_INVALID;
	}
}

clrmap(ctx)
uint16 ctx;
{
	register struct ctxinfo *c;
	register i;
	register lbp_p b_table;

	c = &ctxinfo[ctx];
	c->ctx_inuse = 0;
	for (i=0; i<N_A_DESCR; i++) {
		if ((c->ctx_lA[i].lftd_flags&FL_DT_MASK)==FL_DT_V8BYTE) {
			b_table = (lbp_p) c->ctx_lA[i].lftd_taddr;
			clrBmap(b_table);
			b_table->lbp_next = lbp_freelist;
			lbp_freelist = b_table;
			lbp_free++;
		}
		c->ctx_lA[i].lftd_flags = FL_DT_INVALID;
	}
}
#endif /* NOPROC */

disable_caching(addr, size)
vir_bytes addr,size;
{
	register result;

	if (!is_mc68030)
		return;	/* Don't bother */
	result = fill_map(rtctx, (vir_clicks) (addr>>CLICKSHIFT),
				(phys_clicks) (addr>>CLICKSHIFT),
				(phys_clicks) ((size+CLICKSIZE-1)>>CLICKSHIFT),
				(long) FL_DT_PAGE|FL_CI);
	compare(result, ==, 0);
	Pflusha();
}

static
setup_kernel_mapping(c, kmc, nkmc)
register struct ctxinfo *c;
register struct kernel_mem_chunks *kmc;
{
	register i;
	register result;

	for(i=0;i<nkmc;i++,kmc++) {
		result = fill_map(c, (vir_clicks) (kmc->kmc_base>>CLICKSHIFT),
			(phys_clicks) (kmc->kmc_base>>CLICKSHIFT),
			(phys_clicks) ((kmc->kmc_size+CLICKSIZE-1)>>CLICKSHIFT),
			kmc->kmc_flags|FL_DT_PAGE);
		compare(result, ==, 0);
	}
}

static
initpmmu() {
	register struct ctxinfo *c;
	register lbp_p b_table;
	register lcp_p c_table;
	register i;
	extern int textbegin, begin;
#ifndef ROMKERNEL
	int lowprotpage, highprotpage,lowsppage;
#endif

	/* First some sanity checks about alignment */
	compare(sizeof(struct ctxinfo)&0xF, ==, 0);

	/* Testing sanity of various boundaries */
	compare(IS+TIA+TIB+TIC+TID+CLICKSHIFT, ==, 32);

	rtctx = (struct ctxinfo *)
			aalloc((vir_bytes) sizeof(struct ctxinfo),16);
	for (i=N_LBP; i>0; i--) {
		b_table = (lbp_p) aalloc((vir_bytes) sizeof(lbp_t), 16);
		b_table->lbp_next = lbp_freelist;
		lbp_freelist = b_table;
	}
	lbp_free = lbp_minfree = N_LBP;
	for (i=N_LCP; i>0; i--) {
		c_table = (lcp_p) aalloc((vir_bytes) sizeof(lcp_t), 16);
		c_table->lcp_next = lcp_freelist;
		lcp_freelist = c_table;
	}
	lcp_free = lcp_minfree = N_LCP;

#ifndef NOPROC
	ctxinfo = (struct ctxinfo *)
			aalloc((vir_bytes) nproc * sizeof(struct ctxinfo),16);
	for (i=0,c=ctxinfo;i<nproc;i++,c++) {
		c->ctx_rpr.rpr_flags = FL_DT_V8BYTE;
		c->ctx_rpr.rpr_limit = LIM_UPPER|(N_A_DESCR-1);
		c->ctx_rpr.rpr_taddr = (long) c->ctx_lA;
	}
#endif
	pagemap = (long *) aalloc((vir_bytes) npages*sizeof(long),16);
	init_page_map();
#ifndef ROMKERNEL
	lowprotpage = (((long)&textbegin)-OBMEMBASE)>>CLICKSHIFT;
	highprotpage = (((long)&begin)-OBMEMBASE)>>CLICKSHIFT;
	lowsppage = (((long)KERNELSPBOT)-OBMEMBASE)>>CLICKSHIFT;
	/*
	 * set up a red zone for stack
	 */
	assert(lowsppage>=0 && lowsppage<npages);
	pagemap[lowsppage] = 0;
	if (lowprotpage>=0 && highprotpage<npages) {
		/*
		 * Kernel is running in RAM, but still prevent it from
		 * changing initialized data, so it can be moved to ROM
		 */
		for(i=lowprotpage; i<highprotpage; i++)
			pagemap[i] |= FL_WP;
	}
#endif

	c = rtctx;
	c->ctx_rpr.rpr_flags = FL_DT_V8BYTE;
	c->ctx_rpr.rpr_limit = LIM_UPPER|(N_A_DESCR-1);
	c->ctx_rpr.rpr_taddr = (long) c->ctx_lA;

	setup_kernel_mapping(c, kernel_mem_chunks,
						ARRAY_SIZE(kernel_mem_chunks));
	i = fill_map(c, (vir_clicks) (OBMEMBASE>>CLICKSHIFT),
				    (phys_clicks) (OBMEMBASE>>CLICKSHIFT),
				    (phys_clicks) npages, (long) FL_DT_PAGE);
	compare(i, ==, 0);

	PsetSrp(&c->ctx_rpr);		/* set supervisor root pointer */
	PsetTc(0L);
	PsetTc(TC_E|TC_SRE|		/* enable memory management */
		((long) CLICKSHIFT <<TC_PS_SHIFT)|
		((long) IS  <<TC_IS_SHIFT )|
		((long) TIA <<TC_TIA_SHIFT)|
		((long) TIB <<TC_TIB_SHIFT)|
		((long) TIC <<TC_TIC_SHIFT)|
		((long) TID <<TC_TID_SHIFT)
	);
}

/*ARGSUSED*/
void
bootkernel(entry, size, commandline, flags)
phys_bytes entry;
long size;
char *commandline;
int flags;
{

	PsetTc(0L);			/* disable MMU */
	(*(int (*)()) entry)();		/* Wheeeeehhh... */
}

#ifndef SMALL_KERNEL
static char *dt_strings[] = {
	"Invalid",
	"Page",
	"Short",
	"Long"
};

static char *
dump_flags(begin, end, flags, dt_expected)
char *  begin;  /* start of buffer to hold dump */
char *  end;    /* end of buffer */
long flags;
{
	char *p;

	p = begin;
	if (flags&FL_SG)
		p = bprintf(p, end, "SG");
	if (flags&FL_S)
		p = bprintf(p, end, "S");
	if (flags&FL_G)
		p = bprintf(p, end, "G");
	if (flags&FL_CI)
		p = bprintf(p, end, "C");
	if (flags&FL_L)
		p = bprintf(p, end, "L");
	if (flags&FL_M)
		p = bprintf(p, end, "M");
	if (flags&FL_U)
		p = bprintf(p, end, "U");
	if (flags&FL_WP)
		p = bprintf(p, end, "P");
	if ((flags&FL_DT_MASK) != dt_expected)
		p = bprintf(p, end, "/%s", dt_strings[flags&FL_DT_MASK]);
	return p;
}

static char * 
dump_ctable(begin, end, c_table)
char *  begin;  /* start of buffer to hold dump */
char *  end;    /* end of buffer */
lcp_p c_table;
{
	int i;
	int skipmode;
	char *p;
	long page,flags,prevaddr;

	p = begin;
	skipmode = 1;
	prevaddr = -1;
	for (i=0 ; i < N_C_DESCR; i++) {
		if (c_table->lcp_page[i]) {
			if (skipmode) {
				p = bprintf(p, end, "\n\t\t%d:", i);
				skipmode=0;
			}
			page = c_table->lcp_page[i];
			flags = page & 0xFF;
			page &= ~0xFF;
			if (page != prevaddr+(1<<CLICKSHIFT))
				p = bprintf(p, end, "%x", page);
			prevaddr = page;
			p = dump_flags(p, end, flags, FL_DT_PAGE);
			p = bprintf(p, end, ",");
		} else
			skipmode = 1;
	}
	return p;
}

static char * 
dump_btable(begin, end, b_table)
char *  begin;  /* start of buffer to hold dump */
char *  end;    /* end of buffer */
lbp_p b_table;
{
	int i;
	char *p;
	int desc_type;

	p = begin;
	for (i = 0; i < N_B_DESCR; i++) {
		desc_type = b_table->lbp_lB[i].lftd_flags&FL_DT_MASK;
		if (desc_type != FL_DT_INVALID) {
			p = bprintf(p, end, "\t%2d [", i);
			p = dump_flags(p, end,
				(long) b_table->lbp_lB[i].lftd_flags,
						FL_DT_V4BYTE);
			if (b_table->lbp_lB[i].lftd_limit != LIM_UNLIM)
				p = bprintf(p, end, ",%x",
					b_table->lbp_lB[i].lftd_limit);
			p = bprintf(p, end, ",%lx]",
					b_table->lbp_lB[i].lftd_taddr);
			if (desc_type == FL_DT_V4BYTE) {
				p = dump_ctable(p, end,
					(lcp_p) b_table->lbp_lB[i].lftd_taddr);
			}
			p = bprintf(p, end, "\n");
		}
	}
	return p;
}

static char *
dump_ctx(begin, end, c)
char *begin;	/* start of buffer to hold dump */
char *end;	/* end of buffer */
struct ctxinfo *c;
{
	char *p;
	register i;
	int desc_type;

	p = begin;
	for (i = 0; i < N_A_DESCR; i++) {
		desc_type = c->ctx_lA[i].lftd_flags&FL_DT_MASK;
		if (desc_type != FL_DT_INVALID) {
			p = bprintf(p, end, "%2d [", i);
			p = dump_flags(p, end, (long) c->ctx_lA[i].lftd_flags,
						FL_DT_V8BYTE);
			if (c->ctx_lA[i].lftd_limit != LIM_UNLIM)
				p = bprintf(p, end, ",%x",
						c->ctx_lA[i].lftd_limit);
			p = bprintf(p, end, ",%lx]\n", c->ctx_lA[i].lftd_taddr);
		}
		if (desc_type == FL_DT_V8BYTE)
			p = dump_btable(p, end,
					(lbp_p) c->ctx_lA[i].lftd_taddr);
	}
	return p;
}

pmmudump(begin, end)
char *	begin;	/* start of buffer to hold dump */
char *	end;	/* end of buffer */
{
	register ctx;
	register struct ctxinfo *c;
	register char *	p;

	p = bprintf(begin, end, "Tc is %lx\n", PgetTc());
#ifndef NOPROC
	p = bprintf(p, end, "cache_reset = %x\n", cache_reset);
#endif
	p = bprintf(p, end, "npages = %d\n", npages);
	p = bprintf(p, end, "free level B page tables %d, min %d\n",
			lbp_free, lbp_minfree);
	p = bprintf(p, end, "free level C page tables %d, min %d\n",
			lcp_free, lcp_minfree);
	p = bprintf(p, end, "pagetree kernel\n");
	p = dump_ctx(p, end, rtctx);
#ifndef NOPROC
	for (ctx = 0; ctx < nproc; ctx++) {
		c = &ctxinfo[ctx];
		if (!c->ctx_inuse)
			continue;
		p = bprintf(p, end, "pagetree process %d:\n", ctx);
		p = dump_ctx(p, end, c);
	}
#endif
	return p - begin;
}

#endif /* SMALL_KERNEL */

#ifndef BOOTPROM_ADDR
#define BOOTPROM_ADDR 0
#endif

static int (*romjump)() = (int(*)()) BOOTPROM_ADDR;

reboot()
{
	PsetTc(0L);		/* disable pmmu */
	(*romjump)();
}

#ifdef PMMUDEBUG

pmmudebug(addr) char *addr; {
	long *dp;
	uint16 psr;

	psr = Ptest(addr, 1, &dp);
	printf("Uberr: addr=%lx, psr=%x, descriptor=%lx(%lx,%lx)\n",
		addr, psr, dp, dp[0], dp[1]);
	/*
	printf("char = %x, short=%x, long=%lx\n",*addr, *((short *) addr), *((long *) addr));
	*/
}

#endif PMMUDEBUG

int
mmrerun(addr, fc)
char *addr;
int fc;
{
	long *dp;
	uint16 psr;

	psr = Ptest(addr, fc, &dp);
	if (psr&PSR_I)
		return 0;
	Ploadw(addr);
	return 1;
}

void
mm_initseg() 
{
	phys_clicks begin_click, end_click;
	extern char end;

	hunt_for_mem();

	/* calculate start and end of the kernel */
	begin_click= 0;
	end_click= BSTOC((long)&end);

	/* setup segments */
	seg_reg_memchunk(BATOC(OBMEMBASE), npages);	/* all free memory */
	seg_mark_used(begin_click, end_click-begin_click, (long) ST_KERNEL);
}

void
initmm() {
	initpmmu();			/* Initialise PMMU chip */
#ifndef NOPROC
	initcache();
#endif
}
