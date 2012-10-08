/*	@(#)pwd.c	1.1	91/04/24 17:36:58 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	char *getcwd();
	char buf[1026];

	if (getcwd(buf, sizeof buf - 1) == (char *) 0) {
		perror(argv[0]);
		exit(1);
	}
	(void) strcat(buf, "\n");
	(void) write(1, buf, (int) strlen(buf));
	exit(0);
}
