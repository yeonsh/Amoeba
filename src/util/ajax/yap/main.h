/*	@(#)main.h	1.2	96/03/19 13:03:49 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef _MAIN_
# define PUBLIC extern
# else
# define PUBLIC
# endif

PUBLIC int	nopipe;		/* Not reading from pipe? */
PUBLIC char *	progname;	/* Name of this program */
PUBLIC int	interrupt;	/* Interrupt given? */
PUBLIC int	no_tty;		/* output not to a terminal, behave like cat */

int	main();
/*
 * int main(argc,argv)
 * int argc;		Argument count
 * char *argv[];	The arguments
 *
 * Main program.
 */

int	opentemp();
/*
 * int opentemp(i)
 * int i;		Either 0 or 1, indicates which temporary to open
 *
 * Returns a file descriptor for the temporary file, or panics if
 * it couldn't open one.
 */

VOID	catchdel();
/*
 * VOID catchdel(s);
 * int s;
 *
 * interrupt handler.
 */

VOID	quit();
/*
 * VOID quit();
 *
 * Used for exits. It resets the terminal and exits.
 */

VOID	do_quit();
/*
 * VOID do_quit(i)
 * int i;
 *
 * Quit signal handler.
 */

VOID	panic();
/*
 * void panic(str)
 * char *str;		Reason for panic
 *
 * Panic, but at least tell the user why.
 */

# ifdef JOB_CONTROL
VOID	suspend();
/*
 * void suspend()
 *
 * Suspends this process
 */
# endif

# undef PUBLIC
