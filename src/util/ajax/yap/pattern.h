/*	@(#)pattern.h	1.1	91/04/12 15:02:14 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef _PATTERN_
# define PUBLIC extern
# else
# define PUBLIC
# endif

char *	re_comp();
int	re_exec();

# undef PUBLIC
