/*	@(#)bc_string.h	1.1	91/04/11 11:31:34 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/* Strings are allocated in a fixed string descriptor table 
** This mechanism is used to avoid string copying as much as possible
*/

typedef struct{
	char	*strval;
	int	strcount;
	int	strlength;
	} String;

String *_newstr() ;

#define MAXSTRING 1024
