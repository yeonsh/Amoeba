/*	@(#)kill.c	1.2	96/02/27 12:44:28 */
/* kill - send a signal to a process	Author: Adri Koppes  */

#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>

main(argc,argv)
int argc;
char **argv;
{
  int proc, signal = SIGTERM;
  int sts = 0;

  if (argc < 2) usage();
  if (argc > 1 && *argv[1] == '-') {
	signal = atoi(&argv[1][1]);
	if (!signal)
		usage();
	argv++;
	argc--;
  }
  while(--argc) {
	argv++;
	proc = atoi(*argv);
	if (!proc && strcmp(*argv, "0"))
		usage();
	if (kill(proc, signal)) {
		perror("kill");
		sts = 1;
	}
  }
  exit(sts);
}

usage()
{
  printf("Usage: kill [-sig] pid\n");
  exit(1);
}
