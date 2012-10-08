/*	@(#)cgrep.c	1.3	96/03/01 16:52:24 */
/* cgrep - grep and display context	Author: Mark Mallet */

/*
        Nov 19 1984     Mark Mallett   (mem@zinn.MV.COM)

mem	860224	Modified to do r/e (regular expression) parsing on unix
mem	860324	Added -f, -n; added code to number lines correctly on output.
mem	870325	Added support for regcmp()/regex() style regular expression
  library; redid some conditionals to provide better mix'n'match.
mem	870326	Don't try to print the filename if reading from stdin.
  Add -w option.  Fix a small problem which occasionally allowed
  the separator to come out between adjacent lines of the file.
mem	871119	Fix semantics of call to regcmp(): the NULL terminating the
  argument list was missing.  It worked, but probably only
  due to some bizarre coincidence.
dro	890109  Minor mods to compile under Minix

gregor  910411  Ansi-fied a little
ed	910416	Added beginning-of-line flag to call of regexec

*/

#define  OS_UNIX		/* Define this for unix systems */
#define	 REGEX 			/* Define this for re_comp/re_exec library */
/* #define  REGCMP			/* Define this to use regcmp/regex */
 /* #define	OS_CPM *//* Define this for CP/M-80 */


/* Don't touch these */
#define	NOREGEXP		/* Set this for no regular expression */
#ifdef	REGEX
#undef	NOREGEXP
#endif	/* REGEX */

#ifdef	REGCMP
#undef	NOREGEXP
#endif	/* REGCMP */


#ifdef OS_CPM
#include "stdio.h"
#include "ctype.h"
#endif /* OS_CPM */

#ifdef OS_UNIX
#include <sys/types.h>
#include <dirent.h>		/* Either here or in sys directory - dro */
#include <ctype.h>
#include <limits.h>		/* should have this                - dro */
#ifdef REGCMP
#include <regexp.h>		/* should have this                - dro */
#endif /* REGCMP */
#include <stdio.h>
#include <stdlib.h>
#endif /* OS_UNIX */

/* Local definitions */

#ifndef	NULL
#define	NULL	((char *)0)
#endif	/* NULL */

#ifndef NUL
#define NUL     '\000'
#endif

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

/* External routines */
#ifdef	REGEX
extern char 	*re_comp();
extern		re_exec();
#endif

/* Internal data declared global */


/* Internal routines */
void dosrch _ARGS((char *));


/* External data */


/* Local data */

static int Debug = {FALSE};	/* Debug enabled flag */
static int Lcur = {0};		/* Current line (in Lines array) */
static char **Lines = {(char **) NULL};	/* Lines pointer array */
static int Linlen = {100};	/* Line length */
static int Lone = {0};		/* Line one (in Lines array) */
static int Nmr = {0};		/* Number of matched regions */
static char *Pat = {NULL};	/* Pattern */
static char Shwfile = {TRUE};	/* Show file name... */
static char Shwline = {TRUE};	/* Show line number */
static int Waft = {0};		/* Window after */
static int Wbef = {0};		/* Window before */
static int Wsiz = {0};		/* Window size */

#ifdef	REGEX
char 	*Re;
#else
regexp	*Re;			/* Result from reg compilation */
#endif

void
usage(prg)
char * prg;
{
    fprintf(stderr,
"Usage: %s [-a n] [-b n] [-f] [-l n] [-n] [-w n] pattern [filename... ]\n",
prg);
}


main(argc, argv)
int argc;			/* Argument count */
char **argv;			/* Argument values */

