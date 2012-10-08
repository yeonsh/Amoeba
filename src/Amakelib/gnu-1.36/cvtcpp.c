/*	@(#)cvtcpp.c	1.2	94/04/06 10:35:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdio.h>

#define MAX_PATH	256

char dep_arg[2 + MAX_PATH];
char ack_cpp[] = "/usr/local/ambin/cpp";

#define fail1(s1)	{ fprintf(stderr, "%s\n", s1); exit(1); }
#define fail2(s1, s2)	{ fprintf(stderr, "%s%s\n", s1, s2); exit(1); }

void
main(argc, argv)
int argc;
char *argv[];
{
	extern char *strcat(), *strcpy(), *strrchr();
	char *source, *dest, *last_component;

	/* Convert the cpp command, as provided by gcc, to an Ack cpp command
	 * that also writes a dependency list in Amake format.
	 *
	 * A command
	 *     "gnucpp -opt1 .. -optn source.c outfile"
	 * will be converted to
	 *     "ackcpp -opt1 .. -optn -dsource.d source.c"
	 * redirected to outfile.
	 */
	
	/* first perform sanity checks: make sure the command is in the
	 * form we expect it to be.
	 */
	if (argc < 3) {
		fail1("usage: cpp <optional args> source.c outfile");
	}

	source = argv[argc - 2];
	dest = argv[argc - 1];

	/* source must end in ".c" */
	if (strcmp(".c", (source + strlen(source) - 2)) != 0) {
		fail2(source, " doesn't end in \".c\"");
	}

	/* create dependency argument.
	 * if the source doesn't reside in the current directory,
	 * first strip off path leading to it.
	 */
	if ((last_component = strrchr(source, '/')) != 0) {
		last_component++;
	} else {
		last_component = source;
	}
	(void) strcpy(dep_arg, "-d");
	(void) strcat(dep_arg, last_component);
	dep_arg[strlen(dep_arg) - 1] = 'd';	/* replace '.c' by '.d' */
	
	/* change argument list */
	argv[0] = ack_cpp;
	argv[argc - 2] = dep_arg;
	argv[argc - 1] = source;

	/* redirect cpp output to dest */
	if (freopen(dest, "w", stdout) == 0) {
		fail1("couldn't redirect standard output");
	}
	
	/* start real command */
	execv(ack_cpp, argv);

	/* not found */
	fail2("couldn't start ", ack_cpp);
}

