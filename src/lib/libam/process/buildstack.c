/*	@(#)buildstack.c	1.4	96/02/27 11:02:22 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Build stack segment. */

#include "amoeba.h"
#include "module/proc.h"
#include "string.h"

#ifndef NULL
#define NULL 0
#endif

#if  !defined(AMOEBA) && !defined(__STDC__)
#define memcpy(to, from, nbytes)	bcopy(from, to, nbytes)
#endif

/* In a bottom-to-top fashion, here follow some tools for pushing
 * various data onto the stack.
 * Parameter meanings:
 * start	stack's start address in target VM (Virtual Memory)
 *  	s	start of buffer allocated for stack
 *  	e	end of unused part of that buffer;
 *  		can be 0 if too much has been pushed on it
 *	ptr	a pointer into the stack buffer,
 *		to be translated into target VM
 * Functions and macros:
 *	ptr2vm		translate a pointer into the stack buffer to target VM
 *	push_bytes	push a number of uninterpreted bytes onto the stack
 *	push_ptr	push a pointer, translating it to target VM
 *	push_uint32	push a 32-bit int
 *	push_str	push a null-terminated string
 *	push_align	align the stack pointer to a 4-byte boundary
 * All push_* functions and macros return the new value for e;
 * they return NULL if too much has been pushed, or if e was 0 on entry.
 */

#define ptr2vm(start, s, ptr) \
	( (ptr) == NULL ? 0 : (start) + ((ptr) - (s)) )

#define push_ptr(start, s, e, ptr) \
	push_uint32(s, e, ptr2vm(start, s, ptr))


static char *
push_bytes(s, e, data, n)
char *s, *e;
char *data;
int n;
{
    if (e != NULL && e-n >= s) {
	e -= n;
	memcpy(e, data, n); /* non overlapping */
	return e;
    } else {
	return NULL;
    }
}

static char *
push_uint32(s, e, data)
char *s, *e;
uint32 data;
{
    return push_bytes(s, e, (char*)&data, sizeof data);
}

static char *
push_str(s, e, data)
char *s, *e;
char *data;
{
    return push_bytes(s, e, data, strlen(data) + 1);
}

static char *
push_align(s, e)
char *s, *e;
{
    return push_bytes(s, e, "\0\0\0", (e-s) & 3);
}


static int
count_caps(caps, str_len, cap_len)
struct caplist caps[];
int *str_len;
int *cap_len;
{
    register int ncap;
    register int slen, clen;

    /* calculate the size of the capability array and the string array */
    for (slen = 0, clen = 0, ncap = 0; caps[ncap].cl_name != NULL; ncap++) {
	clen += CAPSIZE;
	slen += strlen(caps[ncap].cl_name) + 1;
    }

    *str_len = slen;
    *cap_len = clen;
    return ncap;
}

/* Push the capability environment onto the stack.
 * Order (from highest stack address to lowest):
 * 1)	the capabilities
 * 2)	the null-terminated names
 * 3)	alignment bytes
 * 4)	the 'capv' array, consisting of pairs <straddr, capaddr>,
 *	terminated by two NULL pointers (this must be pushed starting
 *	at the end because the stack grows downward)
 */

static char *
push_caps(start, s, e, caps)
uint32 start;
char  *s;	/* lowest address of stack */
char  *e;	/* address to start pushing */
struct caplist caps[];
{
    int ncap;	/* total # entries in capability environment */
    int slen;	/* total length of all the string names */
    int clen;	/* total length of all the capabilities */
    char *eptr;	/* place where the pointers go */
    char *estr;	/* place where the strings go */
	
    if (e == NULL) {
	return NULL;
    }

    if (caps == NULL) {	/* Use default array with no capabilities in it: */
	static struct caplist nullv[1] = { NULL, NULL };

	caps = nullv;
    }

    ncap = count_caps(caps, &slen, &clen);

    /* Calculate where to start adding the strings and pointers.
     * The capabilities go at the top (ie at 'e').
     */
    estr = e - clen;
    if (estr - slen < s) {
	return NULL;
    }
    eptr = push_align(s, estr - slen);

    /* push null pointers that terminate the pointer lists */
    eptr = push_ptr(start, s, eptr, (char *) NULL);
    eptr = push_ptr(start, s, eptr, (char *) NULL);

