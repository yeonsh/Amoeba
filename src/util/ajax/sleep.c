/*	@(#)sleep.c	1.1	91/04/24 17:37:47 */
/* sleep - suspend a process for x sec		Author: Andy Tanenbaum */
#include <stdlib.h>

extern unsigned int sleep();

main(argc, argv)
int argc;
char *argv[];
{
  register unsigned int seconds;
  register char c;

  seconds = 0;

  if (argc != 2) {
	std_err("Usage: sleep time\n");
	exit(1);
  }

  while (c = *(argv[1])++) {
	if (c < '0' || c > '9') {
		std_err("sleep: bad arg\n");
		exit(1);
	}
	seconds = 10 * seconds + (c - '0');
  }

  /* Now sleep. */
  (void) sleep(seconds);
  exit(0);
}
