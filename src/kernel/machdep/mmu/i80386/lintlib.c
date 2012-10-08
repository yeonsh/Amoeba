/*	@(#)lintlib.c	1.7	96/02/27 13:58:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * lintlib.c
 *
 * Lint library for variables in khead.S
 */
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <map.h>
#include <process/proc.h>
#include <i80386.h>
#include <type.h>
#include <seg.h>

/*
 * the next two are actually assembler labels but so we can use them from
 * C we disguise them as characters and take there address where we need
 * them.
 */
char	kst_beg;	/* beginning of kernel stack */
char	kst_end;	/* end of kernel stack */

/* another label marker - from the loader: at the end of bss */
char 	end;

/* the int we put at the beginning of bss */
int	begin;

/* Intel 80386 MMU structures */
long		*kpd;
long		*kpt;
struct tss_s	 tss;
struct segdesc_s gdt[GDT_SIZE];

