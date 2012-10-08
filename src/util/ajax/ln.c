/*	@(#)ln.c	1.2	93/10/07 16:07:43 */
/* ln - link a file		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

char name[256];
struct stat stb;

main(argc, argv)
int argc;
char **argv;
{
  char *file1, *file2;
  char *last_comp();

  if (argc < 2 || argc > 3) usage();
  if (access(argv[1], 0) < 0) {
	std_err(argv[0]);
  	std_err(": cannot access ");
  	std_err(argv[1]);
  	std_err("\n");
  	exit(1);
  }
#ifndef _AMOEBA
  if (stat(argv[1], &stb) >= 0 && S_ISDIR(stb.st_mode)) {
	std_err(argv[0]);
	std_err(": cannot link directories\n");
	exit(1);
  }
#endif
  file1 = argv[1];
 
  /* "ln file" means "ln file ." */
  if (argc == 2)
	file2 = ".";
  else
  	file2 = argv[2];
 


  /* Check to see if target is a directory. */
  if (stat(file2, &stb) >= 0 && S_ISDIR(stb.st_mode)) {
  	strcpy(name, file2);
  	strcat(name, "/");
  	strcat(name, last_comp(file1));
  	file2 = name;
  }

  if (link(file1, file2)) {
	std_err("ln: cannot link\n");
	exit(1);
  }
  exit(0);
}

char *last_comp(s)
char *s;
{
/* Return pointer to last component of string. */
  int n;
  n = strlen(s);
  while (n--) 
	if (*(s+n) == '/') return(s+n+1);
  return(s);
}


usage()
{
  std_err("Usage: ln file1 [file2]\n");
  exit(1);
}
