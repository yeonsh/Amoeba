/*	@(#)help.h	1.2	96/03/19 13:03:30 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef _HELP_
# define PUBLIC extern
# else
# define PUBLIC
# endif

VOID	do_help();
/*
 * VOID do_help(cnt);
 * long cnt;			This is ignored, but a count is given
 *				to any command
 *
 * Give a summary of commands
 */

# undef PUBLIC
