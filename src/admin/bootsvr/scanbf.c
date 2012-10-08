/*	@(#)scanbf.c	1.3	94/04/06 11:40:44 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ailamoeba.h"
#include "svr.h"
#include "parsbf.h"

/*
 *	Scanner for the boot-configuration file
 *	Returns:
 *	 - NUMBER (hexadecinal or decimal)
 *	 - STRING (C-style)
 *	 - CAPABILITY (ar-format)
 *	 - IDENT (C-style)
 *	 - Punctuation like '{', ';'
 *	 - a keyword
 */

#define ORDERED(a,b,c)	((a) <= (b) && (b) <= (c))
#define MYEOF		0

char yytext[200];
int LineNo, LookAhead;
char *strdup();


	/*** Keywords & Identifiers ***/

static struct {
    char *name;
    int token;
} keywords[] = {	/* In ascii order: */
	"after",	AFTER,
	"argv",		ARGV,
	"bootrate",	BOOTRATE,
	"cap",		CAP,
	"capv",		CAPV,
	"down",		DOWN,
	"environ",	ENVIRON,
	"machine",	MACHINE,
	"pollcap",	POLLCAP,
	"pollrate",	POLLRATE,
	"procsvr",	PROCSVR,
	"program",	PROGRAM,
	"stack",	STACK,
};

static
KeywOrIdent()	/* Or capability litteral */
{
    char *p;
    int lo, hi;
    p = yytext;
    while (
	ORDERED('0', LookAhead, '9') ||
	ORDERED('a', LookAhead, 'z') ||
	ORDERED('A', LookAhead, 'Z') ||
	LookAhead == '_'
    ) {
	if (p - yytext > sizeof(yytext) - 2)
	    MyError(-1, -1, "buffer overflow");
	else *p++ = LookAhead;
	LookAhead = f_get();
    }
    if (LookAhead == ':') {	/* Capability litteral */
	char *ar_tocap();
	do {
	    *p++ = LookAhead;
	    LookAhead = f_get();
	} while (
	    ORDERED('0', LookAhead, '9') ||
	    ORDERED('a', LookAhead, 'f') ||
	    ORDERED('A', LookAhead, 'F') ||
	    LookAhead == '(' || LookAhead == ')' ||
	    LookAhead == ':' || LookAhead == '/'
	);
	*p = '\0';
	if (ar_tocap(yytext, &yylval.t_cap) != p) {
	    MyError(-1, -1, "illegal capability litteral");
	}
	return CAPABILITY;
    }
    *p = '\0';
    lo = 0; hi = SIZEOFTABLE(keywords);
    do {	/* Binary search in keywords table */
	int x, mi;
	mi = (lo + hi) / 2;
	if ((x = strcmp(yytext, keywords[mi].name)) == 0)
	    return keywords[mi].token;
	if (x < 0) hi = mi;
	else lo = mi + 1;
    } while (lo < hi);
    if (p - yytext >= sizeof(ident)) {
	MyError(-1, -1, "identifier too long");
	yytext[sizeof(ident) - 1] = '\0';
    }
    yylval.t_str = strdup(yytext);
    return IDENT;
} /* KeywOrIdent */


	/*** Strings ***/

static int
OctalEscape()
{
    int n, i;
    n = LookAhead - '0';
    for (i = 0; i < 2; ++i) {
	LookAhead = f_get();
	if (ORDERED('0', LookAhead, '7')) n = 8 * n + LookAhead - '0';
	else break;
    }
    return n;
} /* OctalEscape */

static
GetEscape()
{
    LookAhead = f_get();
    if (ORDERED('0', LookAhead, '7')) return OctalEscape();
    else switch (f_get()) {
    case '\\':
	return '\\';
    case 'b':
	return '\b';
    case 'f':
	return '\f';
    case 'n':
	return '\n';
    case 'r':
	return '\r';
    case 't':
	return '\t';
    case MYEOF:
	MyError(-1, -1, "unterminated straing at eof");
	return MYEOF;
    default:
	MyError(-1, -1, "unknown escape");
    }
    return LookAhead;
} /* GetEscape */

