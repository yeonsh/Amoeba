/*	@(#)strtok.c	1.1	91/04/23 08:45:52 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <string.h>
#include "ansi_compat.h"

/*
** strtok - 'string' consists of a sequence of zero or more tokens separated
**	    by spans of one or more characters from the string 'separators'.
**	    Successive calls to strtok return pointers to the start of the
**	    successive tokens in 'string'.
**	    In the first call 'string' must be set, but for subsequent calls
**	    must be the null-pointer.  The place where we are up to in 'string'
**	    is kept in the static variable 'savestring'.  The 'separators'
**	    string may be different each time.
*/

char *
strtok(string, separators)
register char	*string;
const char	*separators;
{
	register char *s1, *s2;
	static char *savestring;

	if (string == NULL) {
		string = savestring;
		if (string == NULL) return (char *)NULL;
	}

	s1 = string + strspn(string, separators);
	if (*s1 == '\0') {
		savestring = NULL;
		return (char *)NULL;
	}

	s2 = strpbrk(s1, separators);
	if (s2 != NULL)
		*s2++ = '\0';
	savestring = s2;
	return s1;
}
