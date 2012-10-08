/*	@(#)lintlib.c	1.9	96/02/27 13:45:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * lintlib.c
 *
 * Lint library for routines in as.S and interrupt.S.
 */
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <map.h>
#include <exception.h>
#include <process/proc.h>
#include <i80386.h>
#include <type.h>
#include <seg.h>
#include <fault.h>

/*LINTLIBRARY*/
void page_enable(a) long * a; { return; }
void page_disable() { return; }

/*
 * We have to use newsys and lots of other routines called from
 * assembler but defined in C.  For simplicity we do it in waitint().
 */
long newsys();

void waitint() {
#if defined(ISA) || defined(MCA)
	void nmi();

	nmi(0, (struct fault *) 0);
#endif
	newsys((long) 0, (long *) 0);
	swtrap((signum) 0, 0, (struct fault *) 0);
	trap(0, (struct fault *) 0);
	return;
}

void enable() { return; }
void disable() { return; }

int get_flags() { return 0; }
void set_flags(a) int a; { return; }

int getCR0() { return 0; }
int getCR2() { return 0; }
int getCR3() { return 0; }
void setCR0(a) int a; { return; }
void setCR3(a) phys_bytes a; { return; }

#ifdef KGDB
void breakpoint() { return; }
#endif /* KGDB */

void flushTLB() { return; }

void out_byte(p, v) int p, v; { return; }
void out_word(p, v) int p, v; { return; }
void out_long(p, v) int p; long v; { return; }
int  in_byte(p) int p; { return 0; }
int  in_word(p) int p; { return 0; }
long  in_long(p) int p; { return 0; }

void outs_byte(p, ptr, len) /* output byte string */
int p;
char * ptr;
int len;
{
    return;
}

void outs_word(p, ptr, len) /* output word string */
int p;
char * ptr;
int len;	/* in bytes! */
{
    return;
}

void ins_byte(p, ptr, len) /* input byte string */
int p;
char * ptr;
int len;
{
    return;
}

void ins_word(p, ptr, len) /* input word string */
int p;
char * ptr;
int len;	/* in bytes! */
{
    return;
}

int irq_0_vect() { return 0; }
int irq_1_vect() { return 0; }
int irq_2_vect() { return 0; }
int irq_3_vect() { return 0; }
int irq_4_vect() { return 0; }
int irq_5_vect() { return 0; }
int irq_6_vect() { return 0; }
int irq_7_vect() { return 0; }
int irq_8_vect() { return 0; }
int irq_9_vect() { return 0; }
int irq_10_vect() { return 0; }
int irq_11_vect() { return 0; }
int irq_12_vect() { return 0; }
int irq_13_vect() { return 0; }
int irq_14_vect() { return 0; }
int irq_15_vect() { return 0; }

int aligntrap() { return 0; }
int div0trap() { return 0; }
int dbgtrap() { return 0; }
int brktrap() { return 0; }
int ovflotrap() { return 0; }
int boundstrap() { return 0; }
int invoptrap() { return 0; }
int ndptrap() { return 0; }
int invaltrap() { return 0; }
int invtsstrap() { return 0; }
int segnptrap() { return 0; }
int stktrap() { return 0; }
int gptrap() { return 0; }
int fptrap() { return 0; }

void ndp_save(ndp_state) int ndp_state[]; {}
void ndp_restore(ndp_state) int ndp_state[]; {}
void ndp_reinit() {}
void ndp_clearerr() {}
