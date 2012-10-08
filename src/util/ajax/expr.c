/*	@(#)expr.c	1.1	91/04/24 17:34:03 */
/* expr(1)  --  author Erik Baalbergen */

/* expr accepts the following grammar:
	expr ::= primary | primary operator expr ;
	primary ::= '(' expr ')' | string ;
  where the priority of the operators is taken care of.
  The resulting value is printed on stdout.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EOI	0
#define OR	1
#define AND	2
#define LT	3
#define LE	4
#define EQ	5
#define NE	6
#define GE	7
#define GT	8
#define PLUS	9
#define MINUS	10
#define TIMES	11
#define DIV	12
#define MOD	13
#define COLON	14
#define LPAREN	15
#define RPAREN	16
#define OPERAND	20

#define MAXPRIO	6

struct op {
	char *op_text;
	short op_num, op_prio;
} ops[] = {
	{"|",	OR,	6},
	{"&",	AND,	5},
	{"<",	LT,	4},
	{"<=",	LE,	4},
	{"=",	EQ,	4},
	{"!=",	NE,	4},
	{">=",	GE,	4},
	{">",	GT,	4},
	{"+",	PLUS,	3},
	{"-",	MINUS,	3},
	{"*",	TIMES,	2},
	{"/",	DIV,	2},
	{"%",	MOD,	2},
	{":",	COLON,	1},
	{"(",	LPAREN,	-1},
	{")",	RPAREN,	-1},
	{0, 0, 0}
};

char *eval(), *expr(), *Salloc(), *long2str(), *expr_colon();
char *prog;
char **ip;
struct op *ip_op;

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char *argv[];
{
	char *res;

	prog = argv[0];
	ip = &argv[1];
	res = expr(lex(*ip), MAXPRIO);
	if (*++ip != 0)
		syntax();
	printf("%s\n", res);
	exit(0);
}

lex(s)
	register char *s;
{
	register struct op *op = ops;

	if (s == 0) {
		ip_op = 0;
		return EOI;
	}
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0) {
			ip_op = op;
			return op->op_num;
		}
		op++;
	}
	ip_op = 0;
	return OPERAND;
}

num(s, pl)
	register char *s;
	long *pl;
{
	register long l = 0, sign = 1;

	if (*s == '\0')
		return 0;
	if (*s == '-') {
		sign = -1;
		s++;
	}
	while (*s >= '0' && *s <= '9')
		l = l * 10 + *s++ - '0';
	if (*s != '\0')
		return 0;
	*pl = sign * l;
	return 1;
}

nonnumeric()
{
	fprintf(stderr, "%s: nonnumeric arguments\n", prog);
	exit(1);
}

syntax()
{
	fprintf(stderr, "%s: syntax error\n", prog);
	exit(1);
}

char *
expr(n, prio)
{
	char *res;

	if (n == EOI)
		syntax();
	if (n == LPAREN) {
		if (prio == 0) {
			res = expr(lex(*++ip), MAXPRIO);
			if (lex(*++ip) != RPAREN)
				syntax();
		}
		else
			res = expr(n, prio - 1);
	}
	else
		if (n == OPERAND) {
			if (prio == 0)
				return *ip;
			res = expr(n, prio - 1);
		}
		else
			syntax();
	while ((n = lex(*++ip)) && ip_op && ip_op->op_prio == prio)
		res = eval(res, n, expr(lex(*++ip), prio - 1));
	ip--;
	return res;
}

char *
eval(l1, op, l2)
	char *l1, *l2;
{
	long v1, v2;
	int n1 = num(l1, &v1), n2 = num(l2, &v2);

	switch (op) {
	case OR:
		if (n1)
			return v1 ? l1 : l2;
		return (*l1 == '\0') ? l2 : l1;
	case AND:
		if (((n1 && v1) || (!n1 && *l1))
		&& ((n2 && v2) || (!n2 && *l2)))
			return l1;
		return "0";
	case LT:
		if (n1 && n2)
			return (v1 < v2) ? "1" : "0";
		return (strcmp(l1, l2) < 0) ? "1" : "0";
	case LE:
		if (n1 && n2)
			return (v1 <= v2) ? "1" : "0";
		return (strcmp(l1, l2) <= 0) ? "1" : "0";
	case EQ:
		if (n1 && n2)
			return (v1 == v2) ? "1" : "0";
		return (strcmp(l1, l2) == 0) ? "1" : "0";
	case NE:
		if (n1 && n2)
			return (v1 != v2) ? "1" : "0";
		return (strcmp(l1, l2) != 0) ? "1" : "0";
	case GE:
		if (n1 && n2)
			return (v1 >= v2) ? "1" : "0";
		return (strcmp(l1, l2) >= 0) ? "1" : "0";
	case GT:
		if (n1 && n2)
			return (v1 > v2) ? "1" : "0";
		return (strcmp(l1, l2) > 0) ? "1" : "0";
	case PLUS:
		if (n1 && n2)
			return long2str(v1 + v2, 10);
		nonnumeric();
	case MINUS:
		if (n1 && n2)
			return long2str(v1 - v2, 10);
		nonnumeric();
	case TIMES:
		if (n1 && n2)
			return long2str(v1 * v2, 10);
		nonnumeric();
	case DIV:
		if (n1 && n2)
			return long2str(v1 / v2, 10);
		nonnumeric();
	case MOD:
		if (n1 && n2)
			return long2str(v1 % v2, 10);
		nonnumeric();
	case COLON:
		return Salloc(expr_colon(l2, l1));
	}
	fatal();
	/*NOTREACHED*/
}

