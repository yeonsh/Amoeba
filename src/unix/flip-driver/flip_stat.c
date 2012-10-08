/*	@(#)flip_stat.c	1.3	94/04/07 14:02:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include "ux_ctrl_int.h"

static char buf[MAXPRINTBUF];

main()
{
	int r;
	char *b;

	r = flctrl_stat(buf);
	if(r < 0) {
		fprintf(stderr, "flip_stat failed (%s)\n", err_why(r));
		exit(0);
	}
	b = buf;
        while (--r >= 0)
           putchar(*b++);
        putchar('\n');
}
