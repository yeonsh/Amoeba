/*	@(#)prof__main.c	1.2	94/04/07 10:58:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

extern void _mcleanup();
extern void etext();
extern void (*__text_start)();

#ifndef lint
/*
 * Point __text_start to the start of the text segment.
 */

#if defined(__sparc__) || defined(sparc)
__asm__ ("
	.seg	\"data\"
	.align  4
	.global ___text_start
___text_start:
	.word   start
");
#endif

#if  defined(__i80386__) || defined(i80386)
extern void _start();
void (*__text_start)() = _start;
#endif

#if defined(__mc68000__) || defined(mc68000)
__asm__ ("
	.data
	.even
	.globl ___text_start
___text_start:
	.long  start
");
#endif

#endif /* lint */

__main()
{
    /* Alternative runtime startoff used for profiling
     * This overrides the default __main() from libamoeba.a.
     */
    monstartup(__text_start, etext);
    atexit(_mcleanup);
}

