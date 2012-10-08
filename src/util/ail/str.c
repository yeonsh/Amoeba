/*	@(#)str.c	1.3	94/04/07 14:39:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include <stdlib.h>

static struct stind *StrIndex[MAX_HASH];	/* Contains NULL's */

/*
 *	Allocation routine that always
 *	returns enough memory (IF it returns).
 */
char *
MyAlloc(n)
    unsigned int n;
{
    char *ptr;

    ptr = (char *) malloc((size_t) n);
    if (ptr == NULL) fatal("Out of memory.");
    chkptr(ptr);
    return ptr;
} /* MyAlloc */

/*
 *	StrSave: save a string somewhere.
 */
static char *
StrSave(s, len)
    char *s;
    int len;
{
    char *ptr;
    ptr = MyAlloc((unsigned) ++len);
    while ((*ptr++ = *s++) != '\0')
	;
    return ptr-len;
} /* StrSave */

/*
 *	Simple hash function: returns values
 *	in the range 0..MAX_HASH-1.
 *	Puts the length of the string at *len.
 */
int
hash(s, len)
    char *s;
    int *len;
{
    char *cptr;
    int sum = 0;
    for (cptr = s; *cptr != '\0'; sum = (sum + *cptr++ ) % MAX_HASH)
	;
    *len = cptr - s;
    return sum;
} /* Hash */

char *
StrFind(s)
    char *s;
{
    struct stind *trav;
    int len, hv;	/* Length and hashvalue of the string. */

    hv = hash(s, &len);
    for (trav=StrIndex[hv]; trav!=NULL; trav=trav->st_nxt)
	if (trav->st_len == len && strcmp(s, trav->st_str)==0)
	    return trav->st_str;	/* Found */
    /* Not found, make new node. */
    trav=new(struct stind);
    trav->st_str = StrSave(s, len);
/*  dbg3("String %s saved at %p\n", s, trav->st_str);	*/
    trav->st_len = len;
    /* And put in the list. */
    trav->st_nxt = StrIndex[hv];
    StrIndex[hv] = trav;
    return trav->st_str;
} /* StrFind */
