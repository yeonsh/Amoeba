/*	@(#)tokenfile.c	1.2	94/04/07 14:54:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* LLgen generated code from source tokenfile.g */
#ifdef LL_DEBUG
#define LL_assert(x) if(!(x)) LL_badassertion("x",__FILE__,__LINE__)
#else
#define LL_assert(x)	/* nothing */
#endif

extern int LLsymb;

#define LL_SAFE(x)	/* Nothing */
#define LL_SSCANDONE(x)	if (LLsymb != x) LLerror(x); else
#define LL_SCANDONE(x)	if (LLsymb != x) LLerror(x); else
#define LL_NOSCANDONE(x) LLscan(x)
#ifdef LL_FASTER
#define LLscan(x)	if ((LLsymb = LL_LEXI()) != x) LLerror(x); else
#endif

# include "Lpars.h"

extern unsigned int LLscnt[];
extern unsigned int LLtcnt[];
extern int LLcsymb;

#define LLsdecr(d)	{LL_assert(LLscnt[d] > 0); LLscnt[d]--;}
#define LLtdecr(d)	{LL_assert(LLtcnt[d] > 0); LLtcnt[d]--;}
#define LLsincr(d)	LLscnt[d]++
#define LLtincr(d)	LLtcnt[d]++
#define LL_LEXI lexan


