/*	@(#)options.h	1.2	96/03/19 13:04:04 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef _OPTIONS_
# define PUBLIC extern
# else
# define PUBLIC
# endif

PUBLIC int	cflag;		/* no home before each page */
PUBLIC int	uflag;		/* no underlining */
PUBLIC int	nflag;		/* no pattern matching on input */
PUBLIC int	qflag;		/* no exit on the next page command */
PUBLIC int	hflag;		/* display backspaces if no pattern
				   is recognized
				*/
PUBLIC char *	startcomm;	/* There was a command option */

char ** readoptions();
/*
 * char ** readoptions(argv)
 * char **argv;			Arguments given to yap.
 *
 * process the options from the arguments. Return 0 if there was an error,
 * otherwise return a pointer to where the filenames start.
 */

# undef PUBLIC