static
GetString()
{
    char *p;
    int done = 0, ok = 1;
    p = yytext;
    do {
	LookAhead = f_get();
	switch (LookAhead) {
	case '"':
	    done = 1;
	    *p = '\0';
	    LookAhead = f_get();
	    break;
	case MYEOF:
	    MyError(-1, -1, "Unclosed string at eof");
	    ok = 0;
	    break;
	case '\n':
	    MyError(-1, -1, "Unclosed string at eoln");
	    ok = 0;
	    break;
	case '\\':
	    if ((*p++ = GetEscape()) == MYEOF) done = 1;
	    break;
	default:
	    *p++ = LookAhead;
	}
	/* Will null byte fit? */
	if (!done && p - yytext > sizeof(yytext) - 1) {
	    ok = 0;
	    MyError(-1, -1, "buffer overflow");
	    while (LookAhead != MYEOF && LookAhead != '\n' && LookAhead != '"')
		LookAhead = f_get();
	}
    } while (!done && ok);
    yylval.t_str = strdup(yytext);
    return STRING;
} /* GetString */


	/*** Numbers, capabilities ***/

static int
hexval(ch)
    char ch;
{
    if (ORDERED('0', ch, '9')) return ch - '0';
    if (ORDERED('a', ch, 'f')) return ch - 'a' + 10;
    return ch - 'A' + 10;
} /* hexval */

static long
HexToNum(s)
    char *s;
{
    long l = 0;
    for (s += 2; *s; ++s) l = 16 * l + hexval(*s);
    return l;
} /* HexToNum */

/*
 *	Recognize (hexa)decimal numbers or capabilities in ar-format
 *	We simply eat all digits, and any of ":()/xXa-fA-F".
 */
static
Numberish()
{
    int saw_colon = 0, saw_letter = 0, stop = 0;
    char *p = yytext;
    while (!stop) {
	if (p - yytext >= sizeof(yytext)) MyError(-1, -1, "buffer overflow");
	*p++ = LookAhead;
	LookAhead = f_get();

	if (ORDERED('a', LookAhead, 'f')
	|| ORDERED('A', LookAhead, 'F')
	|| LookAhead == 'x' || LookAhead == 'X')
	    saw_letter = 1;
	else if (LookAhead == '(' || LookAhead == ')'
	|| LookAhead == '/' || LookAhead == ':')
	    saw_colon = 1;
	else if (!ORDERED('0', LookAhead, '9'))
	    stop = 1;
    }
    *p = '\0';
    if (saw_colon) {
	char *ar_tocap(), *s;
	s = ar_tocap(yytext, &yylval.t_cap);
	if (s == NULL || *s != '\0') {
	    MyError(-1, -1, "illegal capability litteral");
	}
	return CAPABILITY;
    }
    if (saw_letter) yylval.t_num = HexToNum(yytext + 2);
    else yylval.t_num = atol(yytext);
    return NUMBER;
} /* Numberish */


	/*** Blankstripping, line counting ***/

static
SkipComment()
{
    do LookAhead = f_get();
    while (LookAhead != '\n' && LookAhead != MYEOF);
} /* SkipComment */

static
BlankStrip()
{
    int stop = 0;
    do {
	if (LookAhead == '#') {
	    SkipComment();
	} else if (LookAhead == '\n') {
	    ++LineNo;
	    LookAhead = f_get();
	} else if (LookAhead != ' ' && LookAhead != '\t') {
	    stop = 1;
	} else {
	    LookAhead = f_get();
	}
    } while (!stop);
} /* BlankStrip */


	/*** The scanner ***/

int
yylex()
{
    int tok;
    BlankStrip();
    if (LookAhead == '"')
	return GetString();
    if (ORDERED('0', LookAhead, '9'))
	return Numberish();	/* Capability or number */
    if (ORDERED('a', LookAhead, 'z') || ORDERED('A', LookAhead, 'Z')
    || LookAhead == '_')
	return KeywOrIdent();
    if (LookAhead & 0x80)
	MyError(-1, -1, "garbage character");
    tok = LookAhead;
    if (tok != MYEOF) LookAhead = f_get();
    return tok;
} /* yylex */


	/*** These routines are called before and after a parse ***/

int
startscan()
{
    extern int LineNo, Nconf;
    LineNo = 1;
    Nconf = 0;
    if (!ReadConfig()) return 0;	/* Read the whole file */
    LookAhead = f_get();		/* Get first char */
    return 1;
} /* startscan */

void
stopscan()
{
    LineNo = -1;
    LookAhead = 0;
} /* stopscan */
