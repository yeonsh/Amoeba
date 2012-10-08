/*	@(#)prof_cxx.c	1.1	96/02/27 11:13:59 */
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

_prof_init()
{
    /* Profile initilazation func to be called by C++ programs.
     * The __main() trick from prof__main.c cannot be used here
     * because that's used by the C++ initialization functions.
     */
    monstartup(__text_start, etext);
    atexit(_mcleanup);
}

