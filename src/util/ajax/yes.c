/*	@(#)yes.c	1.2	94/04/07 14:40:14 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: yes.c
 *
 * Original	: Jan 15, 1991.
 * Author(s)	: Berend Jan Beugel (beugel@cs.vu.nl).
 * Description	: Writes a string to stdout until interrupted ("y" by default).
 *
 * Update 1	:
 * Author(s)	:
 * Description	:
 */


#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#define TRUE		1
#define FALSE		0
#define	ARGS		"n"
#define	MIN(a,b)	((a) < (b) ? (a) : (b))

typedef	int		bool;


/* Print a message about how to use the program. */
static	int
usage(progname, args)
char	progname[], args[];
{
  fprintf(stderr, "Usage: %s [-%s] [string]\n", progname, args);
  exit(1);
}



int
main(argc, argv)
int	argc;
char	*argv[];
{
  char		c, s[257];
  bool		newline = TRUE;
  int		slen;
  extern int    optind;


  /* Check the command-line  options. */
  while((c = getopt(argc, argv, ARGS)) != EOF) {
    if (c == 'n')
      newline = FALSE;
    else
      usage(argv[0], ARGS);
  }

  /* Decide which string to use. */
  if (optind == argc)
    s[0] = 'y';
  else if (optind+1 == argc)
    (void)strncpy(s, argv[optind], 255);
  else
    usage(argv[0], ARGS);

  slen = strlen(s);
  slen = MIN(255, slen);
  if (newline)
    s[slen++] = '\n';

  s[slen] = '\0';

  while(fputs(s, stdout) != EOF && fflush(stdout) != EOF)
	/* do nothing */;
  exit(0);
}  /* main() */

