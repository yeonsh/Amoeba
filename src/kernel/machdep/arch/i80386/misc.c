/*	@(#)misc.c	1.5	96/02/27 13:45:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * misc.c
 *
 * Special 80386 debug routines, and other routine that don't fit in
 * anywhere else.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <amoeba.h>
#include <global.h>
#include <machdep.h>
#include <assert.h>
INIT_ASSERT
#include <fault.h>
#include <exception.h>
#include <syscall.h>
#include <map.h>
#include <process/proc.h>
#include <kthread.h>
#include <i80386.h>
#include <type.h>
#include <trap.h>
#include <ndp.h>
#include <sys/proto.h>

#if defined(ISA) || defined(MCA)
/*
 * Scan kernel command line options, each of them has the
 * following format:
 *
 *	<kernelname> [[-]<arg>:<level>] ...
 *
 * Where <arg> is the name of the module, and <level> its debug level.
 */
#include <bootinfo.h>

#define	isspace(c)	((c)==' ' || (c)=='\t')
#define isxdigit(c)	(((c)>='0' && (c)<='9') || ((c)>='A' && (c)<='F'))

int
kernel_option(arg)
    char *arg;
{
    struct bootinfo *bootinfo;
    register char *p, *q;
    register int n;

    assert(sizeof(struct bootinfo) <= 2048);
    bootinfo = (struct bootinfo *)BI_ADDR;
    if (bootinfo->bi_magic != BI_MAGIC) return 0;

    for (p = bootinfo->bi_line; !isspace(*p) && *p != '\0'; p++)
	/* skip kernel name */ ;
    while (*p != '\0') {
	while (isspace(*p)) p++;
	if (*p == '-') p++;
	for (q = arg; *q == *p; q++, p++)
	    /* nothing */;
	if (*q || *p != ':') {
	    while (!isspace(*p) && *p != '\0') p++;
	    continue;
	}
	for (n = 0, p++; isxdigit(*p); p++) {
	    n <<= 4;
	    if (*p >= '0' && *p <= '9')
		n += *p - '0';
	    if (*p >= 'A' && *p <= 'F')
		n += *p - 'A' + 10;
	}
	return n;
    }
    return 0;
}
#endif /* defined(ISA) || defined(MCA) */

#ifdef STATISTICS
/*
 * Maintain interrupt counters for gathering statistics
 */
uint32 icount[NIRQ];

int
intdump(begin, end)
    char *begin, *end;
{
    register char *p;
    register int i;

    p = bprintf(begin, end, "======== INTERRUPT statistics =======\n");
    for (i = 0; i < NIRQ; i++)
	p = bprintf(p, end, "IRQ %2d: #%lu\n", i, icount[i]);
    return p - begin;
}
#endif /* STATISTICS */

#ifndef NDEBUG
/*
 * Print CPU and NDP type
 */
void
dump_cpu_ndp_type()
{
    extern int cpu_type, ndp_type;

    printf("CPU type %d", cpu_type);
    if (ndp_type != NDP_NONE)
	printf(", and NDP type %d", ndp_type);
    printf("\n");
}

/*
 * Dump contents of global descriptor table
 */
void
dump_gdt()
{
    extern struct segdesc_s gdt[];
    void print_gdt();

    print_gdt(gdt, GDT_SIZE);
}

void
print_gdt(gdtbase, gdtsize)
    struct segdesc_s *gdtbase;
    int gdtsize;
{
    register int i;

    printf("Global descriptor table (0x%x):\n", gdtbase);
    printf("gdt[ 0]: Null descriptor\n");
    for (i = 1; i < gdtsize; i++) {
	register struct segdesc_s *g = &gdtbase[i];
	printf("gdt[%2d]=0x%02x: limit=%05x, base=%08x, access=%2x, gran=%1x\n",
	    g - gdtbase, i, g->limit_low + ((g->gran & 0xF) << 16), 
	    g->base_low + (g->base_mid << 16) + (g->base_high << 24),
	    g->access, ((int)g->gran & 0xF0) >> 4);
    }
}

/*
 * Dump contents of task state segment
 */
void
dump_tss()
{
    extern struct tss_s tss;
    char *bitmap, *p;
    int l;
  
    printf("TSS size = %d\n", TSS_SIZE);
    printf("ss0  %4x, sp0 %8x\n", tss.ss0, tss.sp0);
    printf("ss1  %4x, sp1 %8x\n", tss.ss1, tss.sp1);
    printf("ss2  %4x, sp2 %8x\n", tss.ss2, tss.sp2);
    printf("trap %4x, iobase  %4x\n", tss.trap, tss.iobase);

    printf("bitmap");
    bitmap = (char *)&tss + SIZEOF_TSS;
    for (p = bitmap, l = 0; p < bitmap + ((MAX_TSS_IOADDR+7)/8); p++) {
	if (++l > 16) {
	    printf("\n      ");
	    l = 1;
	}
	printf(" %02x", *p & 0xFF);
    }
    printf("\n");
}

/*
 * Dump contents of kernel page table directory
 */
void
dump_kpd()
{
    register int i, j;
    extern long *kpd;
    long *pt;

    for (i = 0; i < PAGE_TABLE_ENTRIES; i++) {
	if (kpd[i] != 0) {
	    printf("kpd[%d] = 0x%x\n", i, kpd[i]);
	    pt = (long *)(kpd[i] & ~(CLICKSIZE - 1));
	    for (j = 0; j < PAGE_TABLE_ENTRIES; j++)
		printf("[%d:0x%x] ", j, pt[j]);
	    printf("\n");
	}
    }
}
#endif /* NDEBUG */
