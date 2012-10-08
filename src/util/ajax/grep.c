/*	@(#)grep.c	1.3	93/01/15 14:57:01 */
/* grep - search for a pattern 		Author: Martin C. Atkins */
/* 29/11/89 - gregor@cs.vu.nl modified to use strchr instead of index  */
/* 03/06/92 - gregor@cs.vu.nl added -i option */

/*
 *	Search files for a regular expression
 *
 *<-xtx-*>cc -o grep grep.c -lregexp
 */

/*
 *	This program was written by:
 *		Martin C. Atkins,
 *		University of York,
 *		Heslington,
 *		York. Y01 5DD
 *		England
 *	and is released into the public domain, on the condition
 *	that this comment is always included without alteration.
 *
 *	The program was modified by Andy Tanenbaum.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Interface to regular expression handler */

extern int ignoreCase ;
extern char *re_comp();
extern int  re_exec();

#define MAXLINE (1024)
int status = 1;
char *progname;
int pmflag = 1;		/* print lines which match */
int pnmflag = 0;	/* print lines which don't match */
int nflag = 0;		/* number the lines printed */
int iflag;
int args;


main(argc,argv)
int argc;
char *argv[];
{
  char *exp;
  char **argp = &argv[1];

  /* if (!isatty(1)) setbuf(stdout); Removed --Guido */
  args = argc;
  progname = argv[0];
  while(*argp != 0 && argp[0][0] == '-') {
	args--;			/* flags don't count */
	switch(argp[0][1]) {
	case 'v':
		pmflag = 0;
		pnmflag = 1;
		break;
	case 'n':
		nflag++;
		break;
	case 's':
		pmflag = pnmflag = 0;
		break;
	case 'e':
		argp++;
		goto out;
	case 'i':
		ignoreCase = 1;
		break;
	default:
		usage();
	}
	argp++;
  }
out:
  if(*argp == 0) usage();

  if((exp = re_comp(*argp++)) ) {
  	std_err("grep: ");
  	std_err(exp);
  	std_err("\n");
	exit(2);
  }
  if(*argp == 0)
	match((char *)0,exp);
  else
	while(*argp) {
		if(strcmp(*argp,"-") == 0)
			match("-",exp);
		else {
			/* Fix --Guido */
			if(freopen(*argp, "r", stdin) == NULL) {
				std_err("Can't open ");
				std_err(*argp);
				std_err("\n");
				status = 2;
			} else {
				match(*argp,exp);
			}
		}
		argp++;
	}
  exit(status);
}

/*
 *	This routine actually matches the file
 */
match(name, exp)
char *name;
char *exp; /* Can be ignored, remainder from old times */
{
  char buf[MAXLINE];
  int lineno = 0;
  int line_length;

  while((line_length=getline(buf,MAXLINE)) != 0) {
	lineno++;
	if(line_length==MAXLINE && buf[MAXLINE-1]!='\n' ) {
		std_err("Line too long in ");
		std_err(name == 0 ? "stdin":name);
	}
	buf[line_length-1]=0 ;
	if(re_exec(buf)) {
		if(pmflag)
			pline(name,lineno,buf);
		if(status != 2)
			status = 0;
	} else if(pnmflag)
		pline(name,lineno,buf);
  }
}

void regerror(s)
char *s;
{
  std_err("grep: ");
  std_err(s);
  std_err("\n");
  exit(2);

}

pline(name, lineno, buf)
char *name;
int lineno;
char buf[];
{
  if(name && args > 3) printf("%s:",name);
  if(nflag) printf("%d:", lineno);
  printf("%s\n",buf);
}


usage()
{
  std_err("Usage: grep [-insv] [-e] pattern [file ...]\n");
  exit(2);
}

int getline(buf, size)
char *buf;
int size;
{
  char *initbuf = buf;
  int c;

  while (1) {
	c = getc(stdin);
  	*buf++ = c;
	if (c <= 0) return(0);
  	if (buf - initbuf == size - 1) return(buf - initbuf);
  	if (c == '\n') return(buf - initbuf);
  }
}
