/*	@(#)cmd.c	1.4	96/02/27 12:43:15 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"
#include "gen.h"

/*
 *	Define this to set the timeout of the generated clients to 10 seconds
#define DEF_TIMEOUT	"10000L"
 */

/*
 *	Print a routine that prints a message and dies,
 *	in several languages.
 *	'%' signs must be escaped by an extra '%', to
 *	prevent mypf() from popping an absent argument.
 */
static char
    *ansi_fail[] = {			/* Ansi version		*/
	"%nstatic void",
	"%n_fail(char *s, char *name, errstat err)",
	"%n{%>",
	"%nif (err == STD_OK) fprintf(stderr, \"%%s: %%s%%s\\n\", _progname, s, name);",
	"%nelse fprintf(stderr, \"%%s: %%s%%s (%%s)\\n\", _progname, s, name, err_why(err));",
	"%nexit(1);",
	"%<%n} /* _fail */\n",
	(char *) NULL
    },

    *trad_fail[] = {			/* Traditional version	*/
	"%nstatic void",
	"%n_fail(s, name, err)%>%nchar *s, *name;%nerrstat err;%<",
	"%n{%>",
	"%nif (err == STD_OK) fprintf(stderr, \"%%s: %%s%%s\\n\", _progname, s, name);",
	"%nelse fprintf(stderr, \"%%s: %%s%%s (%%s)\\n\", _progname, s, name, err_why(err));",
	"%nexit(1);",
	"%<%n} /* _fail */\n",
	(char *) NULL
    };

/*
 *	The get_int routine:
 */
static char
    *ansi_get_int[] = {			/* ANSI version		*/
	"%nstatic int",
	"%n_get_int(char *num)",
	"%n{%>",
	"%nchar *s;",
	"%nint n = 0, neg = 0;",
	"%nswitch (*(s = num)) {",
	"%ncase '-':%>",
	"%nneg = 1;",
	"%n/*FALLTHROUGH*/%<",
	"%ncase '+':%>",
	"%n++s;",
	"%<%n}",	/* Terminate switch		*/
	"%ndo {%>",
	"%nif (*s < '0' || '9' < *s)%>",
	"%n_fail(num, \" is not an integer\", STD_OK);%<",
	"%nn = 10 * n + *s++ - '0';",
	"%nif (n < 0) _fail(\"integer overflow in \", num, STD_OK);%<",
	"%n} while (*s != '\\0');",
	"%nreturn neg ? -n : n;%<%n} /* _get_int */\n",
	(char *) NULL
    },

    *trad_get_int[] = {			/* Traditional version	*/
	"%nstatic int",
	"%n_get_int(num)%>",
	"%nchar *num;%<",
	"%n{%>",
	"%nchar *s;",
	"%nint n = 0, neg = 0;",
	"%nswitch (*(s = num)) {",
	"%ncase '-':%>",
	"%nneg = 1;",
	"%n/*FALLTHROUGH*/%<",
	"%ncase '+':%>",
	"%n++s;",
	"%<%n}",	/* Terminate switch		*/
	"%ndo {%>",
	"%nif (*s < '0' || '9' < *s)%>",
	"%n_fail(num, \" is not an integer\", STD_OK);%<",
	"%nn = 10 * n + *s++ - '0';",
	"%nif (n < 0) _fail(\"integer overflow in \", num, STD_OK);%<",
	"%n} while (*s != '\\0');",
	"%nreturn neg ? -n : n;%<%n} /* _get_int */\n",
	(char *) NULL
    },

    /*
     *	Same for chars:
     */
    *ansi_get_char[] = {
	"%nstatic int",
	"%n_get_char(char *s)",
	"%n{%>",
	"%nif (s == NULL || s[1] != '\\0') {%>",
	"%nfprintf(stderr, \"%%s: should be a single char\\n\", s);",
	"%nexit(2);",
	"%<%n}",
	"%nreturn s[0] & 0xff;%<%n} /* _get_char */\n",
	(char *) NULL
    },

    *trad_get_char[] = {
	"%nstatic int",
	"%n_get_char(s)%>",
	"%nchar *s;%<",
	"%n{%>",
	"%nif (s == NULL || s[1] != '\\0') {%>",
	"%nfprintf(stderr, \"%%s: should be a single char\\n\", s);",
	"%nexit(2);",
	"%<%n}",
	"%nreturn s[0] & 0xff;%<%n} /* _get_char */\n",
	(char *) NULL
    };