fatal()
{
	fprintf(stderr, "fatal\n");
	exit(1);
}

/* the ':' operator */
char *
expr_colon(re,str)
	char *re;
	char *str;
{
	extern char *strcpy();
	register char *p;
	static char substr[128];
	char *re_comp();

	if ((p = re_comp(re)) != 0) {
		fprintf(stderr, "\t%s: %s\n", p, re);
		exit(1);
	}
	
	/* set default return value in case the match fails */
	strcpy(substr, "0");

	if (re_exec(str)) {
	    /* try to get value of a \( \) pair, if there was one */
	    if (!re_subs("\\1",substr)) {
		/* no, just give the length, if there was a match */
		if (re_subs("&",substr)) {
		    sprintf(substr,"%d", strlen(substr));
		}
	    }
	}
	return substr;
}

char *
Salloc(s)
	register char *s;
{
#define BUF 1024
	static char string_buf[BUF];	/* ouch... */
	static int index = 0;
	int len = strlen(s);
	char *bos = &string_buf[index];
	register char *t = bos;

	if (index + len + 1 > BUF) {
		fprintf(stderr, "out of string space\n");
		exit(1);
	}
	index += (len + 1);
	while (*t++ = *s++) {}
	return bos;
}

/*  em/modules/string/long2str.c  */

/* Integer to String translator
	-> base is a value from [-16,-2] V [2,16]
	-> base < 0: see 'val' as unsigned value
	-> no checks for buffer overflow and illegal parameters
	(1985, EHB)
*/

#define MAXWIDTH 32

