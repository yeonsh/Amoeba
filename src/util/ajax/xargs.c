/*	@(#)xargs.c	1.1	91/11/14 14:23:18 */
/* xargs -- quickie version of System V xargs(1): read a list of
 *	arguments from stdin, apply to the command which is
 *	the argument(s) of xargs
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *cmdname;		/* should point to argv[0] */

char command[BUFSIZ];	/* command given to xargs */
char line[BUFSIZ];	/* current input line */	
char cmdbuf[BUFSIZ];	/* command + input lines */

main(argc, argv)
	int argc;
	char *argv[];
{
	cmdname = argv[0];

	/* skip (xargs) command name */

	argv++, argc--;

	/* construct command from arguments */

	(void) strcpy(command, "exec");
	while (argc--) {
		(void) strcat(command, " ");
		(void) strcat(command, *argv++);
	}

	/* initialize for command execution loop */

	(void) strcpy(cmdbuf, command);

	/* here's where all the action is: read in arguments
	 * from stdin, appending to the current command buffer
	 * if next line would overflow command buffer, execute
	 * command buffer and reinitialize
	 */

	while (gets(line) != NULL) {

		/* the "+2" is for the blank and trailing null char */

		if (strlen(cmdbuf)+strlen(line)+2 > BUFSIZ) {
			(void) docmd(cmdbuf);
			(void) strcpy(cmdbuf, command);
		}
		(void) strcat(cmdbuf, " ");
		(void) strcat(cmdbuf, line);
	}

	/* see if there is any left to do */

	if (strlen(cmdbuf) != strlen(command)) {
		(void) docmd(cmdbuf);
	}
	exit(0);
}

docmd(cmdbuf)
char *cmdbuf;
{
	return system(cmdbuf);
}
