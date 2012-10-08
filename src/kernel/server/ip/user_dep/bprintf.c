/*	@(#)bprintf.c	1.2	92/05/14 10:28:46 */
/*
bprintf.c
*/

#define port am_port_t
#include <amoeba.h>
#include <_ARGS.h>
#undef port

#include "amoeba_incl.h"
#include "va_dcl.h"

#define printf kernel_printf
#define put_char	kernel_putchar
#define printnum	kernel_printnum
#define doprnt		kernel_doprnt

static void printf _ARGS(( char *format, ... ));
static char *doprnt _ARGS(( char *begin, char *end, char *fmt, VA_LIST argp ));
static char *put_char _ARGS(( char *p, char *end, int c ));
static char *printnum _ARGS(( char *begin, char *end, long n, int base, 
	int sign, int width, int pad ));

#define MAXWIDTH	32

char hexdec[] = "0123456789ABCDEF";

/*
** the following is a table of the characters which should be printd
** when a particular value for a character is requested to be printed
** by printchar.
*/
#define	BEL		0x07	/* bell */
#define	BS		0x08	/* back space */
#define	NL		0x0a	/* new line */
#define	CR		0x0d	/* carriage return */

static char	displayable[256] =
{
'?', '?', '?', '?', '?', '?', '?', BEL, BS , '?', NL , '?', '?', CR , '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?'
};


/*
** PRINTF
**	Format arguments into an ascii string and print the result on the
**	console.
*/

/*VARARGS1*/
#if __STDC__
static void printf(char *fmt, ...)
#else
static void printf(fmt VA_ALIST)
char *	fmt;
VA_DCL
#endif
{
    VA_LIST args;

    VA_START( args, fmt );

    (void) doprnt((char *) 0, (char *) 0, fmt, args);

    VA_END( args );
}


/*
** BPRINTF
**	This is rather like sprintf except that it takes pointers to the
**	beginning and end of the buffer where it must put the formatted
**	string.  It returns when either the buffer is full or when the
**	format string is copied.  The string in the buffer is not null
**	terminated!
**	It returns a pointer to the end of the formatted string in the buffer
**	If either of the buffer pointers are null it writes on standard output.
**	So if your output unexpectedly appears on the console look for a null
**	pointer.
*/

/*VARARGS3*/
#if __STDC__
char *bprintf(char *begin, char *end, char *fmt, ...)
#else
char *
bprintf(begin, end, fmt VA_ALIST)
char *	begin;
char *	end;
char *	fmt;
VA_DCL
#endif
{
    char *rc;
    VA_LIST args;

    VA_START( args, fmt );

    rc = doprnt(begin, end, fmt, args);

    /* If safe, null terminate */
    if (rc && rc < end)
	*rc = '\0';

    VA_END( args );
    return( rc );
}


/*
** PUT_CHAR
**	Put a character in the specified buffer and return a pointer to the
**	next free place in the buffer.
**	However:
**		If the buffer is full, change nothing.
**		If there, is no buffer, write it on the console.
*/

static char *
put_char(p, end, c)
char *	p;
char *	end;
char	c;
{
#if 0
    if (p == 0)
    {
	putchr(c);
	return 0;
    }
#endif
    if (p >= end)
	return p;
    *p++ = c;
    return p;
}


/*
** PRINTNUM
**	Format a number in a the chosen base
*/


static char *
printnum(begin, end, n, base, sign, width, pad)
char *	begin;
char *	end;
long	n;
int	base;
int	sign;
int	width;
int	pad;
{
    register short	i;
    register short	mod;
    char		a[MAXWIDTH];
    register char *	p = a;

    if (sign)
	if (n < 0)
	{
		n = -n;
		width--;
	}
	else sign = 0;

    do
    {		/* mod = n % base; n /= base */
	mod = 0;
	for (i = 0; i < 32; i++) {
		mod <<= 1;
		if (n < 0) mod++;
		n <<= 1;
		if (mod >= base) {
			mod -= base;
			n++;
		}
	}
	*p++ = hexdec[mod];
	width--;
    } while (n);

    while (width-- > 0)
	begin = put_char(begin, end, pad);

    if (sign)
	*p++ = '-';

    while (p > a)
	begin = put_char(begin, end, *--p);
    return begin;
}


