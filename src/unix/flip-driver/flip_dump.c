/*	@(#)flip_dump.c	1.2	94/04/07 14:02:06 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include "ux_ctrl_int.h"

static char buf[MAXPRINTBUF];

main()
{
	int r;
	char *b;

	r = flctrl_dump(buf);
	if(r < 0) {
		fprintf(stderr, "flip_dump failed\n");
		exit(0);
	}
	b = buf;
        while (--r >= 0)
           putchar(*b++);
        putchar('\n');

}
