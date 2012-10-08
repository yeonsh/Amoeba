/*	@(#)ail.c	1.4	94/04/07 14:31:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"
# include <errno.h>

extern int errno;

/* The "global" variables : */
struct class GlobClass, *ThisClass = &GlobClass, *ImplClass = NULL;
struct typdesc ErrDesc, StrDesc, *cap_typ = NULL;
struct optr *ThisOptr;
struct m_state MarshState;
int MonitorAll;
int NoKeywords = No;
int PostDeact;
int options = 0;

enum en_act Activate;
char *ActFunc, *DeactFunc, *ActType;
char *dfltoutdir = ".";		/* Default output directory	*/
int dfltoutlang = L_DEFAULT;	/* Runtime default language	*/
int dynamic = B_ERROR;
char *FileName = "<stdin>";	/* Last name in #line directive	*/
char *genname = NULL;		/* Current active generator	*/
int lang;		/* Should be set to one of the L_* values	*/
int LineNumber = 0;		/* Current linenumber		*/
char *outdir;			/* "Current" output directory	*/
char *inputlist;		/* Name of inputfiles-list	*/
long n_read = 0;
int n_optrs;
int RetryCount;
int ErrCnt = 0, WarnCnt = 0;	/* Diagnostic counters	*/

int MaxFilenameLen = -1;	/* Guess what			*/

#ifndef AILAMOEBA_H
#define AILAMOEBA_H "ailamoeba.h"
#endif /* AILAMOEBA_H */

char *general_header_file = AILAMOEBA_H;

FILE *cpp;			/* Inputstream		*/
char *strchr();
char getoptstring[] = "bBCD:df:gGi:I:l:no:p:U:v:";
char Usage[] =
"Usage: ail [options] [+code]... [source-file] [+code]...\n\
Options are:\n\
    -b(ypass cpp) -B(ackup) -d(ebug) -f(ilename length) -[gG](enerator-list)\n\
    -i <inputlist> -l(anguage) -n(o output) -o <output-dir> -p(reprocessor)\n\
    -v(ersion).\n\
Options -C, -D, -I and -U are passed to cpp.\n" ;

extern void WriteInFiles();

#ifndef PREPROCESSOR
#define PREPROCESSOR "/lib/cpp"
#endif /* PREPROCESSOR */

static char cpparg[1024] = PREPROCESSOR; /* Default preprocessor, no args yet*/