/*
** DOPRNT
**	This routine converts variables to printable ascii strings for printf
**	and bprintf.  The first two parameters point to the beginning and end
**	(respectively) of a buffer to hold the result of the formatting.
**	If either of the buffer pointers is null the output is directed to the
**	system console.  If the buffer is there it will stop formatting at the
**	end of the buffer and return
*/

/*VARARGS3*/
static char *
doprnt(begin, end, fmt, argp)
char *			begin;	/* pointer to buffer where to put output */
char *			end;	/* end of buffer to receive output */
register char *		fmt;	/* format of output */
VA_LIST			argp;	/* arguments to format string */
{
    register char *	s;
    register char	c;
    register short	width;
    register short	pad;
    register short	n;

/*
** Make sure that we don't have a defective buffer pointer!  We write on the
** console if begin is the null-pointer.
*/
    if (end == 0)
	begin = 0;

/*
** Process the format string, doing conversions where required
** note: we ignore the left justify flag.  Numbers are always right justified
** and everything else is left justified.
*/
    for ( ; *fmt != 0; fmt++)
	if (*fmt == '%')
	{
	    if (*++fmt == '-')
		fmt++;
	    pad = *fmt == '0' ? '0' : ' ';
	    width = 0;
	    while (isdigit(*fmt))
	    {
		width *= 10;
		width += *fmt++ - '0';
	    }
	    if (*fmt == 'l' && islower(*++fmt))
		c = toupper(*fmt);
	    else
		c = *fmt;
	    switch (c)
	    {
	    case 'c':
	    case 'C':
		begin = put_char(begin, end,
				     displayable[ VA_ARG( argp, int )]);
		width--;
		break;
	    case 'b':
		begin = printnum(begin, end, (long) VA_ARG( argp, unsigned ),
				 2, 0, width, pad);
		width = 0;
		break;
	    case 'B':
		begin = printnum(begin, end, VA_ARG( argp, long ),
				 2, 0, width, pad);
		width = 0;
		break;
	    case 'o':
		begin = printnum(begin, end, (long) VA_ARG( argp, unsigned ),
				 8, 0, width, pad);
		width = 0;
		break;
	    case 'O':
		begin = printnum(begin, end, VA_ARG( argp, long ),
				 8, 0, width, pad);
		width = 0;
		break;
	    case 'd':
		begin = printnum(begin, end, (long) VA_ARG( argp, int ),
				 10, 1, width, pad);
		width = 0;
		break;
	    case 'D':
		begin = printnum(begin, end, VA_ARG( argp, long ),
				 10, 1, width, pad);
		width = 0;
		break;
	    case 'u':
		begin = printnum(begin, end, (long) VA_ARG( argp, unsigned ),
				 10, 0, width, pad);
		width = 0;
		break;
	    case 'U':
		begin = printnum(begin, end, VA_ARG( argp, long ),
				 10, 0, width, pad);
		width = 0;
		break;
	    case 'x':
		begin = printnum(begin, end, (long) VA_ARG( argp, unsigned ),
				 16, 0, width, pad);
		width = 0;
		break;
	    case 'X':
		begin = printnum(begin, end, VA_ARG( argp, long ),
				 16, 0, width, pad);
		width = 0;
		break;
	    case 'p':
	    case 'P':
		s = (char *) VA_ARG( argp, am_port_t **);
		n = PORTSIZE;
		do {
		    if (*s > ' ')
			begin = put_char(begin, end, displayable[*s]);
		    else
			begin = put_char(begin, end, '?');
		    width--;
		    s++;
		} while (--n);
		break;
			    
	    case 's':
	    case 'S': 
		s = VA_ARG( argp, char *);
		while (*s)
		{
		    begin = put_char(begin, end, displayable[*s++]);
		    width--;
		}
		break;
	    }
	    while (width-- > 0)
		begin = put_char(begin, end, pad);
	}
	else
	    begin = put_char(begin, end, *fmt);

    return begin;
}