char *
long2str(val, base)
	register long val;
	register base;
{
	static char numbuf[MAXWIDTH];
	static char vec[] = "0123456789ABCDEF";
	register char *p = &numbuf[MAXWIDTH];
	int sign = (base > 0);

	*--p = '\0';		/* null-terminate string	*/
	if (val) {
		if (base > 0) {
			if (val < 0L) {
				if ((val = -val) < 0L)
					goto overflow;
			}
			else
				sign = 0;
		}
		else
		if (base < 0) {			/* unsigned */
			base = -base;
			if (val < 0L) {	/* taken from Amoeba src */
				register mod, i;
			overflow:
				mod = 0;
				for (i = 0; i < 8 * sizeof val; i++) {
					mod <<= 1;
					if (val < 0)
						mod++;
					val <<= 1;
					if (mod >= base) {
						mod -= base;
						val++;
					}
				}
				*--p = vec[mod];
			}
		}
		do {
			*--p = vec[(int) (val % base)];
			val /= base;
		} while (val != 0L);
		if (sign)
			*--p = '-';	/* don't forget it !!	*/
	}
	else
		*--p = '0';		/* just a simple 0	*/
	return p;
}
/* end of  em/modules/string/long2str.c  */
/*
 * regex - Regular expression pattern matching
 *         and replacement
 *
 *
 * By:  Ozan S. Yigit (oz)
 *      Dept. of Computer Science
 *      York University
 *
 *
 * These routines are the PUBLIC DOMAIN equivalents 
 * of regex routines as found in 4.nBSD UN*X, with minor
 * extensions.
 *
 * These routines are derived from various implementations
 * found in software tools books, and Conroy's grep. They
 * are NOT derived from licensed/restricted software.
 * For more interesting/academic/complicated implementations,
 * see Henry Spencer's regexp routines, or GNU Emacs pattern
 * matching module.
 *
 * Routines:
 *      re_comp:        compile a regular expression into
 *                      a DFA.
 *
 *			char *re_comp(s)
 *			char *s;
 *
 *      re_exec:        execute the DFA to match a pattern.
 *
 *			int re_exec(s)
 *			char *s;
 *
 *      re_subs:	substitute the matched portions in
 *              	a new string.
 *
 *			int re_subs(src, dst)
 *			char *src;
 *			char *dst;
 *
 *	re_fail:	failure routine for re_exec.
 *
 *			void re_fail(msg, op)
 *			char *msg;
 *			char op;
 *  
 * Regular Expressions:
 *
 *      [1]     char    matches itself, unless it is a special
 *                      character (metachar): . \ [ ] * + ^ $
 *
 *      [2]     .       matches any character.
 *
 *      [3]     \       matches the character following it, except
 *			when followed by a left or right round bracket,
 *			a digit 1 to 9 or a left or right angle bracket. 
 *			(see [7], [8] and [9])
 *			It is used as an escape character for all 
 *			other meta-characters, and itself. When used
 *			in a set ([4]), it is treated as an ordinary
 *			character.
 *
 *      [4]     [set]   matches one of the characters in the set.
 *                      If the first character in the set is "^",
 *                      it matches a character NOT in the set. A
 *                      shorthand S-E is used to specify a set of
 *                      characters S upto E, inclusive. The special
 *                      characters "]" and "-" have no special
 *                      meaning if they appear as the first chars
 *                      in the set.
 *                      examples:        match:
 *
 *                              [a-z]    any lowercase alpha
 *
 *                              [^]-]    any char except ] and -
 *
 *                              [^A-Z]   any char except uppercase
 *                                       alpha
 *
 *                              [a-zA-Z] any alpha
 *
 *      [5]     *       any regular expression form [1] to [4], followed by
 *                      closure char (*) matches zero or more matches of
 *                      that form.
 *
 *      [6]     +       same as [5], except it matches one or more.
 *
 *      [7]             a regular expression in the form [1] to [10], enclosed
 *                      as \(form\) matches what form matches. The enclosure
 *                      creates a set of tags, used for [8] and for
 *                      pattern substution. The tagged forms are numbered
 *			starting from 1.
 *
 *      [8]             a \ followed by a digit 1 to 9 matches whatever a
 *                      previously tagged regular expression ([7]) matched.
 *
 *	[9]	\<	a regular expression starting with a \< construct
 *		\>	and/or ending with a \> construct, restricts the
 *			pattern matching to the beginning of a word, and/or
 *			the end of a word. A word is defined to be a character
 *			string beginning and/or ending with the characters
 *			A-Z a-z 0-9 and _. It must also be preceded and/or
 *			followed by any character outside those mentioned.
 *
 *      [10]            a composite regular expression xy where x and y
 *                      are in the form [1] to [10] matches the longest
 *                      match of x followed by a match for y.
 *
 *      [11]	^	a regular expression starting with a ^ character
 *		$	and/or ending with a $ character, restricts the
 *                      pattern matching to the beginning of the line,
 *                      or the end of line. [anchors] Elsewhere in the
 *			pattern, ^ and $ are treated as ordinary characters.
 *
 *
 * Acknowledgements:
 *
 *	HCR's Hugh Redelmeier has been most helpful in various
 *	stages of development. He convinced me to include BOW
 *	and EOW constructs, originally invented by Rob Pike at
 *	the University of Toronto.
 *
 * References:
 *              Software tools			Kernighan & Plauger
 *              Software tools in Pascal        Kernighan & Plauger
 *              Grep [rsx-11 C dist]            David Conroy
 *		ed - text editor		Un*x Programmer's Manual
 *		Advanced editing on Un*x	B. W. Kernighan
 *		RegExp routines			Henry Spencer
 *
 * Notes:
 *
 *	This implementation uses a bit-set representation for character
 *	classes for speed and compactness. Each character is represented 
 *	by one bit in a 128-bit block. Thus, CCL or NCL always takes a 
 *	constant 16 bytes in the internal dfa, and re_exec does a single
 *	bit comparison to locate the character in the set.
 *
 * Examples:
 *
 *	pattern:	foo*.*
 *	compile:	CHR f CHR o CLO CHR o END CLO ANY END END
 *	matches:	fo foo fooo foobar fobar foxx ...
 *
 *	pattern:	fo[ob]a[rz]	
 *	compile:	CHR f CHR o CCL 2 o b CHR a CCL bitset END
 *	matches:	fobar fooar fobaz fooaz
 *
 *	pattern:	foo\\+
 *	compile:	CHR f CHR o CHR o CHR \ CLO CHR \ END END
 *	matches:	foo\ foo\\ foo\\\  ...
 *
 *	pattern:	\(foo\)[1-3]\1	(same as foo[1-3]foo)
 *	compile:	BOT 1 CHR f CHR o CHR o EOT 1 CCL bitset REF 1 END
 *	matches:	foo1foo foo2foo foo3foo
 *
 *	pattern:	\(fo.*\)-\1
 *	compile:	BOT 1 CHR f CHR o CLO ANY END EOT 1 CHR - REF 1 END
 *	matches:	foo-foo fo-fo fob-fob foobar-foobar ...
 * 
 */

