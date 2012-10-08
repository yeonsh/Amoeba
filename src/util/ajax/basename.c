/*	@(#)basename.c	1.1	91/04/24 17:32:02 */
/* basename - print the last part of a path:	Author: Blaine Garfolo */
/* 29/11/89 - gregor@cs.vu.nl modified to use strrchr instead of rindex  */

#include "string.h"
#include "stdlib.h"

#ifndef __STDC__
#define NULL 0
#endif

main(argc,argv)

int argc;
char *argv[];
{
  int j,suflen;
  char *c;
  char *d;

  if (argc < 2) {
	std_err("Usage: basename string [suffix]  \n");
	exit(1);
  }
  c = argv[1];
  d = strrchr(argv[1],'/');
  if (d == NULL) 
	d = argv[1]; 
  else        
	d++;

  if (argc == 2) {       /* if no suffix */
	printf("%s\n",d);
	exit(0);
  } else {		    /* if suffix is present */
	c = d;
	suflen = strlen(argv[2]);
	j = strlen(c) - suflen;
	if (strcmp(c+j, argv[2]) == 0)
		*(c+j) = 0;
  }
  printf("%s\n",c);
  exit(0);
} 
