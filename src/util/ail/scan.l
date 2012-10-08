/*	@(#)scan.l	1.4	96/02/27 12:43:56 */
%{

/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */


# include "ail.h"
# include <ctype.h>

/*
 *	Two hacks ahead: one for lex, another for flex.
 *	The idea is to provide my own input routine.
 *	Lex never had a documented way of replacing it,
 *	though it can be done at getchar() level.
 *	In flex, it can be done at read()-level.
 *
 *	The standard way is to set the "yyin" file
 *	pointer. This works with flex as well, but
 *	of course, I need something more powerfull,
 *	since the arguments may be scanned as well.
 *
 *	Since there's a lot of renaming involved,
 *	I'd better explain things, to avoid a
 *	"worst abuse of the preprocessor" award
 *	in the obfuscated C contest :-)
 *
 *	The lowest-level get-next-char function is
 *	always done by myinput. There is also a function
 *	myunput() which pushes back one character.
 *	These functions know where to get the characters
 *	from the '+' commandline option.
 *
 *	This is very easy in lex: you only redefine
 *	"input" as "myinput" and "unput" as "myunput".
 *
 *	For flex this is a problem. It declares input()
 *	and unput() by itself, on top of YY_INPUT.
 *	YY_INPUT is a macro with the same parameters
 *	as read(). It also uses read().
 *	Since read() is always allowed to return less
 *	than the requested number of characters, it is
 *	perfectly ok to return 1 character in all cases.
 *	This is what the redefinition of YY_INPUT abuses.
 *
 *	Note that this lousy hack defeats the speedup
 *	that flex claims to have over lex.
 */

#ifdef YY_DO_BEFORE_ACTION
/* Hack for flex */
#define LEX 0
#define FLEX 1
/*
 *	Equivalent with flex' version, depends on read()
 *	being allowed to return less than max_size:
 */
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) \
	result = ((buf[0] = myinput()) == LEX_EOF) ? 0 : 1
#undef yywrap

#else
#define LEX 1
#define FLEX 0
/* Hack for lex */
#undef  input
#define input	myinput
#undef  unput
#define unput	myunput
#endif /* YY_DO_BEFORE_ACTION */

extern FILE *cpp;
extern int errno;

long integer();

# ifdef YYDEBUG
extern int yydebug;
# endif

%}

SPACE		[ \t\014]
STR_ELEM	\\[btn\"\\]
STRING		\"({STR_ELEM}|[^"])*\"
INTEGER		[0-9]+
HEXDIGITS	[0-9a-fA-F]+

%%

\n			{ ++LineNumber; }

^#{SPACE}*(line{SPACE}*)?{INTEGER}{SPACE}*({STRING}{SPACE}*)?({INTEGER})?\n {
			    /* Preprocessor directive. */
			    UpdPos(yytext);
			}

^#.*$			{/* Unrecognized preprocessor directive. */
			    mypf(ERR_HANDLE,
				"%r Illegal line from cpp: '%s'\n", yytext);
			}

"/*"			{ /* Comment */
			int saw_one; /* 2=finish, 1=last-was-*, otherwise 0 */
			int start_com;
			start_com = LineNumber;
			for (saw_one = 0; saw_one<2; ) switch (myinput()) {
			    case '*':
				saw_one = 1;
				break;
			    case '\n':
				saw_one = 0;
				++LineNumber;
				break;
			    case 0:
				saw_one = 0;
				mypf(ERR_HANDLE,
				"%r Comment started on line %d not terminated",
				    start_com);
				exit(1);
				/*NOTREACHED*/
			    case '/':
				if (saw_one) saw_one = 2;
				else saw_one = 0;
				break;
			    default:
				saw_one = 0;
			}
			}

"*/"			{ mypf(ERR_HANDLE, "%r Illegal \"*/\""); }

{SPACE}+		{ /* Discard. */ }

0{INTEGER}		{ yylval.t_int = integer(yytext, 8); return INTCONST; }
0[xX]{HEXDIGITS}	{ yylval.t_int = integer(yytext+2,16);return INTCONST;}
{INTEGER}		{ yylval.t_int = integer(yytext,10); return INTCONST; }
{STRING}		{
			    /* Strip the quotes: */
			    yytext[strlen(yytext)-1] = '\0';
			    yylval.t_ptr = StrFind(yytext + 1);
			    return STRCONST;
			}
\"[^\"\n]+\n		{
			    mypf(ERR_HANDLE, "%r Unclosed string.\n");
			    yylval.t_ptr = "fout";
			    return STRCONST;
			}

[_a-zA-Z][_a-zA-Z0-9]*	{ /* Id, typedef-name or keyword. */
			    int ret;
			    /* Check if it is a keyword. */
			    if (!NoKeywords) {
				ret = KeywLookup(yytext);
				if (ret >= 0) return ret;
			    }
			    yylval.t_ptr = StrFind(yytext);
			    return IDENT;
			}

".."			{ return DOUBLE_DOT; }
"<<"			{ return LEFT; }
">>"			{ return RIGHT; }
"&&"			{ return AND; }
"||"			{ return OR; }
"=="			{ return EQUAL; }
"!="			{ return UNEQU; }
"<="			{ return LEQ; }
">="			{ return GEQ; }

.			{
			    if (isprint(yytext[0])) return yytext[0];
			    mypf(ERR_HANDLE, "%r Unrecognized char %s\n",
				yytext);
			}