#define MAXDFA  1024
#define MAXTAG  10

#define OKP     1
#define NOP     0

#define CHR     1
#define ANY     2
#define CCL     3
#define NCL     4
#define BOL     5
#define EOL     6
#define BOT     7
#define EOT     8
#define BOW	9
#define EOW	10
#define REF     11
#define CLO     12

#define END     0

/*
 * The following defines are not meant
 * to be changeable. They are for readibility
 * only.
 *
 */
#define MAXCHR	128
#define CHRBIT	8
#define BITBLK	MAXCHR/CHRBIT
#define BLKIND	0170
#define BITIND	07

#define ASCIIB	0177

typedef /*unsigned*/ char CHAR;

static int  tagstk[MAXTAG];             /* subpat tag stack..*/
static CHAR dfa[MAXDFA];		/* automaton..       */
static int  sta = NOP;               	/* status of lastpat */

static CHAR bittab[BITBLK];		/* bit table for CCL */

static void
chset(c) register CHAR c; { bittab[((c)&BLKIND)>>3] |= 1<<((c)&BITIND); }

#define badpat(x)	return(*dfa = END, x)
#define store(x)	*mp++ = x
 
char *     
re_comp(pat)
char *pat;
{
	register char *p;               /* pattern pointer   */
	register CHAR *mp=dfa;          /* dfa pointer       */
	register CHAR *lp;              /* saved pointer..   */
	register CHAR *sp=dfa;          /* another one..     */

	register int tagi = 0;          /* tag stack index   */
	register int tagc = 1;          /* actual tag count  */

	register int n;
	int c1, c2;
		
	if (!pat || !*pat)
		if (sta)
			return(0);
		else
			badpat("No previous regular expression");
	sta = NOP;

	for (p = pat; *p; p++) {
		lp = mp;
		switch(*p) {

		case '.':               /* match any char..  */
			store(ANY);
			break;

		case '^':               /* match beginning.. */
			if (p == pat)
				store(BOL);
			else {
				store(CHR);
				store(*p);
			}
			break;

		case '$':               /* match endofline.. */
			if (!*(p+1))
				store(EOL);
			else {
				store(CHR);
				store(*p);
			}
			break;

		case '[':               /* match char class..*/

			if (*++p == '^') {
				store(NCL);
				p++;
			}
			else
				store(CCL);

			if (*p == '-')		/* real dash */
				chset(*p++);
			if (*p == ']')		/* real brac */
				chset(*p++);
			while (*p && *p != ']') {
				if (*p == '-' && *(p+1) && *(p+1) != ']') {
					p++;
					c1 = *(p-2) + 1;
					c2 = *p++;
					while (c1 <= c2)
						chset(c1++);
				}
#ifdef EXTEND
				else if (*p == '\\' && *(p+1)) {
					p++;
					chset(*p++);
				}
#endif
				else
					chset(*p++);
			}
			if (!*p)
				badpat("Missing ]");

			for (n = 0; n < BITBLK; bittab[n++] = (char) 0)
				store(bittab[n]);
	
			break;

		case '*':               /* match 0 or more.. */
		case '+':               /* match 1 or more.. */
			if (p == pat)
				badpat("Empty closure");
			lp = sp;                /* previous opcode */
			if (*lp == CLO)         /* equivalence..   */
				break;
			switch(*lp) {

			case BOL:
			case BOT:
			case EOT:
			case BOW:
			case EOW:
			case REF:
				badpat("Illegal closure");
			default:
				break;
			}

			if (*p == '+')
				for (sp = mp; lp < sp; lp++)
					store(*lp);

			store(END);
			store(END);
			sp = mp;
			while (--mp > lp)
				*mp = mp[-1];
			store(CLO);
			mp = sp;
			break;

		case '\\':              /* tags, backrefs .. */
			switch(*++p) {

			case '(':
				if (tagc < MAXTAG) {
					tagstk[++tagi] = tagc;
					store(BOT);
					store(tagc++);
				}
				else
					badpat("Too many \\(\\) pairs");
				break;
			case ')':
				if (*sp == BOT)
					badpat("Null pattern inside \\(\\)");
				if (tagi > 0) {
					store(EOT);
					store(tagstk[tagi--]);
				}
				else
					badpat("Unmatched \\)");
				break;
			case '<':
				store(BOW);
				break;
			case '>':
				if (*sp == BOW)
					badpat("Null pattern inside \\<\\>");
				store(EOW);
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				n = *p-'0';
				if (tagi > 0 && tagstk[tagi] == n)
					badpat("Cyclical reference");
				if (tagc > n) {
					store(REF);
					store(n);
				}
				else
					badpat("Undetermined reference");
				break;
#ifdef EXTEND
			case 'b':
				store(CHR);
				store('\b');
				break;
			case 'n':
				store(CHR);
				store('\n');
				break;
			case 'f':
				store(CHR);
				store('\f');
				break;
			case 'r':
				store(CHR);
				store('\r');
				break;
			case 't':
				store(CHR);
				store('\t');
				break;
#endif
			default:
				store(CHR);
				store(*p);
			}
			break;

		default :               /* an ordinary char  */
			store(CHR);
			store(*p);
			break;
		}
		sp = lp;
	}
	if (tagi > 0)
		badpat("Unmatched \\(");
	store(END);
	sta = OKP;
	return(0);
}