/*
 *	This is to keep track of which {str|ord}_<enum-tag>
 *	functions we still have to generate.
 */
static int n_eseen;
static struct {
	struct typdesc *en_type; /* To which type this one applies	 */
	int en_dir;	/* For which directions we need conversion routines */
} enums[200];

static void
MustDeclEnum(t, dir)
    struct typdesc *t;
    int dir;	/* Direction: request/reply */
{
    int i;
    /* Did we generate this one yet?	*/
    for (i = 0; i < n_eseen; ++i) {
	if (t == enums[i].en_type) {	/* Done yet	*/
		enums[i].en_dir |= dir;	/* Add flags	*/
		return;
	}
    }
    if (n_eseen >= sizeof(enums)/sizeof(enums[0])) {
	mypf(ERR_HANDLE, "%r implementation limit; too many enum types\n");
	return;
    }
    enums[n_eseen].en_dir = dir;
    enums[n_eseen++].en_type = t;
} /* MustDeclEnum */

/*
 *	Declare all support for enumerated types:
 *	str_<tag>, ord_<tag> and optionally tab_<tag>.
 */
static void
DeclEnumSupport(h)
    handle h;
{
    int i;
    for (i = n_eseen; i--; ) switch (enums[i].en_dir) {
	case AT_REQUEST:
	    ConvEnum(h, enums[i].en_type, No, Yes);
	    break;
	case AT_REPLY:
	    ConvEnum(h, enums[i].en_type, Yes, No);
	    break;
	case AT_REQUEST|AT_REPLY:
	    ConvEnum(h, enums[i].en_type, Yes, Yes);
	    break;
	default:
	    assert(No);
    }
} /* DeclEnumSupport */

/*
 *	Function to be called by FindOptrs().
 *	Passed by CommandGen() - generates one cmd_<op>.c file.
 */
void
CommandGenFunc(name, op)
    char *name;
    struct optr *op;
{
    char fname[256];
    handle fp;
    if (op == NULL) {	/* Means that FindOptrs couldn't find it	*/
	mypf(ERR_HANDLE, "%r Unknown operator %s\n", name);
    } else if (op->op_flags & OP_STARRED) {	/* '*' as first arg	*/
	/* Open an output file:	*/
	sprintf(fname, "cmd_%s.c", op->op_name);
	fp = OpenFile(fname);
	CommandOptr(fp, op);
	CloseFile(fp);
    } else {		/* Probably an operator that wraps another function */
	/* I could build a program that
	 * calls the function anyway,
	 * but I got better things to do, so:
	 */
	mypf(ERR_HANDLE, "%r '*' argument of %s missing\n", op->op_name);
    }
} /* CommandGenFunc */

/*
 *	CommandGen() : the code generator for C-programs.
 *	Actually a wrapper around CommandGenFunc()
 */
/*ARGSUSED*/
void
CommandGen(args)
    struct gen *args;
{
    struct name *nm;
    if (args != NULL)
	FindOptrs(args, 0, CommandGenFunc);
    /* If not, generate all of the clientstubs defined within the class	*/
    else for (nm = ThisClass->cl_scope; nm != NULL; nm = nm->nm_next) {
	if (nm->nm_kind == CALL)
	    CommandGenFunc(nm->nm_name, nm->nm_optr);
	if (ErrCnt) break;	/* Something went wrong	*/
    }
} /* CommandGen */

/*
 *	Print out a routine to fetch an integer from the commandline
 *	Forget every difference between signed, unsigned and all other
 *	brands of integers, because Siebren doesn't feel like that today.
 */