static void
add_cpp_arg(arg)
char *arg;
{
    if (strlen(cpparg) + strlen(arg) + 1 >= sizeof(cpparg)) {
	mypf(ERR_HANDLE, "ail: arguments to cpp too long\n");
	exit(2);
    }
    strcat(cpparg, arg);
}

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int c;
    extern int optind;		/* For getopt() */
    extern char *optarg;	/* For getopt() */
    int run;
    char *filename = NULL;

    run = Yes;	/* We really wanna do something!	*/
    /*
     *	Parse commandline options:
     */
    while ((c = getopt(argc, argv, getoptstring)) != EOF) switch(c) {
	case 'b':
	    options |= OPT_NOCPP;
	    break;
	case 'B':
	    options |= OPT_BACKUP;
	    break;
	case 'd':
	    options |= OPT_DEBUG;
	    break;
	case 'f':
	    MaxFilenameLen = atoi(optarg);
	    if (MaxFilenameLen < 1) {
		mypf(ERR_HANDLE, "Illegal filenamelength: %s\n", optarg);
		run = No;	/* We don't want to run anymore	*/
	    }
	    break;
	case 'g':
	case 'G':
	    ListGenerators(c);
	    run = 0;
	    break;
	case 'i':
	    options |= OPT_RECORDIN;
	    inputlist = optarg;
	    break;
	case 'l':
	    {
		static int again = 0;
		if (again++) 
		    mypf(ERR_HANDLE, "%r cannot specify language twice\n");
	    }
	    dfltoutlang = GetLang(optarg);
	    break;
	case 'n':
	    options |= OPT_NOOUTPUT;
	    break;
	case 'o':
	    assert(optarg != NULL);
	    dfltoutdir = optarg;
	    break;
	case 'p':
	    if (strchr(cpparg, ' ') != NULL) {
		mypf(ERR_HANDLE, "ail: option -p must come first\n");
		exit(2);
	    }
	    *cpparg = '\0';
	    add_cpp_arg(optarg);	/* Other preprocessor	*/
	    break;
	case '?':
	    mypf(ERR_HANDLE, Usage);
	    exit(1);
	    /*NOTREACHED*/
	case 'C':	/* For cpp -- don't strip comment	*/
	    add_cpp_arg(" -C");
	    break;
	case 'D':	/* For cpp -- define something		*/
	    add_cpp_arg(" -D");
	    add_cpp_arg(optarg);
	    break;
	case 'I':	/* For cpp -- set include-path		*/
	    add_cpp_arg(" -I");
	    add_cpp_arg(optarg);
	    break;
	case '+':	/* Extra input				*/
	    GenerAdd(optarg);
	    break;
	case 'U':	/* For cpp -- undefine a name		*/
	    add_cpp_arg(" -U");
	    add_cpp_arg(optarg);
	    break;
	case 'v':
	    SetVersion(optarg);
	    break;
	default:
	    fatal("I do not comprehend getopt()\n");
    }
    /* Do we still wanna run?	*/
    if (ErrCnt || !run) exit(ErrCnt);
    argc -= optind; argv += optind;

    /*
     *	The next arguments specify the input and look like:
     *		+<text>		- Use <text> as input
     *		-		- Use stdin as input
     *		<filename>	- Use the file as input
     */

    for (;argc-- > 0; ++argv) {
	if (*argv[0] == '+') {
	    GenerAdd(*argv + 1);
	} else {
	    /* Inputfile	*/
	    if (filename != NULL) {
		mypf(ERR_HANDLE, "%r Need exactly one inputfile\n");
	    } else {
		filename = *argv;
		GenerAdd((char *) NULL);
	    }
	}
    }
    if (filename == NULL) {
	mypf(ERR_HANDLE, "%r Need exactly one inputfile\n");
    }
    if (ErrCnt) exit(ErrCnt);

    if (options & OPT_NOCPP) {	/* Open the file yourself	*/
	if (!strcmp(filename, "-")) {
	    cpp = stdin;
	} else {
	    cpp = freopen(filename, "r", stdin);
	    if (cpp == NULL) {
		perror(*argv);
		exit(1);
	    }
	}
    } else {			/* Read via pipe from cpp	*/
	extern FILE *popen ARGS((char *, char *));

	add_cpp_arg(" -Dail=1 ");
	add_cpp_arg(filename);
	/* All preprocessors supported assume that standard output is
	 * meant when the output file is missing.  Some allow '-' as
	 * well, but not all.
	 */
	cpp = popen(cpparg, "r");
	if (cpp == NULL) {
	    perror(cpparg);
	    fatal("Can't start preprocessor\n");
	    /*NOTREACHED*/
	}
    }

    ImplInit();		/* Inits for the code generators	*/
    SymInit();		/* Inits for the symbol table		*/
    AmInit();		/* Declares port, private, capability	*/

    LineNumber = 1;
    yyparse();		/* Read the inputfile.			*/
    LineNumber = 0;
    /* DoToDo(); */

    if (ErrCnt != 0 || WarnCnt != 0) {
	mypf(ERR_HANDLE, "%d %s, %d %s\n",
	    ErrCnt, ErrCnt == 1 ? "error" : "errors",
	    WarnCnt, WarnCnt == 1 ? "warning" : "warnings");
    }
    if (options & OPT_RECORDIN) WriteInFiles(inputlist);

    /*
     *	I used to return the exit status from main,
     *	but this doesn't work on Suns...
     */
    exit(ErrCnt != 0);
    /*NOTREACHED*/	/* To make lint shut up	*/
} /* main */