static char *bol;
/*static*/ char *bopat[MAXTAG];
/*static*/ char *eopat[MAXTAG];
static char *pmatch();

/*
 * re_exec:
 * 	execute dfa to find a match.
 *
 *	special cases: (dfa[0])	
 *		BOL
 *			Match only once, starting from the
 *			beginning.
 *		CHR
 *			First locate the character without
 *			calling pmatch, and if found, call
 *			pmatch for the remaining string.
 *		END
 *			re_comp failed, poor luser did not
 *			check for it. Fail fast.
 *
 *	If a match is found, bopat[0] and eopat[0] are set
 *	to the beginning and the end of the matched fragment,
 *	respectively.
 *
 */

int
re_exec(lp)
register char *lp;
{
	register char c;
	register char *ep = 0;
	register CHAR *ap = dfa;

	bol = lp;

	bopat[0] = 0;
	bopat[1] = 0;
	bopat[2] = 0;
	bopat[3] = 0;
	bopat[4] = 0;
	bopat[5] = 0;
	bopat[6] = 0;
	bopat[7] = 0;
	bopat[8] = 0;
	bopat[9] = 0;

	switch(*ap) {

	case BOL:			/* anchored: match from BOL only */
		ep = pmatch(lp,ap);
		break;
	case CHR:			/* ordinary char: locate it fast */
		c = *(ap+1);
		while (*lp && *lp != c)
			lp++;
		if (!*lp)		/* if EOS, fail, else fall thru. */
			return(0);
	default:			/* regular matching all the way. */
		while (*lp) {
			if ((ep = pmatch(lp,ap)))
				break;
			lp++;
		}
		break;
	case END:			/* munged automaton. fail always */
		return(0);
	}
	if (!ep)
		return(0);

	bopat[0] = lp;
	eopat[0] = ep;
	return(1);
}

