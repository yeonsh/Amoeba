/*	@(#)printf.c	1.1	92/06/25 14:49:32 */
/*
 * printf.c
 *
 * A stripped down version of printf,
 * but still powerful enough for most tasks.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"

void
_exit(n)
    int n;
{
    printf("exit(%d) called; halting system.\n", n);
    halt();
}

/* VARARGS1 */
printf(fmt, args)
    char *fmt;
{
    int *ap = &args;
    char *str, c;
    int lflag;

    for ( ; *fmt != '\0'; fmt++) {
	lflag = 0;
	if (*fmt == '%') {
	    if ((c = *++fmt) == 'l') {
		lflag = 1;
		c = *++fmt;
	    }
	    if (c == 'd' || c == 'u' || c == 'o' || c == 'x') {
		printnum(lflag ? *(long *)ap : (long)*(int *)ap,
		    c == 'o' ? 8 : (c == 'x' ? 16 : 10));
		if (lflag) ap += (sizeof (long) / sizeof (int)) - 1;
	    } else if (c == 'O' || c == 'D' || c == 'X') {
		printnum(*(long *)ap, c == 'O' ? 8 : (c == 'X' ? 16 : 10));
	    	ap += (sizeof (long) / sizeof (int)) - 1;
	    } else if (c == 's') {
		for (str = (char *)*ap; *str; str++)
		    putchar(*str);
	    } else if (c == 'c') {
		putchar(*(char *)ap);
	    }
	    ap++;
	} else {
	    if (*fmt == '\n')
		putchar('\r');
	    putchar(*fmt);
	}
    }
}

static
printnum(num, base)
    long num;
    int base;
{
    long n;

    if ((n = num/base) > 0)
	printnum(n, base);
    putchar("0123456789ABCDEF"[(int)(num % base)]);
}