%%

/*
 *	Compute a radix-base integer
 */
long
integer(s, base)
    char *s;
    int base;
{
    long ret;
    for (ret = 0; *s != '\0'; ++s) {
	int val;
	if (islower(*s)) val = *s - 'a' + 10;
	else if (isupper(*s)) val = *s - 'A' + 10;
	else val = *s - '0';
	if (val >= base)
	    mypf(ERR_HANDLE, "%r Digit '%c' beyond radix %d\n", *s, (long) base);
    	ret = base*ret + val;
    }
    return ret;
} /* integer */

static char *InFiles[200] = { NULL };
static int n_infiles = 0;

UpdPos(s)
    char *s;
{
    /* The digits MUST be present (yylex assures this), skip trash. */
    do ++s; while (!isdigit(*s));
    /* Find linenumber: */
    LineNumber = 0;
    while (isdigit(*s)) LineNumber = 10*LineNumber + *s++ - '0';
    /* Find (optional) filename. */
    while(*s != '"' && *s != '\0') ++s;
    if (*s == '"') {
	char *cptr;
	++s;		/* Forget first '"'	*/
	for(cptr = s; *cptr != '"'; ++cptr)
	    ;
	*cptr = '\0';	/* overwrite second '"'	*/
	if (*s == '\0') FileName = "<stdin>";
	else FileName = StrFind(s);
	if (options & OPT_RECORDIN) {
	    int i;
	    for (i = n_infiles - 1; i >= 0 && FileName != InFiles[i]; --i)
		;
	    if (i < 0) {
		if (n_infiles >= sizeof(InFiles) / sizeof(InFiles[0]))
		    fatal("Short on infile-entries");
		else InFiles[n_infiles++] = FileName;
	    }
	}
    }
    if (options & OPT_DEBUG) {
	mypf(ERR_HANDLE, "\nFilePosition now: %s %d\n", FileName, LineNumber);
    }
} /* UpdPos */

	/****************************************\
	 *					*
	 *	This is for the '+' option	*
	 *					*
	\****************************************/

static char *cominp[20];	/* Points in the arguments, NULL=cpp	*/
static int a_todo = 0;		/* And how many				*/
static int a_arg = 0;		/* Index in cominp			*/
static int a_offs = 0;		/* Index in cominp[a_arg]		*/
static Bool eo_arg = No;	/* End of argument seen			*/
static Bool n_pending = No;	/* Got a pushed back character?		*/
static char pending_c;		/* Waiting char				*/

myinput()
{
    int ch;
    ++n_read;

    if (n_pending) {		/* Had character pushback		*/
	/* mypf(ERR_HANDLE, "Returning pushed-back\n"); */
	n_pending = No;
	return pending_c;
    }

    if (eo_arg) {		/* Force call to yywrap()		*/
	/* mypf(ERR_HANDLE, "Process end of arg\n"); */
	eo_arg = No;
	return LEX_EOF;
    }

    if (cominp[a_arg] == NULL) {
	/* Reading real input			*/
	ch = getc(cpp);
	if (ch == EOF) ch = LEX_EOF;
    } else {
	/* Reading a commandline argument	*/
	ch = cominp[a_arg][a_offs++];
	/*
	 *	Mimic \n so a '#' on the next file
	 *	shows up at the first position:
	 *	We match ^# for the preprocessor
	 */
	if (ch == '\0') {	/* End of argument	*/
	    /* mypf(ERR_HANDLE, "Mimicing eoln"); */
	    eo_arg = Yes;
	    ch = '\n';
	}
    }
    return ch;
} /* myinput */

void
myunput(ch)
    int ch;
{
    if (ch == LEX_EOF) {
	/* mypf(ERR_HANDLE, "Ignore LEX_EOF pushback\n"); */
	return;	/* I don't know why lex pushes these back */
    }
    /* mypf(ERR_HANDLE, " unput(%c)\n", ch); */
    if (n_pending) fatal("Too many chars pushback (scanner)");
    n_pending = Yes;
    pending_c = ch;
} /* myunput */

yywrap()
{
    int err;
    if (cominp[a_arg] == NULL && !(options & OPT_NOCPP)) {
	static WasHere = No;
	if (WasHere) abort();
	WasHere = Yes;
	/* Check cpp's exit status	*/
	errno = 0;
	if (a_arg <= a_todo && (err = pclose(cpp)) != 0) {
	    mypf(ERR_HANDLE, "%r Preprocessor exit status %d\n", err);
	}
    }
    a_offs = 0;
    /* Are we finished?	*/
    return ++a_arg >= a_todo;
} /* yywrap */

/*
 *	Called by main() to specify the input
 *	Calling this with <NULL> means insert
 *	a preprocessed file.
 */
void
GenerAdd(text)
    char *text;
{
    if (a_todo >= sizeof(cominp)/sizeof(cominp[0]))
	fatal("Too many commandline generators\n");
    cominp[a_todo++] = text;
} /* GenerAdd */

void
WriteInFiles(fn)
    char *fn;
{
    FILE *fp;
    fp = fopen(fn, "w");
    if (fp == NULL) {
	mypf(ERR_HANDLE, "%r cannot open \"%s\"\n", fn);
	return;
    }
    while (n_infiles > 0) fprintf(fp, "%s\n", InFiles[--n_infiles]);
    fclose(fp);
} /* WriteInFiles */
