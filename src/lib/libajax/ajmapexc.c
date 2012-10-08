/*	@(#)ajmapexc.c	1.2	94/04/07 09:42:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Map exceptions to signals (shared by sigaction.c and waitpid.c) */

#include <amoeba.h>
#include <exception.h>
#include <sys/types.h> 
#include <signal.h>

int
_ajax_mapexception(exc)
	long exc;
{
	/* This code is rearranged to make it smaller */
	switch (exc) {
	
	case SIG_TRANS:
		return SIGAMOEBA;
	
	case EXC_ODD:
	case EXC_ACC:
	case EXC_MEM:
		return SIGSEGV;
	
	case EXC_BPT:
		return SIGTRAP;
	
	case EXC_FPE:
	case EXC_DIV:
		return SIGFPE;
	
	case EXC_SYS:
		return SIGSYS;
	
	case EXC_ABT:
		return SIGABRT;
	
	/*
	case EXC_INS:
	case EXC_ARG:
	case EXC_ILL:
	*/
	default:
		return SIGILL;
	
	}
}