/* 
 * pmatch: 
 *	internal routine for the hard part
 *
 * 	This code is mostly snarfed from an early
 * 	grep written by David Conroy. The backref and
 * 	tag stuff, and various other mods are by oZ.
 *
 *	special cases: (dfa[n], dfa[n+1])
 *		CLO ANY
 *			We KNOW ".*" will match ANYTHING
 *			upto the end of line. Thus, go to
 *			the end of line straight, without
 *			calling pmatch recursively. As in
 *			the other closure cases, the remaining
 *			pattern must be matched by moving
 *			backwards on the string recursively,
 *			to find a match for xy (x is ".*" and 
 *			y is the remaining pattern) where
 *			the match satisfies the LONGEST match
 *			for x followed by a match for y.
 *		CLO CHR
 *			We can again scan the string forward
 *			for the single char without recursion, 
 *			and at the point of failure, we execute 
 *			the remaining dfa recursively, as
 *			described above.
 *
 *	At the end of a successful match, bopat[n] and eopat[n]
 *	are set to the beginning and end of subpatterns matched
 *	by tagged expressions (n = 1 to 9).	
 *
 */

extern void re_fail();

/*
 * character classification table for word boundary
 * operators BOW and EOW. the reason for not using 
 * ctype macros is that we can let the user add into 
 * our own table. see re_modw. This table is not in
 * the bitset form, since we may wish to extend it
 * in the future for other character classifications. 
 *
 *	TRUE for 0-9 A-Z a-z _
 */
static char chrtyp[MAXCHR] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 0, 0, 0, 0, 0
	};

#define inascii(x)	(0177&(x))
#define iswordc(x) 	chrtyp[inascii(x)]
#define isinset(x,y) 	((x)[((y)&BLKIND)>>3] & (1<<((y)&BITIND)))

/*
 * skip values for CLO XXX to skip past the closure
 *
 */

