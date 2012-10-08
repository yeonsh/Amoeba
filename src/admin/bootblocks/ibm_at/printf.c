/*	@(#)printf.c	1.4	94/04/06 11:30:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * printf.c
 *
 * Author:
 *	Leendert van Doorn
 */

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
		    bios_putchar(*str);
	    }
	    ap++;
	} else {
	    if (*fmt == '\n')
		bios_putchar('\r');
	    bios_putchar(*fmt);
	}
    }
}

static
printnum(num, base)
    long num;
    int base;
{
    long n;

    if ((n = num/base) != 0)
	printnum(n, base);
    bios_putchar("0123456789ABCDEF"[(int)(num % base)]);
}
