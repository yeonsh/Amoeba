/*	@(#)mktemp.c	1.2	94/04/07 09:49:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* mktemp - make a name for a temporary file */

char *mktemp(template)
char *template;
{
  register int pid, k;
  register char *p;

  pid = getpid();		/* get process id as semi-unique number */
  p = template;
  while (*p) p++;		/* find end of string */

  /* Replace XXXXXX at end of template with pid. */
  while (*--p == 'X') {
	*p = '0' + (pid % 10);
	pid /= 10;
  }
  p++;
  if ( *p == '\0' ) {
	/* Hmm. There were no X-en at all. Return original string */
	return template;
  }
  for (k = 'a'; k <= 'z'; k++) {
	*p = k;
	if (access(template, 0) < 0) {
		return template;
	}
  }
  return("/");
}