static void
DeclGetInt(fp)
    handle fp;
{
    char **p;
    /* Find right routine:	*/
    switch (lang) {
    case L_ANSI:
	p = &ansi_get_int[0];
	break;
    case L_TRADITIONAL:
	p = &trad_get_int[0];
	break;
    default:
	assert(No);
    }
    /* Print it:		*/
    while (*p != NULL) mypf(fp, *p++);
} /* DeclGetInt */

/*
 *	Print out a routine to fetch a character from the commandline.
 */
static void
DeclGetChar(fp)
    handle fp;
{
    char **p;
    /* Find right routine:	*/
    switch (lang) {
    case L_ANSI:
	p = &ansi_get_char[0];
	break;
    case L_TRADITIONAL:
	p = &trad_get_char[0];
	break;
    default:
	assert(No);
    }
    /* Print it:		*/
    while (*p != NULL) mypf(fp, *p++);
} /* DeclGetChar */

/*
 *	Check _error, print the errorstring and exit if != STD_OK
 */
static void
CheckError(fp, s, n)
    handle fp;
    char *s;
    int n;	/* Which commandline argument we refer to */
{
    mypf(fp, "%nif (_error != STD_OK)%>");
    if (n < 0) mypf(fp, "%n_fail(\"%s\", \"\", _error);%<", s);
    else mypf(fp, "%n_fail(\"%s\", argv[%d], _error);%<", s, n);
} /* CheckError */

/*
 *	Write a program for an operator.
 *	This very much like a normal clientstubs.
 *	We only accept capabilities, enums, integers and strings.
 *	For strings and integers, things are easy:
 *	The in-parameters come from the commandline,
 *	out-parameters are printed using stdio.
 *	For capabilities this is not welldone.
 *	So, in-parameters are name_lookup'ed, and out ones
 *	are name_appended. If there is any old capability
 *	there, we simply name_delete it. Maybe we could make
 *	the various other possible behaviours an option?
 */
void
CommandOptr(fp, op)
    handle fp;
    struct optr *op;
{
    struct itlist *arg;
    char **p;
    Bool decled_getint = No, decled_getchar = No;
    int cnt;
    int outcap = 0;	/* How many capabilities are out-only	*/

    n_eseen = 0;	/* Haven't declared any enum-gets yet	*/

    mypf(fp, "/*\n *\tClient program for %s (class %s) by ail\n */\n\n",
	op->op_name, ThisClass->cl_name);
    mypf(fp, "#include <amoeba.h>\n");
    mypf(fp, "#include <stdio.h>\n");
    mypf(fp, "#include <stderr.h>\n");
    mypf(fp, "#include <module/name.h>\n");
    mypf(fp, "#include \"%s.h\"\n", ThisClass->cl_name);
    mypf(fp, "\nstatic char *_progname;\n");

    /* Print the _fail routine	*/
    switch (lang) {
    case L_TRADITIONAL:
	for (p = &trad_fail[0]; *p != NULL; ++p)
	    mypf(fp, *p);
	break;
    case L_ANSI:
	for (p = &ansi_fail[0]; *p != NULL; ++p)
	    mypf(fp, *p);
	break;
    }

    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	struct typdesc *type;
	for (type = arg->il_type;
	    type->td_kind == tk_tdef && type != cap_typ;
	    type = type->td_prim)
		;
	assert (type->td_kind != tk_ref && type->td_kind != tk_copy);

