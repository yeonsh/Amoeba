/*	@(#)printenv.c	1.1	91/04/24 17:36:52 */
/* printenv - print the current environment	Author: Richard Todd */
#include <stdlib.h>

main ()
{
  extern char **environ;
  char **sptr;

  for (sptr = environ ; *sptr ; ++sptr) {
	printf("%s\n",*sptr);
  }
  exit(0);
}
