/*	@(#)clear.c	1.2	94/03/08 17:41:36 */
/* clr - clear the screen		Author: Andy Tanenbaum */

/* This version modified by Rich Heffel */

/* Use termcap for normal, reverse, clear-line. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sgtty.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define reverse()	write(1, SO, (int) strlen(SO))	/* reverse video */
#define normal()	write(1, SE, (int) strlen(SE))	/* undo reverse() */
#define clearln()	write(1,"\r",1); \
			write(1, CD, (int) strlen(CD))	/* clear line */

extern char *tgetstr();

#define  TC_BUFFER  1024	/* Size of termcap(3) buffer	 */

char *SO, *SE, *CD;
char buffer[TC_BUFFER];
char *term;
char clear[30];
char *p = &clear[0];


main()
{

  get_termcap();

  /* Clear the screen  */
  normal();
  clearln();
  printf("%s", clear);

  exit(0);
}

get_termcap()
{
  static char termbuf[50];
  extern char *tgetstr(), *getenv();
  char *loc = termbuf;
  char entry[1024];

  if ((term = getenv("TERM")) == NULL) {
	Error("$TERM not defined");
  }
  if (tgetent(entry, term) <= 0) {
	Error("Unknown terminal.");
  }
  if (tgetent(buffer, term) != 1) {
	Error("No termcap definition for $TERM");
  }
  if ((tgetstr("cl", &p)) == NULL) {
	Error("No clear (cl) entry for $TERM");
  }
  SO = tgetstr("so", &loc);
  SE = tgetstr("se", &loc);
  CD = tgetstr("cd", &loc);

  if (CD == (char *) 0) CD = "             \r";

}

Error(str)
char *str;
{
  fprintf(stderr, "clr: %s\n", str);
  exit(1);
}
