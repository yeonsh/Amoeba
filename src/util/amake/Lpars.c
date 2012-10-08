/*	@(#)Lpars.c	1.3	96/02/27 13:05:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* LLgen generated code from source . */
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
#define LLNOFIRSTS
#define LL_LEXI lexan
#define LL_SSIZE 9
#define LL_NSETS 11
#define LL_NTERMINALS 67
LLcmdline() {
	unsigned int s[LL_NTERMINALS+LL_NSETS+2];
	LLnewlevel(s);
	LLread();
	L2_cmd_line_def();
	LL_SCANDONE(EOFILE);
	LLoldlevel(s);
}
LLstatefile() {
	unsigned int s[LL_NTERMINALS+LL_NSETS+2];
	LLnewlevel(s);
	LLread();
	L1_state_file();
	LL_SCANDONE(EOFILE);
	LLoldlevel(s);
}
LLparse() {
	unsigned int s[LL_NTERMINALS+LL_NSETS+2];
	LLnewlevel(s);
	LLread();
	L0_amake_program();
	LL_SCANDONE(EOFILE);
	LLoldlevel(s);
}
static char LLsets[] = {
032,0304,0171,055,0220,06,0220,010,01,
032,00,0322,040,024,06,0220,010,01,
0172,01,0120,0140,020,06,0220,0370,03,
02,00,00,00,00,00,0120,00,00,
032,00,0120,040,020,06,0220,010,01,
00,02,00,0222,0120,00,00,00,02,
00,02,00,0202,0120,00,00,00,00,
032,00,0120,040,020,06,0220,00,01,
012,00,00,00,020,00,00,00,00,
02,00,00,00,00,00,00,00,06,
00,00,04,00,00,010,00,00,00,
0 };
#define LLindex (LL_index+1)
static short LL_index[] = {0,0,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
64,
-1,
-1,
-1,
60,
-1,
52,
54,
-1,
61,
53,
-1,
-1,
59,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
58,
51,
-1,
57,
-1,
62,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
65,
63,
66,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
55,
-1,
56,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
-1,
0,
1,
2,
3,
4,
5,
6,
7,
8,
9,
10,
11,
12,
13,
14,
15,
16,
17,
18,
19,
20,
21,
22,
23,
24,
25,
26,
27,
28,
29,
30,
31,
32,
33,
34,
35,
36,
37,
38,
39,
40,
41,
42,
43,
44,
45,
46,
47,
48,
49,
50,
0 };
#define LL_NEWMESS
/* 
 * Some grammar independent code.
 * This file is copied into Lpars.c.
 */

# ifdef LL_DEBUG
# include <stdio.h>
# endif

unsigned int	LLtcnt[LL_NTERMINALS];
unsigned int	LLscnt[LL_NSETS];
int		LLcsymb, LLsymb;
static int	LLlevel;

/* In this file are defined: */
extern		LLread();
extern int	LLskip();
extern int	LLnext();
#ifndef LLscan
extern		LLscan();
#endif
extern		LLerror();
# ifndef LLNOFIRSTS
extern int	LLfirst();
# endif
extern		LLnewlevel();
extern		LLoldlevel();

#ifndef LLscan
LLscan(t) {
	/*
	 * Check if the next symbol is equal to the parameter
	 */
	if ((LLsymb = LL_LEXI()) == t) {
		return;
	}
	/*
	 * If we come here, an error has been detected
	 */
	LLerror(t);
}
#endif

LLread() {
	for (;;) {
		if ((LLcsymb = LLindex[(LLsymb = LL_LEXI())]) >= 0) return;
		LLmessage(0);
	}
	/* NOTREACHED */
}

LLerror(t) {
	register int i;

	if (t == EOFILE && LLsymb <= 0) return;
#ifdef LL_NEWMESS
	if (t == EOFILE) {
#ifdef LL_USERHOOK
		static int lst[] = { EOFILE, 0 };
		LLuserhook(EOFILE, lst);
#endif /* LL_USERHOOK */
		if (LLsymb != EOFILE && LLsymb > 0) {
			LLmessage(-1);
			while ((LLsymb = LL_LEXI()) > 0 && LLsymb != EOFILE) 
				/* nothing */ ;
		}
		return;
	}
#endif
	if ((LLcsymb = LLindex[LLsymb]) < 0) {
		LLmessage(0);
		LLread();
	}
	i = LLindex[t];
	LLtcnt[i]++;
#ifdef LL_USERHOOK
	if (LLdoskip(t)) /* nothing */;
#else
	if (LLskip()) /* nothing */;
#endif
	LLtcnt[i]--;
	if (LLsymb != t) LLmessage(t);
}

# ifndef LLNOFIRSTS
LLfirst(x, d) {
	register int i;

	return (i = LLindex[x]) >= 0 &&
		(LLsets[d + (i >> 3)] & (1 << (i & 07)));
}
# endif