	/*
	 *	Types we grok: capabilities, integers,
	 *	chars, enums and strings.
	 */
	if (type == cap_typ) {
	    if ((arg->il_bits & AT_IO) == AT_O) ++outcap;
	} else switch (type->td_kind) {
	case tk_int:
	    if ((arg->il_bits & AT_REQUEST) && !decled_getint) {
		DeclGetInt(fp);
		decled_getint = Yes;
	    }
	    break;
	case tk_char:
	    if ((arg->il_bits & AT_REQUEST) && !decled_getchar) {
		DeclGetChar(fp);
		decled_getchar = Yes;
	    }
	    break;
	case tk_enum:
	    if (type->td_name == NULL) {
		mypf(ERR_HANDLE, "%r need a tagged enum type for %s\n",
		    arg->il_name);
		return;
	    }
	    if (!type->td_defd) {
		mypf(ERR_HANDLE, "%r enum %s not complete\n", type->td_name);
	    }
	    MustDeclEnum(type, arg->il_bits & (AT_REQUEST | AT_REPLY));
	    break;
	case tk_arr:
	    type = SameType(type->td_prim);
	    if (type->td_kind == tk_char)
		break;
	    /* Else: FALLTHROUGH	*/
	default:
	    mypf(ERR_HANDLE, "%r Cannot cope with ");
	    TypeSpell(ERR_HANDLE, arg->il_type->td_styp, No);
	    SoftSpell(ERR_HANDLE, arg->il_type, arg->il_name);
	    mypf(ERR_HANDLE, "\n");
	}
    }
    DeclEnumSupport(fp);	/* Emit ord_*, str_* and tab_* functions */

    /* Write everything from "main" up to main's first brace	*/
    switch (lang) {
    case L_TRADITIONAL:
	mypf(fp, "%nmain(argc, argv)%>%nint argc;%nchar *argv[];\n{");	/* } */
	mypf(fp, "%nint _error;");
	break;
    case L_ANSI:
	mypf(fp, "%nmain(int argc, char *argv[])\n{%>");	/* } */
	mypf(fp, "%nint _error;");
	break;
    default:
	mypf(ERR_HANDLE, "%r unknown language\n");
    }
    if (ErrCnt) return;

    /* Declare some variables	*/
    mypf(fp, "%ncapability _cap;");
    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	mypf(fp, "%n");
	TypeSpell(fp, arg->il_type->td_styp, No);
	SoftSpell(fp, arg->il_type, arg->il_name);
	mypf(fp, ";");
    }

#ifdef DEF_TIMEOUT
    /* Set timeout: */
    mypf(fp, "\n%n(void) timeout(%s);", DEF_TIMEOUT);
