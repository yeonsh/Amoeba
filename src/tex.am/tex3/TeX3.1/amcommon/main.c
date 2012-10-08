/*	@(#)main.c	1.1	91/11/21 13:44:59 */
/* main.c -- the usual main program.  */

#include "extra.h"


/* The command line is stored here, since Pascal has usurped `argv' 
   for a procedure.  */
static char **gargv;

/* Referenced from the Pascal, so not static.  */
integer argc;


/* The entry point for all the programs except TeX and Metafont, which
   have more to do.  We just have to set up the command line.  Pascal's
   main block is transformed into the procedure `main_body'.  */

void
main (ac, av)
  int ac;
  char **av;
{
  argc = ac;
  gargv = av;
  main_body ();
  exit (0);
}


/* Read the Nth argument from the command line.  BUF is a Pascal
   string, i.e., it starts at index 1 and ends with a space.  */

void
argv (n, buf)
  int n;
  char buf[];
{
  (void) strcpy (buf + 1, gargv[n]);
  (void) strcat (buf + 1, " ");
}