LLnext(n) {
	/*	returns: 0 if the current symbol is'nt skipped, and it
			 is'nt a member of "n",
			 1 if we have a new symbol, but it is'nt a member,
			 2 if the current symbol is a member,
			 and 3 if we have a new symbol and it is a member.
		So, the low order bit indicates wether we have a new symbol,
		and the next bit indicates wether it is a member of "n".
	*/

	int retval = 0;

	if (LLskip()) retval = 1;
	if (n <= 0 && LLsets[(LLcsymb >> 3) - n] & (1 << (LLcsymb & 07))) {
		retval |= 2;
	}
	else if (n > 0 && LLcsymb == LLindex[n]) retval |= 2;
	return retval;
}

LLskip() {
	/*	returns 0 if the current symbol is'nt skipped, and
		1 if it is, t.i., we have a new symbol
	*/
#ifdef LL_USERHOOK
	return LLdoskip(0);
}

LLuserhook(exp, list)
	int *list;
{
	int old = LLsymb;
	LL_USERHOOK(exp, list);
	LLread();
	return LLsymb != old;
}

LLmklist(list)
	register int *list;
{
	char Xset[LL_SSIZE];
	register char *p;
	register int i;

	for (p = &Xset[0]; p < &Xset[LL_SSIZE]; ) *p++ = 0;
	for (i = 0; i < LL_NTERMINALS; i++) {
		if (LLtcnt[i] != 0) Xset[i >> 3] |= (1 << (i & 07));
	}
	for (i = LL_NSETS - 1; i >= 0; i--) if (LLscnt[i] != 0) {
		register char *q = &LLsets[LL_SSIZE * i];

		p = &Xset[0];
		while (p < &Xset[LL_SSIZE]) *p++ |= *q++;
	}
	for (i = 0; i < LL_NTERMINALS; i++) {
		if (Xset[i >> 3] & (1 << (i & 07))) {
			*list++ = LLtok[i];
		}
	}
	*list = 0;
}

LLdoskip(exp) {
	int LLx;
	int list[LL_NTERMINALS+1];
#endif /* LL_USERHOOK */
	register int i;
	int retval;
	int LLi, LLb;

	retval = 0;
#ifdef LL_USERHOOK
	LLmklist(list);
	LLx = LLuserhook(exp, list);
	if (LLx) retval = 1;
#endif /* LL_USERHOOK */
	for (;;) {
		if (LLtcnt[LLcsymb] != 0) {
#ifdef LL_USERHOOK
			if (!exp || !LLx || LLcsymb == LLindex[exp])
#endif
			return retval;
		}
		LLi = LLcsymb >> 3;
		LLb = 1 << (LLcsymb & 07);
		for (i = LL_NSETS - 1; i >= 0; i--) {
			if (LLscnt[i] != 0) {
				if (LLsets[LL_SSIZE*i+LLi] & LLb) {
#ifdef LL_USERHOOK
					if (!exp || !LLx || LLcsymb == LLindex[exp])
#endif
					return retval;
				}
			}
		}
#ifdef LL_USERHOOK
		if (LLx) {
			LLx = LLuserhook(exp, list);
			continue;
		}
#endif /* LL_USERHOOK */
		LLmessage(0);
		retval = 1;
		LLread();
	}
	/* NOTREACHED */
}

LLnewlevel(LLsinfo) unsigned int *LLsinfo; {
	register int i;

	if (LLlevel++) {
		LLsinfo[LL_NSETS+LL_NTERMINALS] = (unsigned) LLsymb;
		LLsinfo[LL_NSETS+LL_NTERMINALS+1] = (unsigned) LLcsymb;
		for (i = LL_NTERMINALS - 1; i >= 0; i--) {
			LLsinfo[i] = LLtcnt[i];
			LLtcnt[i] = 0;
		}
		for (i = LL_NSETS - 1; i >= 0; i--) {
			LLsinfo[LL_NTERMINALS+i] = LLscnt[i];
			LLscnt[i] = 0;
		}
	}
	LLtincr(0);
}

LLoldlevel(LLsinfo) unsigned int *LLsinfo; {
	register int i;

	LLtdecr(0);
# ifdef LL_DEBUG
	for (i = 0; i < LL_NTERMINALS; i++) LL_assert(LLtcnt[i] == 0);
	for (i = 0; i < LL_NSETS; i++) LL_assert(LLscnt[i] == 0);
# endif
	if (--LLlevel) {
		for (i = LL_NSETS - 1; i >= 0; i--) {
			LLscnt[i] = LLsinfo[LL_NTERMINALS+i];
		}
		for (i = LL_NTERMINALS - 1; i >= 0; i--) {
			LLtcnt[i] = LLsinfo[i];
		}
		LLsymb = (int) LLsinfo[LL_NSETS+LL_NTERMINALS];
		LLcsymb = (int) LLsinfo[LL_NSETS+LL_NTERMINALS+1];
	}
}

# ifdef LL_DEBUG
LL_badassertion(asstr,file,line) char *asstr, *file; {

	fprintf(stderr,"Assertion \"%s\" failed %s(%d)\n",asstr,file,line);
	abort();
}
# endif