{
  int i;			/* Scratch */
  int n;			/* Scratch again */
  int c;			/* A character */
  char *aptr;			/* Argument pointer */
  int nf;			/* number of files on command line */

  nf = 0;			/* No files on line */

  for (i = 1; i < argc; i++) {	/* Look at args */
	if (argv[i][0] != '-') {/* If option */
		if (Pat == NULL) {	/* If no pattern yet given */
			Pat = argv[i];	/* point here */
#ifdef	REGEX
			if ((Re = re_comp(Pat)) != NULL) {
				fprintf(stderr, "cgrep: %s\n", Re);
				exit(1);
			}
#endif	/* REGEX */

#ifdef	REGCMP
			if ((Re = regcomp(Pat)) == NULL) {
				fprintf(stderr, "cgrep: error in regular expression.\n");
				exit(1);
			}
#endif	/* REGCMP */

		} else {	/* This must be a file to search */
			nf++;	/* Count it */
			dosrch(argv[i]);	/* Search */
		}
	} else {		/* Option char */
		c = argv[i][1];	/* Get option char */
		if (isupper(c))	/* Trap idiot definition of tolower */
			c = tolower(c);	/* Don't care about case */
		n = i;
		aptr = NULL;	/* Find arg, if any */
		if (argv[i][2] != NUL) {
			aptr = &argv[i][2];
			n = i;	/* Where to set i if we use this arg */
		} else if (i < argc - 1) {	/* use next.. */
			n = i + 1;
			aptr = argv[n];
		}
		switch (c) {	/* Process the option */
		    case 'a':	/* Lines after */
			Waft = atoi(aptr);
			Lines = NULL;
			i = n;
			break;

		    case 'b':	/* Lines before */
			Wbef = atoi(aptr);
			Lines = (char **) NULL;
			i = n;
			break;

/* Disable debug output
                    case 'd':	Debug = TRUE;	                 break;
*/

		    case 'f':	/* Suppress filename on output */
			Shwfile = FALSE;
			break;

		    case 'l':	/* Line length */
			Linlen = atoi(aptr);
			Lines = NULL;
			i = n;
			break;

		    case 'n':	/* Suppress line number on output */
			Shwline = FALSE;
			break;

		    case 'w':	/* Window: lines before and after */
			Waft = Wbef = atoi(aptr);
			Lines = NULL;
			i = n;
			break;

		    default:
			fprintf(stderr, "%s: Invalid option: %s\n", argv[0], argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}
  }

  if (Pat == NULL) {		/* If no pattern given */
	usage(argv[0]);
	exit(1);
  }
  if (nf == 0)			/* No files processed ? */
	dosrch((char *) 0);		/* Do standard input */
  exit(0);
}


 /* Dosrch (ifnm) Perform the search 
  * Accepts :
  * 
  * ifn             Input file name
  * 
  * 
  * Returns :
  * 
  * 
  */

void
dosrch(ifnm)
char *ifnm;			/* Input filelname */

{
  FILE *ifp;			/* Input fp */
  char *lptr;			/* Line pointer */
  int i;			/* Scratch */
  int prtaft;			/* Print-after count */
  int linnum;			/* Line number */
  int nlb;			/* Number of lines buffered */

  if (ifnm != NULL) {		/* If file name given */
	ifp = fopen(ifnm, "r");	/* Open it for read access */
	if (ifp == NULL) {
		fprintf(stderr, "Can not open file %s\n", ifnm);
		return;
	}
  } else
	ifp = stdin;

  if (Lines == NULL) {		/* If no line table allocated.. */
	Wsiz = Wbef + 2;	/* Determine total window size */
	Lines = (char **) calloc((size_t) Wsiz, sizeof(char *));
	/* Allocate pointer table */
	for (i = 0; i < Wsiz; i++)	/* Allocate line buffers */
		Lines[i] = (char *) calloc((size_t) Linlen, sizeof(char));
  }
  Lcur = Lone = 0;		/* Setup line pointers */
  nlb = 0;			/* No lines buffered */
  linnum = 0;			/* Line number is zero */
  prtaft = -(Wbef + 1);		/* Make sure separator given first time */

  for (;;) {			/* Loop through the file */
	lptr = Lines[Lcur];	/* Get pointer to current line */
	if (++Lcur == Wsiz)	/* Bump curr pointer and wrap */
		Lcur = 0;	/* if hit end */
	if (Lone == Lcur)	/* If wrapped to beginning of window */
		if (++Lone == Wsiz)	/* Bump beginning */
			Lone = 0;	/* and wrap if hit end */

	if (fgets(lptr, Linlen, ifp) != lptr) break;	/* if end of file */

	linnum++;		/* Count line number */
	if (matlin(lptr)) {	/* If matching line */
		if (prtaft < (-Wbef))	/* Check for separator needed */
			if ((Nmr++ > 0) && ((Wbef > 0) || (Waft > 0)))
				printf("----------------------------------------------------------------------------\n");
		while (Lone != Lcur) {	/* Until we close the window */
			shwlin(ifnm, linnum - nlb, Lines[Lone]);
			/* Show the line */
			if (++Lone == Wsiz) Lone = 0;
			nlb--;
		}
		nlb = 0;	/* No lines buffered */
		prtaft = Waft;	/* Print n lines after */
	} else {		/* Didn't match */
		if (prtaft-- > 0) {	/* If must print lines after */
			shwlin(ifnm, linnum, lptr);
			/* Show the line */
			Lone = Lcur;	/* Match pointers */
		} else if (nlb < Wbef)	/* Count lines buffered */
			nlb++;
	}
  }

  if (ifnm != NULL) fclose(ifp);
}

 /* Shwlin (fnm, linnum, line) Show a matching line 
  * 
  * Accepts :
  * 
  * fnm             File name
  * 
  * linnum          Line number
  * 
  * line            Line to show
  * 
  * 
  * Returns :
  * 
  * 
  */

shwlin(fnm, linnum, line)
char *fnm;			/* File name */
int linnum;			/* Line number */
char *line;			/* Line (with newline at end) to print */

{
  if (Shwfile && (fnm != NULL)) printf("%s%s", fnm, Shwline ? " " : ":");
  if (Shwline) printf("@%05d%:", linnum);
  printf("%s", line);
}

 /* Matlin (line) Perform match against pattern and line 
  * 
  * Accepts :
  * 
  * line            Address of line to match
  * 
  * 
  * Returns :
  * 
  * <value>         TRUE if match FALSE if not
  * 
  * 
  */


int matlin(line)
char *line;			/* Line to match */

{
  int rtncode;			/* Return value from this routine */


#ifdef	NOREGEXP
  char *pptr, *lptr, *tlptr;
  int c1, c2;
#endif	/* NOREGEXP */

  if (Debug) printf("Matching %s against %s", Pat, line);

#ifdef	REGEX
  rtncode = re_exec(line);	/* Hand off to r/e evaluator */
#endif	/* REGEX */

#ifdef	REGCMP
  rtncode = (regexec(Re, line, 1) != 0);
#endif	/* REGCMP */

#ifdef	NOREGEX			/* Have to do menial comparison.. */
  lptr = line;			/* Init line pointer */

  for (rtncode = -1; rtncode < 0;) {
	tlptr = lptr++;		/* Get temp ptr to line */
	pptr = Pat;		/* Get ptr to pattern */
	while (TRUE) {
		if ((c1 = *pptr++) == NUL) {
			rtncode = 1;	/* GOOD return value */
			break;
		}
		if ((c2 = *tlptr++) == NUL) {
			rtncode = 0;	/* BAD return value */
			break;
		}
		if (isupper(c1)) c1 = tolower(c1);
		if (isupper(c2)) c2 = tolower(c2);
		if (c1 != c2) break;
	}
  }
#endif	/* NOREGEX */


  if (Debug) printf("matlin returned %s\n", rtncode ? "TRUE" : "FALSE");
  return(rtncode);
}


#ifdef REGCMP

void regerror(s)
{
  printf("%s\n", s);
  exit(1);
}

#endif /* REGCMP */
