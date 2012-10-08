/*	@(#)fnmatch.c	1.2	94/04/07 16:06:51 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Written by Guido van Rossum (guido@cwi.nl).
 * NO WARRANTIES!
 */

/*
 * Function fnmatch() as proposed in Posix 1003.2 B.6 (rev. 9).
 * Compares a filename or pathname to a pattern.
 * Extras:
 * - if FNM_QUOTE set in flags, use \ to quote magic characters;
 * - if FNM_HARMONICA set, a pattern segment consisting of ** matches any
 *   number of pathname components (including none, in which case one
 * of the adjacent slashes is omitted).
 */

#include <string.h>
#include "fnmatch.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef STATIC
#define STATIC static
#endif

typedef int bool_t;
#define FALSE 0
#define TRUE 1

#define EOS '\0'

/* Forward declarations */
STATIC char *rangematch();
STATIC bool_t harmonica();

int
fnmatch(patstart, string, flags)
	char *patstart;
	char *string;
	int flags;
{
	register char *pattern = patstart;
	char c, test;
	
	for (;;) {
		switch (c = *pattern++) {
		
		case EOS:
			return *string == EOS;
		
		case '?':
			if ((test = *string++) == EOS ||
					test == '/' && (flags & FNM_PATHNAME))
				return FALSE;
			break;
		
		case '*':
			c = *pattern;
			
#ifdef FNM_HARMONICA
			/* Check for harmonica */
			if ((flags & FNM_HARMONICA) &&
				(flags & FNM_PATHNAME) &&
				c == '*' &&
				(pattern == patstart+1 ||
					pattern >= patstart+2 &&
					pattern[-2] == '/')) {
				if (pattern[1] == EOS)
				    return TRUE; /* ** matches anything */
				if (pattern[1] == '/')
				    return harmonica(pattern+2, string, flags);
			}
#endif /* FNM_HARMONICA */
			
			/* Collapse multiple stars */
			while (c == '*')
				c = *++pattern;
			
			/* Optimize for pattern with * at end or before / */
			if (c == EOS) {
				if (flags & FNM_PATHNAME)
					return !strchr(string, '/');
				else
					return TRUE;
			}
			else if (c == '/' && (flags & FNM_PATHNAME)) {
				if ((string = strchr(string, '/')) == NULL)
					return FALSE;
				break; /* switch; continue loop */
			}
			
			/* General case, use recursion */
			while ((test = *string) != EOS) {
				if (fnmatch(pattern, string, flags))
					return TRUE;
				if (test == '/' && (flags & FNM_PATHNAME))
					break;
				++string;
			}
			return FALSE;
		
		case '[':
			if ((test = *string++) == EOS ||
					test == '/' && (flags & FNM_PATHNAME))
				return FALSE;
			if ((pattern = rangematch(pattern, test)) == NULL)
				return FALSE;
			break;
		
		case '\\':
			if (flags & FNM_QUOTE) {
				if ((c = *pattern++) == EOS) {
					c = '\\';
					--pattern;
				}
				if (c != *string++)
					return FALSE;
				else
					break;
			}
			/* else, fall through */
		default:
			if (c /* Set by switch() above */ != *string++)
				return FALSE;
			break;
		
		}
	}
}

STATIC char *
rangematch(pattern, test)
	char *pattern;
	char test;
{
	char c, c2;
	bool_t ok = FALSE;
	bool_t negate;
	
	if (negate = (*pattern == '!'))
		++pattern;
	
	/* TO DO: quoting */
	
	while ((c = *pattern++) != ']') {
		if (c == EOS)
			return NULL; /* Illegal pattern */
		if (*pattern == '-' && (c2 = pattern[1]) != EOS && c2 != ']') {
			if (c <= test && test <= c2)
				ok = TRUE;
			pattern += 2;
		}
		else if (c == test)
			ok = TRUE;
	}
	
	if (ok != negate)
		return pattern;
	else
		return NULL;
}

#ifdef FNM_HARMONICA

STATIC bool_t
harmonica(patrest, string, flags)
	char *patrest; /* Points to the rest of the pattern, after **/
	char *string;
	int flags;
{
	if (fnmatch(patrest, string, flags))
		return TRUE;
	while ((string = strchr(string, '/')) != NULL) {
		++string;
		if (fnmatch(patrest, string, flags))
			return TRUE;
	}
	return FALSE;
}

#endif