#endif

    /* Check commandline options:	*/
    mypf(fp, "\n%n/* Parse commandline */");
    mypf(fp, "%n_progname = argv[0];");
    /* Check number of arguments:	*/
    mypf(fp, "%nif (argc != %d) {%>", op->op_ih + op->op_ib + outcap + 2);
    mypf(fp, "%nfprintf(stderr, \"usage: %%s capability");
    /* mypf(fp, "%n_fail(\"usage: capability"); */
    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	/* out-capabilities must be on the commandline as well	*/
	if ((arg->il_bits & AT_REQUEST) || arg->il_type == cap_typ)
	    mypf(fp, " %s", arg->il_name);
    }
    mypf(fp, "\\n\", _progname);");
    mypf(fp, "%nexit(2);%<%n}");

    /* Find the capability	*/
    mypf(fp, "%n_error = name_lookup(argv[1], &_cap);");
    CheckError(fp, "cannot lookup ", 1);

    /* Get the other variables:	*/
    cnt = 1;	/* Skip progname and capability name	*/
    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	if (arg->il_bits & AT_REQUEST) {
	    struct typdesc *type;
	    for (type = arg->il_type;
		type->td_kind == tk_tdef && type != cap_typ;
		type = type->td_prim)
		    ;
	    if (type == cap_typ) {
		mypf(fp, "%n_error = name_lookup(argv[%d], &%s);",
		    ++cnt, arg->il_name);
		CheckError(fp, "cannot lookup ", cnt);
	    } else switch (type->td_kind) {
	    case tk_int:
		mypf(fp, "%n%s = _get_int(argv[%d]);", arg->il_name, ++cnt);
		break;
	    case tk_char:
		mypf(fp, "%n%s = _get_char(argv[%d]);", arg->il_name, ++cnt);
		break;
	    case tk_enum:
		mypf(fp, "%n%s = ord_%s(argv[%d]);",
		    arg->il_name, type->td_name, ++cnt);
#ifdef NO_ENUM_COMPARISON
		switch (lang) {
		case L_ANSI:
		    mypf(fp,
			"%nif (%s < %s) _fail(\"Wrong value for %s\", STD_OK);",
			arg->il_name, arg->il_type->td_meml->il_name,
			arg->il_name);
		    break;
		case L_TRADITIONAL:
		    mypf(fp, "%nif ((int) %s < (int) %s) /* Cast pcc */%>",
			arg->il_name, arg->il_type->td_meml->il_name);
		    mypf(fp, "%n_fail(\"Wrong value for %s\", STD_OK);%<",
			arg->il_name);
		    break;
		default:
		    assert(No);
		}
#else
		mypf(fp, "%nif (%s < %s) _fail(\"Wrong value for %s\", STD_OK);",
		    arg->il_name, arg->il_type->td_meml->il_name, arg->il_name);
#endif
		break;
	    case tk_arr:
		mypf(fp, "%nstrcpy(%s, argv[%d]);", arg->il_name, ++cnt);
		break;
	    default:
		mypf(ERR_HANDLE, "%r BUG: td_kind = %d (%s)\n",
				type->td_kind, TokenToString(type->td_kind));
		assert(No);
	    }
	} else if (arg->il_type == cap_typ) ++cnt;	/* Skip argument */
    }

    /* Call the clientstub */
    mypf(fp, "\n%n/* Do it: */");
    mypf(fp, "%n_error = %s(&_cap", op->op_name);
    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	mypf(fp, ", ");
	if ((arg->il_bits & AT_VAR) && arg->il_type->td_kind != tk_arr) {
	    /* Arrays are so "funny" in C	*/
	    mypf(fp, "&");
	}
	mypf(fp, arg->il_name);
    }
    mypf(fp, ");");
    CheckError(fp, "operation failed", -1);

    cnt = 1;
    mypf(fp, "\n%n/* Process results: */");
    /* Print the output variables:	*/
    for (arg = op->op_order; arg != NULL; arg = arg->il_next) {
	if (arg->il_bits & AT_REPLY) {
	    struct typdesc *type;
	    for (type = arg->il_type;
		type->td_kind == tk_tdef && type != cap_typ;
		type = type->td_prim)
		    ;
	    if (type == cap_typ) {
		++cnt;
		/* Maybe you want to do a std_destroy here? Too bad.	*/
		mypf(fp, "%n(void) name_delete(argv[%d]);", cnt);
		/* Ignore errors here; it is not ours, we can't handle it */
		mypf(fp, "%n_error = name_append(argv[%d], &%s);",
		    cnt, arg->il_name);
		CheckError(fp, "cannot store ", cnt);
	    } else switch(type->td_kind) {
	    case tk_int:
		mypf(fp, "%nprintf(\"%s=%%d\\n\", %s);",
		    arg->il_name, arg->il_name);
		break;
	    case tk_char:
		mypf(fp, "%nprintf(\"%s=%%c\\n\", %s);",
		    arg->il_name, arg->il_name);
		break;
	    case tk_arr:
		assert(type->td_prim->td_kind == tk_char);
		mypf(fp, "%nprintf(\"%s=%%s\\n\", %s);",
		    arg->il_name, arg->il_name);
		break;
	    case tk_enum:
		mypf(fp, "%nprintf(\"%s=%%s\\n\", str_%s(%s));",
		    arg->il_name, arg->il_type->td_name, arg->il_name);
		break;
	    default:
		assert(No);
	    }
	} else if (arg->il_bits & AT_REQUEST) ++cnt;	/* Count in-arg	*/
    }

    /* { */
    mypf(fp, "%nexit(0);%n/*NOTREACHED*/%<%n} /* main of %s */\n", op->op_name);
} /* CommandOptr */