    /* Now push the data plus pointers onto the appropriate places
     * on the stack
     */
    while (--ncap >= 0) {
	e = push_bytes(s, e, (char *) caps[ncap].cl_cap, CAPSIZE);
	estr = push_str(s, estr, caps[ncap].cl_name);
	eptr = push_ptr(start, s, eptr, e);
	eptr = push_ptr(start, s, eptr, estr);
    }
    return eptr;
}

static int
count_strings(strings, size)
char *strings[];
int  *size;
{
    register char **strp;
    register int len;

    /* calculate length of strings in strings[] and number of strings */
    for (len = 0, strp = strings; *strp != NULL; strp++) {
	len += strlen(*strp) + 1;
    }

    *size = len;
    return strp - strings;
}

/* Push an array of strings, either argc or envp.
 * Order (from highest address to lowest):
 *	1) the strings
 *	2) alignment bytes
 *	3) a NULL-terminated array of pointers to the strings
 */

static char *
push_strings(start, s, e, strv, nstrings)
uint32 start;	 /* the virtual address in the child of the stack */
char  *s;	 /* the lowest address of the stack */
char  *e;	 /* the place where the next string is pushed */
char  *strv[];	 /* the data to go on the stack */
int   *nstrings; /* in which we return number of strings pushed */
{
    int  nstr;
    int  len;	/* total length of all the strings in strv */
    char *eptr;	/* the place where the next pointer is pushed */

    if (e == NULL) {	/* check for (previous) buffer overflow */
	return NULL;
    }

    if (strv == NULL) {	/* Use default array with no strings in it */
	static char *nullv[1] = { NULL };

	strv = nullv;
    }

    nstr = count_strings(strv, &len);
    *nstrings = nstr;

    eptr = push_align(s, e - len);
    eptr = push_ptr(start, s, eptr, (char *) NULL);

    while (--nstr >= 0) {
	e = push_str(s, e, strv[nstr]);
	eptr = push_ptr(start, s, eptr, e);
    }
    return eptr;
}

/* Build a stack segment in a buffer.
 * Parameters are the starting (target VM) address and the argument
 * list, the environment list and the capability environment list.
 * The return value is the computed stack pointer, or 0 if the data didn't fit.
 */

unsigned long
buildstack(buf, buflen, start, argv, envp, caps)
char          *buf;	/* In: stack buffer */
long           buflen;	/* In: stack buffer length */
unsigned long  start;	/* In: VM start of buffer */
char          *argv[];	/* In: NULL-terminated argument list */
char          *envp[];	/* In: NULL-terminated environment list */
struct caplist caps[];	/* In: capability list */
{
    int nstr, nargs;
    char *s = buf;
    char *e = buf + buflen;
    char *capptr, *envptr, *argptr;
	
    capptr = e = push_caps((uint32) start, s, e, caps);
    envptr = e = push_strings((uint32) start, s, e, envp, &nstr);
    argptr = e = push_strings((uint32) start, s, e, argv, &nstr);
    nargs = nstr;
    e = push_ptr((uint32) start, s, e, capptr);
    e = push_ptr((uint32) start, s, e, envptr);
    e = push_ptr((uint32) start, s, e, argptr);

    /* We store the arg count as a 32 bit integer and the stackfix routine
     * must make sure that what it puts on the stack on the other side is
     * an int on the machine where it will run.
     */
    e = push_uint32(s, e, nargs);

    if (e == NULL) {
	return 0;
    } else {
	return ptr2vm((uint32) start, s, e);
    }
}

int
_stack_required(argv, envp, caps)
char *argv[];		/* In: NULL-terminated argument list */
char *envp[];		/* In: NULL-terminated environment list */
struct caplist caps[];	/* In: capability list */
{
    /* This routine returns the number of bytes that is at least required
     * when building a stack segment for a new process.
     * Note this is not enough:  the caller should add extra space to give
     * the process a chance of actually using the stack where it is meant for.
     */
    int required = 0;
    int str_len, cap_len;

    /* add space required for cap list */
    (void) count_caps(caps, &str_len, &cap_len);
    required += str_len + cap_len;

    /* add space required for environment */
    if (envp != NULL) {
	(void) count_strings(envp, &str_len);
	required += str_len;
    }

    /* add space required for argument list */
    if (argv != NULL) {
	(void) count_strings(argv, &str_len);
	required += str_len;
    }

    return required;
}