#define ANYSKIP	2 	/* CLO ANY END ...	   */
#define CHRSKIP	3	/* CLO CHR chr END ...	   */
#define CCLSKIP 18	/* CLO CCL 16bytes END ... */

static char *
pmatch(lp, ap)
register char *lp;
register CHAR *ap;
{
	register char *e;		/* extra pointer for CLO */
	register char *bp;		/* beginning of subpat.. */
	register char *ep;		/* ending of subpat..	 */
	register int op, c, n;
	char *are;			/* to save the line ptr. */

	while ((op = *ap++) != END)
		switch(op) {

		case CHR:
			if (*lp++ != *ap++)
				return(0);
			break;
		case ANY:
			if (!*lp++)
				return(0);
			break;
		case CCL:
			c = *lp++;
			if (!isinset(ap,c))
				return(0);
			ap += BITBLK;
			break;
		case NCL:
			c = *lp++;
			if (isinset(ap,c))
				return(0);
			ap += BITBLK;
			break;
		case BOL:
			if (lp != bol)
				return(0);
			break;
		case EOL:
			if (*lp)
				return(0);
			break;
		case BOT:
			bopat[*ap++] = lp;
			break;
		case EOT:
			eopat[*ap++] = lp;
			break;
 		case BOW:
			if (!(lp!=bol && iswordc(lp[-1])) && iswordc(*lp))
				break;
			return(0);
		case EOW:
			if ((lp!=bol && iswordc(lp[-1])) && !iswordc(*lp))
				break;
			return(0);
		case REF:
			n = *ap++;
			bp = bopat[n];
			ep = eopat[n];
			while (bp < ep)
				if (*bp++ != *lp++)
					return(0);
			break;
		case CLO:
			are = lp;
			switch(*ap) {

			case ANY:
				while (*lp)
					lp++;
				n = ANYSKIP;
				break;
			case CHR:
				c = *(ap+1);
				while (*lp && c == *lp)
					lp++;
				n = CHRSKIP;
				break;
			case CCL:
			case NCL:
				while (*lp && (e = pmatch(lp, ap)))
					lp = e;
				n = CCLSKIP;
				break;
			default:
				re_fail("closure: bad dfa.", *ap);
				return(0);
			}

			ap += n;

			while (lp >= are) {
				if (e = pmatch(lp, ap))
					return(e);
				--lp;
			}
			return(0);
		default:
			re_fail("re_exec: bad dfa.", op);
			return(0);
		}
	return(lp);
}

/*
 * re_subs:
 *	substitute the matched portions of the src in
 *	dst.
 *
 *	&	substitute the entire matched pattern.
 *
 *	\digit	substitute a subpattern, with the given
 *		tag number. Tags are numbered from 1 to
 *		9. If the particular tagged subpattern
 *		does not exist, null is substituted.
 *
 */
int
re_subs(src, dst)
register char *src;
register char *dst;
{
	register char c;
	register int  pin;
	register char *bp;
	register char *ep;

	if (!*src || !bopat[0])
		return(0);

	while (c = *src++) {
		switch(c) {

		case '&':
			pin = 0;
			break;

		case '\\':
			c = *src++;
			if (c >= '0' && c <= '9') {
				pin = c - '0';
				break;
			}
			
		default:
			*dst++ = c;
			continue;
		}

		if ((bp = bopat[pin]) && (ep = eopat[pin])) {
			while (*bp && bp < ep)
				*dst++ = *bp++;
			if (bp < ep)
				return(0);
		}
	}
	*dst = (char) 0;
	return(1);
}

/* 
 * re_fail:
 *	internal error handler for re_exec.
 *
 *	should probably do something like a
 *	longjump to recover gracefully.
 */ 

void	
re_fail(s, c)
char *s;
char c;
{
	fprintf(stderr, "%s [opcode %o]\n", s, c);
	exit(1);
}
