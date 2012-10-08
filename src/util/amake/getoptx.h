/*	@(#)getoptx.h	1.3	94/04/07 14:50:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


extern int	opterr;
extern int	optind;
extern int	optopt;
extern char	*optarg;

#ifndef EOF
#define EOF	(-1)
#endif

int getopt	(
#if __STDC__
int argc , char **argv , char *opts
#endif
);
